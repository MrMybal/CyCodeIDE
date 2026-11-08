"""Stage the shared native facade and presentation resources, never local application data."""
import argparse
from pathlib import Path
import shutil

def stage(core, ide, destination, cli):
    destination = destination.resolve()
    def copy(source, relative):
        target = destination / relative
        if not target.resolve().is_relative_to(destination):
            raise RuntimeError('Unsafe staging destination')
        if source.resolve() == target.resolve():
            return
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    for name in ['CyTool.json', 'contracts.py', 'tool.py', 'requirements.txt', 'LICENSE-CyToolsCore.txt', 'THIRD_PARTY_NOTICES.md']:
        copy(core / 'app' / name, Path('app') / name)
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
    args = parser.parse_args()
    stage(args.core, args.ide, args.destination, args.cli)
