# Builds index.html out of src/: the page template, the parts it includes and the texts in both
# languages. Standard library only.
#   python3 tools/build.py          write index.html
#   python3 tools/build.py --check  fail when index.html is not what src/ builds
#
# In the template:
#   {{ hero.title }}  the text under that key of src/strings.toml, as <span lang="ru"> and
#                     <span lang="en">; the page shows the one its <html data-lang> names
#   {{> hero }}       the contents of src/parts/hero.html, built the same way
#   {{ modules }}     a modulepreload link for every module of assets/js but main.js, so the
#                     browser fetches them all at once instead of one import after another
# Texts may hold markup. A text used nowhere, a key with no such text and a text without one of
# the two languages all stop the build.

import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'src'
OUT = ROOT / 'index.html'
MODULES = ROOT / 'assets' / 'js'
kLanguages = ('ru', 'en')
kNotice = '<!-- Built by tools/build.py from src/: edit the files there and run it again. -->'


def flatten(table, prefix=''):
    """{'hero': {'title': {'ru': ..., 'en': ...}}} -> {'hero.title': {'ru': ..., 'en': ...}}"""
    texts = {}
    for key, value in table.items():
        path = prefix + key
        if not isinstance(value, dict):
            raise SystemExit(f'strings.toml: {path} is a bare value, not a text with ru and en')
        if set(value) & set(kLanguages):
            if set(value) != set(kLanguages):
                raise SystemExit(f'strings.toml: {path} needs exactly {" and ".join(kLanguages)}')
            texts[path] = value
        else:
            texts.update(flatten(value, path + '.'))
    return texts


def build():
    texts = flatten(tomllib.loads((SRC / 'strings.toml').read_text('utf-8')))
    used = set()
    modules = ''.join(f'  <link rel="modulepreload" href="assets/js/{path.name}">\n'
                      for path in sorted(MODULES.glob('*.js')) if path.name != 'main.js')

    def expand(source, name):
        def part(match):
            path = SRC / 'parts' / (match.group(1) + '.html')
            if not path.exists():
                raise SystemExit(f'{name}: no part {match.group(1)}')
            return expand(path.read_text('utf-8'), path.name).rstrip('\n')

        def text(match):
            key = match.group(1)
            if key == 'modules':
                return modules.rstrip('\n').lstrip()
            if key not in texts:
                raise SystemExit(f'{name}: no text {key} in strings.toml')
            used.add(key)
            return ''.join(f'<span lang="{lang}">{texts[key][lang].strip()}</span>' for lang in kLanguages)

        source = re.sub(r'\{\{>\s*([\w/-]+)\s*\}\}', part, source)
        return re.sub(r'\{\{\s*([\w.]+)\s*\}\}', text, source)

    page = expand((SRC / 'page.html').read_text('utf-8'), 'page.html')
    unused = sorted(set(texts) - used)
    if unused:
        raise SystemExit('strings.toml: used nowhere: ' + ', '.join(unused))
    doctype, rest = page.split('\n', 1)
    return f'{doctype}\n{kNotice}\n{rest}'


page = build()
if '--check' in sys.argv:
    if not OUT.exists() or OUT.read_text('utf-8') != page:
        raise SystemExit('index.html is out of date: run python3 tools/build.py')
else:
    OUT.write_text(page, 'utf-8')
    print(f'index.html: {len(page.encode())} bytes')
