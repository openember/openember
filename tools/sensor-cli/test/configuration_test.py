"""Configuration transactions against a PTY device emulator. No physical writes."""
import argparse
import json
import os
from pathlib import Path
import pty
import select
import struct
import subprocess
import termios
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('binary')
args = parser.parse_args()
binary = str(Path(args.binary).resolve())


def checksummed(body):
    a = b = 0
    for value in body:
        a = (a + value) % 256; b = (b + a) % 256
    return b'YS' + body + bytes([a, b])


def reply(category, operation, payload):
    return checksummed(bytes([category]) + struct.pack('<H', len(payload)*8+operation) + payload)


class Device:
    def __init__(self, failure=''):
        self.master, self.slave = pty.openpty()
        original = termios.tcgetattr(self.slave)
        original[3] &= ~(termios.ECHO | termios.ECHONL)
        termios.tcsetattr(self.slave, termios.TCSANOW, original)
        self.original = termios.tcgetattr(self.slave)
        self.path = os.ttyname(self.slave)
        self.failure = failure
        self.stop = threading.Event()
        self.commands = []
        self.thread = threading.Thread(target=self.loop, daemon=True)
        self.errors = []
        self.thread.start()

    def loop(self):
        try:
            time.sleep(.05)
            buffer = b''; tid = 1; stream_started = False
            rate = 8; mask = 0xc8; baud = 5
            while not self.stop.is_set():
                if select.select([self.master], [], [], .002)[0]:
                    buffer += os.read(self.master,4096)
                while len(buffer) >= 5:
                    assert buffer[:2] == b'YS', buffer
                    category = buffer[2]
                    word = struct.unpack('<H',buffer[3:5])[0]
                    op, length = word & 7, word >> 3
                    if len(buffer) < length+7: break
                    packet, buffer = buffer[:length+7], buffer[length+7:]
                    assert packet == checksummed(packet[2:-2]), packet
                    payload = packet[5:-2]
                    self.commands.append((category,op,payload))
                    if self.failure == 'startup' and not stream_started: continue
                    if op == 0:
                        if self.failure == 'baud_timeout' and baud != 5: continue
                        data = bytes([baud]) if category == 2 else bytes([rate]) if category == 3 else struct.pack('<H',mask)
                        if self.failure == 'readback_mismatch' and category == 3 and rate == 7: data = bytes([8])
                        os.write(self.master,reply(category,0,data))
                    elif category == 2:
                        baud = payload[0]  # Deliberately no ACK for a baud write.
                    else:
                        if self.failure == 'reject' and category == 3:
                            os.write(self.master,reply(category,op,b'\xff')); continue
                        if category == 3: rate = payload[0]
                        elif category == 4: mask = struct.unpack('<H',payload)[0]
                        elif category != 0x4d: raise AssertionError((category,op,payload))
                        if self.failure == 'timeout' and category == 3: continue
                        # Wrong category/operation must not satisfy the transaction.
                        os.write(self.master,reply(category,3-op,b'\xff'))
                        response = reply(category,op,b'\x50\x00' if category == 0x4d else b'\x00')
                        os.write(self.master,response[:3]); os.write(self.master,response[3:])
                fields = b''
                if mask & 0x80: fields += b'\x10\x0c' + struct.pack('<iii',*( (0,0,9806650) if self.failure == 'calibration' else (1000000,-2000000,9000000) ))
                if mask & 0x40: fields += b'\x20\x0c' + struct.pack('<iii',*( (0,0,0) if self.failure == 'calibration' else (90000000,0,0) ))
                if mask & 0x08: fields += b'\x41\x10' + struct.pack('<iiii',500000,-500000,500000,-500000)
                if mask & 0x400: fields += b'\x01\x02' + struct.pack('<h',2534)
                if mask & 0x20: fields += b'\x31\x0c' + struct.pack('<iii',100000,-200000,300000)
                if fields:
                    os.write(self.master,checksummed(struct.pack('<HB',tid,len(fields))+fields)); stream_started = True
                tid = tid % 60000 + 1
        except Exception as e:
            self.errors.append(repr(e))

    def run(self, commands, control=True, ok=True, input_commands=None):
        argv = [binary,'imu','--driver','yesense','--device',self.path,'--baud','921600']
        if control: argv += ['--device-control']
        for command in commands: argv += ['--command',command]
        result = subprocess.run(argv,input=input_commands,capture_output=True,text=True,timeout=20)
        assert (result.returncode == 0) == ok, (result.args,result.stdout,result.stderr)
        assert not self.errors, self.errors
        assert termios.tcgetattr(self.slave) == self.original, 'host termios not restored'
        extra = os.open(self.path,os.O_RDONLY|os.O_NOCTTY); os.close(extra)
        return result.stdout,result.stderr

    def close(self):
        self.stop.set(); self.thread.join(timeout=1)
        os.close(self.master); os.close(self.slave)


