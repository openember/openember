"""Save a reproducible model delta against resolved shared defaults."""
import os
import pathlib
import re
import sys
import tempfile

def values(path):
    result = {}
    for line in pathlib.Path(path).read_text().splitlines():
        match = re.fullmatch(r'CONFIG_([A-Za-z0-9_]+)=(.*)', line)
        disabled = re.fullmatch(r'# CONFIG_([A-Za-z0-9_]+) is not set', line)
        if match:
            result[match[1]] = match[2]
        elif disabled:
            result[disabled[1]] = 'n'
    return result

current, baseline, model, destination = sys.argv[1:]
selected, defaults = values(current), values(baseline)
output = pathlib.Path(destination)
if output.is_symlink():
    raise SystemExit('Refusing to overwrite a symlinked model defconfig')
lines = [f'# Model: {model}; delta against openember.defconfig and Kconfig defaults.',
         f'CONFIG_PRODUCT_MODEL="{model}"']
for key in sorted(selected.keys() | defaults.keys()):
    if key == 'PRODUCT_MODEL' or key.startswith('OPENEMBER_KCONFIG_'):
        continue
    value = selected.get(key, 'n')
    if value != defaults.get(key, 'n'):
        lines.append(f'# CONFIG_{key} is not set' if value == 'n' else f'CONFIG_{key}={value}')
with tempfile.NamedTemporaryFile(mode='w', dir=output.parent, prefix='.defconfig-', delete=False) as stream:
    stream.write('\n'.join(lines) + '\n')
    name = stream.name
try:
    os.chmod(name, output.stat().st_mode & 0o777 if output.exists() else 0o644)
    os.replace(name, output)
finally:
    if os.path.exists(name):
        os.unlink(name)
print(f'[ember] saved model defaults: {output}')
