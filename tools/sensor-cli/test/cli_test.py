"""Bounded Mock and PTY integration checks; never opens physical hardware."""
import argparse
import json
import os
from pathlib import Path
import pty
import re
import select
import signal
import struct
import subprocess
import tempfile
import termios
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('binary')
parser.add_argument('--yesense', action='store_true')
args = parser.parse_args()
binary = str(Path(args.binary).resolve())


def run(options, commands, ok=True):
    command = [binary, 'imu'] + options
    for item in commands:
        command += ['--command', item]
    result = subprocess.run(command, capture_output=True, text=True, timeout=10)
    assert (result.returncode == 0) == ok, (result.args, result.stdout, result.stderr)
    return result.stdout, result.stderr


def samples(output):
    return [json.loads(line) for line in output.splitlines() if line.startswith('{')]


with tempfile.TemporaryDirectory(prefix='sensor-cli-') as directory:
    root = Path(directory)
    recording = root / 'samples with spaces.jsonl'
    output, _ = run(['--driver', 'mock', '--rate', '200'],
                    ['sample', 'stats --seconds 0.3', f'record --file "{recording}" --seconds 0.3',
                     'close', 'status', 'open', 'sample', 'quit'])
    data = samples(output)
    assert len(data) == 2 and data[0]['clock_domain'] != data[1]['clock_domain'], output
    assert data[0]['acceleration_valid'] and not data[0]['orientation_valid'], data
    assert data[0]['magnetic_field_tesla'] is None, data
    stored = [json.loads(line) for line in recording.read_text().splitlines()]
    assert len(stored) > 25, len(stored)
    assert all(b['sequence'] == a['sequence'] + 1 for a, b in zip(stored, stored[1:])), stored
    assert 'running=false' in output and 'DRIVER_DIAGNOSTICS supported=false' in output, output
    assert 100 < float(re.search(r'delivered_hz=([\d.]+)', output)[1]) < 250, output
    configured, _ = run(['--driver','mock'], ['capabilities','device-config','set rate 200',
        'set fields accel,temperature','sample','calibrate static-gyro'])
    value = samples(configured)[0]
    assert value['acceleration_valid'] and not value['angular_velocity_valid'], value
    assert value['raw_quaternion_wxyz'] is None and 'state=COMPLETED' in configured, configured
    run(['--driver','mock'], ['set rate 100 flash'], ok=False)
    before = recording.read_bytes()
    run(['--driver', 'mock'], [f'record --file "{recording}" --seconds 0.1'], ok=False)
    assert recording.read_bytes() == before
    run(['--driver', 'mock', '--closed'], ['sample --timeout 0.1'], ok=False)
    run(['--driver', 'missing', '--closed'], ['status'], ok=False)
    run(['--driver', 'mock'], ['stream --hz nan --seconds 0.1'], ok=False)
    run(['--driver', 'mock', '--baud', '921600'], ['status'], ok=False)
    run(['--driver', 'yesense', '--device', '/dev/not-opened', '--rate', '100'], ['status'], ok=False)

    config = root / 'device.yaml'
    document = {'schema_version': 1, 'robot_id': 'DEMO_ROBOT', 'devices': {
        'imu0': {'type': 'imu', 'driver': 'mock', 'mode': 'mock', 'frame_id': 'line\n"frame', 'publish_rate_hz': 100}}}
    config.write_text(json.dumps(document))
    output, _ = run(['--config', str(config), '--sensor', 'imu0'], ['sample'])
    assert samples(output)[0]['frame_id'] == 'line\n"frame', output
    document['devices']['imu0']['enabled'] = False
    config.write_text(json.dumps(document))
    run(['--config', str(config), '--sensor', 'imu0'], ['sample'], ok=False)
    config.write_text('schema_version: 1\nschema_version: 1\n')
    run(['--config', str(config), '--sensor', 'imu0'], ['sample'], ok=False)

    # Remain at the prompt: reception continues without sample/stream commands.
    process = subprocess.Popen([binary, 'imu', '--driver', 'mock', '--rate', '200'],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        while 'SENSOR_CLI_READY' not in process.stdout.readline():
            assert process.poll() is None
        time.sleep(.3)
        process.stdin.write('status\nstream --hz 5\n'); process.stdin.flush()
        time.sleep(.3)
        process.send_signal(signal.SIGINT)
        time.sleep(.1)
        output, error = process.communicate('status\nquit\n', timeout=3)
        assert process.returncode == 0, (output, error)
        counts = [int(value) for value in re.findall(r'STATUS running=true samples=(\d+)', output)]
        assert len(counts) == 2 and counts[0] > 20 and counts[1] > counts[0], output
    finally:
        if process.poll() is None: process.kill(); process.wait()

    if args.yesense:
        master, slave = pty.openpty()
        original = termios.tcgetattr(slave)
        original[3] &= ~(termios.ECHO | termios.ECHONL)
        termios.tcsetattr(slave, termios.TCSANOW, original)
        stop = threading.Event()
        unexpected = []
        def feed():
            tid = 1
            # Give the CLI time to configure raw mode, then continuously feed.
            time.sleep(.15)
            while not stop.is_set():
                payload = b'\x10\x0c' + struct.pack('<iii', 1000000, -2000000, 9000000)
                payload += b'\x20\x0c' + struct.pack('<iii', 90000000, 0, 0)
                payload += b'\x41\x10' + struct.pack('<iiii', 500000, -500000, 500000, -500000)
                body = struct.pack('<HB', tid, len(payload)) + payload
                ck1 = ck2 = 0
                for value in body:
                    ck1 = (ck1 + value) % 256; ck2 = (ck2 + ck1) % 256
                packet = b'YS' + body + bytes([ck1, ck2])
                os.write(master, packet[:3]); os.write(master, packet[3:])
                if select.select([master], [], [], 0)[0]: unexpected.append(os.read(master, 4096))
                tid = tid % 60000 + 1
                stop.wait(.0025)
        thread = threading.Thread(target=feed, daemon=True)
        try:
            thread.start()
            output, _ = run(['--driver', 'yesense', '--device', os.ttyname(slave), '--baud', '921600'],
                            ['sample', 'stats --seconds 0.2', 'diagnostics', 'close', 'quit'])
            value = samples(output)[0]
            assert value['acceleration_mps2'] == [1, -2, 9], value
            assert abs(value['angular_velocity_radps'][0] - 1.57079632679) < 1e-9, value
            assert not value['orientation_valid'] and value['sample_time_monotonic_ns'] == 0, value
            assert 'DRIVER_DIAGNOSTICS supported=true' in output and 'decoded=' in output, output
            assert termios.tcgetattr(slave) == original
            assert not unexpected, unexpected
            reopened = os.open(os.ttyname(slave), os.O_RDONLY | os.O_NOCTTY); os.close(reopened)
        finally:
            stop.set(); thread.join(timeout=1)
            os.close(master); os.close(slave)
print('Sensor CLI integration passed (Mock, YAML, recording, background acquisition, signals' +
      (', Yesense PTY).' if args.yesense else ').'))
