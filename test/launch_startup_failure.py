"""Early startup failures must exit cleanly without touching an existing owner."""
import fcntl
import os
from pathlib import Path
import subprocess
import sys
import tempfile


launcher = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="ember-launch-startup-") as folder:
    root = Path(folder)
    pid_file = root / "owner.pid"
    owner = f"{os.getpid()}\n"
    with pid_file.open("w+") as lock:
        lock.write(owner)
        lock.flush()
        fcntl.lockf(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        # No launch configuration is loaded and no children/router are started.
        for _ in range(5):
            result = subprocess.run(
                [launcher, "--pid-file", str(pid_file),
                 "--launch", str(root / "not-loaded.yaml")],
                capture_output=True, text=True, timeout=10)
            assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
            assert "has been running" in result.stdout + result.stderr, (result.stdout, result.stderr)
            # Closing another descriptor for this inode would release our POSIX lock.
            lock.seek(0)
            assert lock.read() == owner, "existing owner's PID was modified"
            assert pid_file.exists(), "existing owner's PID file was removed"

    # An open failure follows the same early-return path without a lock owner.
    result = subprocess.run(
        [launcher, "--pid-file", str(root / "missing" / "owner.pid")],
        capture_output=True, text=True, timeout=10)
    assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
    assert "Fail to open pid file" in result.stderr
print("LAUNCH_STARTUP_FAILURE passed: duplicate owner preserved, clean failure exits")
