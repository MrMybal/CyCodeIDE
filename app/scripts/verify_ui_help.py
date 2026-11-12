"""Verify IDE panel help, including explicit aliases to the pinned Core manual."""
import argparse
from pathlib import Path
import re


def verify(ide, core):
    topics=(core/'app/src/HelpTopics.h').read_text(encoding='utf-8')
    known=set(re.findall(r'\{"([a-zA-Z]+)", "[a-z0-9-]+", "[a-z0-9-]+"\}',topics))
    assert known,'Pinned native help catalogue is missing'
    panels=0
    for source in (ide/'app/src').glob('*.cpp'):
        for panel,alias in re.findall(r'beginPanel\([^,\n]+,\s*"([a-zA-Z]+)"(?:\s*,\s*"([a-zA-Z]+)")?\s*\)',source.read_text(encoding='utf-8')):
            assert (alias or panel) in known,f'{source.name}: missing native help for {panel}'
            panels+=1
    assert panels,'No native IDE panels checked'
    return panels


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core',type=Path,required=True)
    args=parser.parse_args()
    print('IDE panels with validated native help:',verify(Path(__file__).resolve().parents[2],args.core))
