"""Host-only process regression tests. Uses Mock devices, never real hardware."""

import argparse
import contextlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def wait_for(predicate, message, processes=(), timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for process in processes:
            require(process.poll() is None, f"process exited early: {process.args}")
        if predicate():
            return
        time.sleep(0.05)
    raise AssertionError(f"timed out: {message}")


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def stop(process, interactive=False, signum=signal.SIGTERM):
    if process.poll() is None:
        if interactive:
            process.stdin.write(b"\n")
            process.stdin.flush()
        else:
            process.send_signal(signum)
    require(process.wait(timeout=8) == 0, f"unclean shutdown: {process.args}")


@contextlib.contextmanager
def running(command, log):
    with log.open("wb") as output:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            yield process
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
            # Children inherit this private process group, including on failure.
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait(timeout=5)
            process.stdin.close()


def hardware(args, work):
    # Existing examples have a fixed endpoint. Refuse to join an unrelated router.
    with socket.socket() as probe:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        probe.bind(("127.0.0.1", 7447))
    source = Path(args.source)
    invalid = work / "invalid.yaml"
    invalid.write_text("endpoints: [\n")
    result = subprocess.run([args.hardware, "--config", str(invalid)],
                            capture_output=True, text=True, timeout=5)
    require(result.returncode == 2 and "config error" in result.stderr,
            f"invalid config was not rejected: {result.stdout}{result.stderr}")
    listener_log = work / "listener.log"
    with running([args.listener], listener_log) as listener:
        wait_for(lambda: "listening for Hardware Interface" in read(listener_log),
                 "listener readiness", [listener])
        with running([args.hardware, "--config", str(source / "examples/hardware_interface/mock.yaml")],
                     work / "sensors.log") as sensors:
            wait_for(lambda: all(token in read(listener_log) for token in
                                 ("imu seq=", "temperature seq=", "gnss seq=")),
                     "all sensor messages", [listener, sensors])
            stop(sensors)
        # Two independent starts exercise release/reacquisition of subscriptions.
        for iteration in range(2):
            offset = len(read(listener_log))
            with running([args.hardware, "--config",
                          str(source / "examples/hardware_interface/mock_joint.yaml")],
                         work / f"joint-{iteration}.log") as joints:
                wait_for(lambda: "joint_state" in read(listener_log)[offset:],
                         "joint state after start", [listener, joints])
                offset = len(read(listener_log))
                result = subprocess.run([args.sender, "--count", "30", "--no-disable"],
                                        capture_output=True, text=True, timeout=10)
                require(result.returncode == 0 and "publish JointCommand" in result.stdout,
                        f"command sender failed: {result.stdout}{result.stderr}")
                wait_for(lambda: "pos=0.5 effort=1" in read(listener_log)[offset:],
                         "command reached controller", [listener, joints])
                wait_for(lambda: any("safe output active" in line and "effort=0" in line
                                     for line in read(listener_log)[offset:].splitlines()),
                         "command timeout safe output", [listener, joints])
                stop(joints, signum=signal.SIGINT if iteration == 0 else signal.SIGTERM)
        stop(listener, interactive=True)


def worker(mode, path):
    with Path(path).open("a") as output:
        output.write(f"{os.getpid()}\n")
    if mode == "fail":
        return 7
    if mode == "success":
        return 0
    while True:
        time.sleep(1)


def pids(path):
    return [int(line) for line in read(path).splitlines()]


def alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def supervision(args, work):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    nodes = []
    for name, mode, restart, startup in (
            ("failure", "fail", "on-failure", 0),
            ("success", "success", "on-failure", 0),
            ("resident", "wait", "always", 0),
            ("no_heartbeat", "wait", "never", 300)):
        nodes.append({"name": name, "command": sys.executable,
                      "args": [str(Path(__file__).resolve()), "worker", mode, str(work / name)],
                      "restart": restart, "restart_limit": 1,
                      "startup_timeout_ms": startup, "shutdown_timeout_ms": 500,
                      "heartbeat_timeout_ms": 0})
    config = work / "launch.yaml"
    config.write_text(json.dumps({"runtime": {"link": {"listen": f"tcp/127.0.0.1:{port}"}},
                                  "nodes": nodes}))
    log = work / "launch.log"
    with running([args.launch_manager, "--launch", str(config),
                  "--pid-file", str(work / "launch.pid")], log) as launch:
        wait_for(lambda: all(pids(work / node["name"]) for node in nodes),
                 "child starts", [launch])
        wait_for(lambda: len(pids(work / "failure")) == 2
                 and "Restart limit reached: name=failure" in read(log),
                 "failure restart limit", [launch])
        wait_for(lambda: "Startup heartbeat timeout: name=no_heartbeat" in read(log)
                 and not alive(pids(work / "no_heartbeat")[0]),
                 "startup timeout cleanup", [launch])
        require(len(pids(work / "success")) == 1, "successful child restarted")
        first_pid = pids(work / "resident")[0]
        os.kill(first_pid, signal.SIGKILL)
        wait_for(lambda: len(pids(work / "resident")) == 2,
                 "resident restarted after crash", [launch])
        require(not alive(first_pid), "old resident was not reaped")
        stop(launch)
        for node in nodes:
            require(all(not alive(pid) for pid in pids(work / node["name"])),
                    f"child leaked: {node['name']}")
    require(len(pids(work / "failure")) == 2, "restart limit exceeded")
    require(len(pids(work / "success")) == 1, "successful child restarted")
    require(len(pids(work / "resident")) == 2, "intentional shutdown restarted child")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "worker":
        return worker(sys.argv[2], sys.argv[3])
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=("hardware", "supervision"))
    parser.add_argument("--hardware")
    parser.add_argument("--listener")
    parser.add_argument("--sender")
    parser.add_argument("--source")
    parser.add_argument("--launch-manager")
    args = parser.parse_args()

    def interrupted(signum, frame):
        raise SystemExit(f"interrupted by signal {signum}")

    signal.signal(signal.SIGTERM, interrupted)
    with tempfile.TemporaryDirectory(prefix="openember-regression-") as directory:
        work = Path(directory)
        try:
            {"hardware": hardware, "supervision": supervision}[args.case](args, work)
        except BaseException:
            for log in sorted(work.glob("*.log")):
                print(f"\n=== {log.name} ===\n{read(log)}", file=sys.stderr)
            raise
    print(f"PASS: {args.case}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
