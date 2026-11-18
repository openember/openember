from pathlib import Path
import subprocess
import sys
import tempfile

for component, yaml, expected in [(True, True, 'ON'), (False, True, 'OFF'),
                                   (None, True, 'ON'), (True, False, 'OFF'), (None, False, 'OFF')]:
    with tempfile.TemporaryDirectory(prefix='openember-yaml-export-') as folder:
        root = Path(folder)
        lines = ['CONFIG_OPENEMBER_USE_YAMLCPP=y' if yaml else '# CONFIG_OPENEMBER_USE_YAMLCPP is not set']
        if component is not None:
            lines += ['CONFIG_OPENEMBER_COMPONENT_CONFIG=y' if component else '# CONFIG_OPENEMBER_COMPONENT_CONFIG is not set']
        (root / '.config').write_text('\n'.join(lines) + '\n')
        subprocess.run(['bash', sys.argv[1], folder], check=True, capture_output=True, text=True)
        assert f'set(OPENEMBER_COMPONENT_CONFIG {expected} CACHE BOOL' in (root / 'config.cmake').read_text()
