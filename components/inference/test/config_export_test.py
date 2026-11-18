from pathlib import Path
import subprocess
import sys
import tempfile

symbols = ["OPENEMBER_COMPONENT_INFERENCE", "OPENEMBER_COMPONENT_REMOTE_CONTROL", "OPENEMBER_ENABLE_TOOLS_GAMEPAD_MONITOR"]
for mode in ("enabled", "disabled", "legacy"):
    with tempfile.TemporaryDirectory(prefix="openember-config-test-") as folder:
        path = Path(folder)
        lines = [] if mode == "legacy" else ["CONFIG_OPENEMBER_KCONFIG_RESOLVED=y"]
        if mode != "legacy":
            lines += [("CONFIG_" + key + "=y") if mode == "enabled" else ("# CONFIG_" + key + " is not set") for key in symbols]
        (path / ".config").write_text("\n".join(lines) + "\n")
        subprocess.run(["bash", sys.argv[1], folder], check=True, capture_output=True, text=True)
        result = (path / "config.cmake").read_text()
        for key in symbols:
            value = "ON" if mode == "enabled" or (mode == "legacy" and key != "OPENEMBER_COMPONENT_INFERENCE") else "OFF"
            assert f"set({key} {value} CACHE BOOL" in result, (mode, key)