def case(failure, commands, ok=True, control=True, input_commands=None):
    device = Device(failure)
    try:
        output,error = device.run(commands,control,ok,input_commands)
        return output,error,list(device.commands)
    finally:
        device.close()


# RAM is default, Flash explicit; output fields can remove quaternion without stopping acquisition.
output,_,commands = case('', ['capabilities','device-config','set rate 50',
    'set fields accel,gyro,temperature,magnetic','sample','set rate 100 flash',
    'set baud 460800','device-config','sample','close','host-baud 460800','quit'])
assert 'state=APPLIED' in output and 'device_baud=460800' in output, output
assert (3,1,b'\x07') in commands and (3,2,b'\x08') in commands, commands
assert (2,1,b'\x04') in commands, commands
samples = [json.loads(line) for line in output.splitlines() if line.startswith('{')]
assert samples and all(s['raw_quaternion_wxyz'] is None and not s['orientation_present'] for s in samples), samples
assert samples[-1]['temperature_celsius'] == 25.34, samples
assert samples[-1]['magnetic_field_tesla'] == [1e-5,-2e-5,3e-5], samples
assert all(s['acceleration_valid'] and s['angular_velocity_valid'] for s in samples), samples

output,_,commands = case('startup',['device-config'])
assert 'device_baud=921600' in output, output
output,_,commands = case('reject',['set rate 50'],ok=False)
assert 'state=REJECTED' in output and sum(op != 0 for _,op,_ in commands) == 1, (output,commands)
output,error,commands = case('timeout',[],ok=False,input_commands='set rate 50\ndevice-config\nquit\n')
assert 'state=UNCONFIRMED' in output and sum(op != 0 for _,op,_ in commands) == 1, (output,commands)
assert 'reopen' in error, error
output,_,commands = case('readback_mismatch',['set rate 50'],ok=False)
assert 'state=UNCONFIRMED' in output, output
output,_,commands = case('baud_timeout',['set baud 460800'],ok=False)
assert 'state=UNCONFIRMED' in output and sum(op != 0 for _,op,_ in commands) == 1, (output,commands)
# Bandwidth validation can query, but sends no configuration write.
_,error,commands = case('',['set baud 9600'],ok=False)
assert 'bandwidth' in error and all(op == 0 for _,op,_ in commands), (error,commands)
_,_,commands = case('',['device-config'],ok=False,control=False)
assert not commands, commands
_,error,commands = case('',['calibrate static-gyro'],ok=False)
assert '10 seconds' in error and not commands, (error,commands)
output,_,commands = case('calibration',['stats --seconds 10.1','calibrate static-gyro'])
assert 'CALIBRATION state=ACCEPTED' in output and (0x4d,1,b'\x50\x01') in commands, (output,commands)
print('IMU configuration passed: mixed frames/replies, readback, RAM/Flash, baud, partial samples, rejection, timeout, bandwidth and read-only policy.')
