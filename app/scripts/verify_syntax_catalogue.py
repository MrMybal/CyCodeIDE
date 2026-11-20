"""Read-only consistency checks of the exact published CyUI syntax dependency."""
import argparse
import json
from pathlib import Path
import re
from cyui_dependency import resolve


def verify(source):
    source = resolve(source, source)
    catalogue = json.loads((source / 'data/code-languages.json').read_text('utf-8'))
    ids = [item['id'] for item in catalogue]
    assert len(ids) == len(set(ids)), 'Duplicate syntax IDs'
    enum = (source / 'include/cyui/detail/CodeLanguageList.inc').read_text('utf-8')
    generated = (source / 'src/modules/code/LanguageData.cpp').read_text('utf-8')
    enum_ids = re.findall(r'^([a-zA-Z0-9_]+)\s*=\s*\d+,', enum, re.M)
    assert ids == enum_ids, 'Shared catalogue/enum order differs'
    for entry in catalogue:
        assert 'CodeLanguage::' + entry['id'] + ',' in generated, entry['id']
    modes = {entry['id']: entry for entry in catalogue}
    for name in ('zig', 'zon'):
        assert name in modes and name in modes[name]['extensions']
        assert modes[name]['lexer'] == 'zig'
    assert modes['zon']['properties']['lexer.zig.zon'] == '1'
    assert {'nan', 'inf'} <= set(modes['zon']['wordLists']['2'].split())
    assert (source / 'third_party/lexilla/lexers/LexZig.cxx').is_file()
    cmake = (source / 'CMakeLists.txt').read_text('utf-8')
    assert 'third_party/lexilla/lexers/*.cxx' in cmake and '${CYUI_LEXER_SOURCES}' in cmake, 'Production lexer glob missing'
    for guide in ('docs/AI_GUIDE.md', 'docs/MODULES.md', 'docs/CODE.md', '.CyDocBuild/manual/fr/code.md'):
        assert (source / guide).is_file(), guide
    print(f'Exact clean CyUI revision verified; {len(catalogue)} catalogue/enum/generated profiles consistent, including Zig/ZON.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cyui', type=Path, required=True)
    verify(parser.parse_args().cyui)
