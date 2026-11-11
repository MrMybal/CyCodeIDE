"""Stage the shared native facade and presentation resources, never local application data."""
import argparse
import json
import re
from pathlib import Path
import shutil

def stage(core, ide, destination, cli, host_executable):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', host_executable):
        raise ValueError('Host executable must be a single distribution filename')
    destination = destination.resolve()
    def copy(source, relative):
        target = destination / relative
        if not target.resolve().is_relative_to(destination):
            raise RuntimeError('Unsafe staging destination')
        if source.resolve() == target.resolve():
            return
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    for name in ['contracts.py', 'tool.py', 'requirements.txt', 'LICENSE-CyToolsCore.txt']:
        copy(core / 'app' / name, Path('app') / name)
    copy(core / 'app/THIRD_PARTY_NOTICES.md', Path('app/CORE_THIRD_PARTY_NOTICES.md'))
    host_notice = ide / 'app/THIRD_PARTY_NOTICES.md'
    copy(host_notice if host_notice.is_file() else core / 'app/THIRD_PARTY_NOTICES.md',
         Path('app/THIRD_PARTY_NOTICES.md'))
    # The operation/permission registry stays byte-for-byte equivalent to Core;
    # only application catalogue metadata belongs to this host.
    manifest=json.loads((core/'app/CyTool.json').read_text(encoding='utf-8'))
    manifest['name']='CyCodeIDE'
    manifest['standaloneExecutable']=host_executable
    target=destination/'app/CyTool.json'
    if not target.resolve().is_relative_to(destination):
        raise RuntimeError('Unsafe manifest staging destination')
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    for folder in ['resources/fonts', 'locales', 'licenses']:
        source_root = ide / 'app' / folder
        if not source_root.is_dir():
            source_root = core / 'app' / folder
        for source in source_root.rglob('*'):
            if source.is_file() and not source.is_symlink():
                copy(source, Path('app') / folder / source.relative_to(source_root))
    copy(ide / 'icons/CyCode.png', Path('icons/CyCode.png'))
    copy(cli, Path(cli.name))
    suffixes = {'.dll', '.so', '.dylib', '.json'}
    for source in (core / 'runtime/plugins').glob('*'):
        if source.is_file() and source.name.split('.')[0] in {'cycode_sample', 'cycode_unreal', 'cycode_svn', 'cycode_perforce', 'cycode_mercurial', 'cycode_diversion', 'sample', 'unreal', 'svn', 'perforce', 'mercurial', 'diversion'} and source.suffix in suffixes:
            copy(source, Path('runtime/plugins') / source.name)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['core', 'ide', 'destination', 'cli']:
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--host-executable',required=True)
    args = parser.parse_args()
    stage(args.core, args.ide, args.destination, args.cli, args.host_executable)
