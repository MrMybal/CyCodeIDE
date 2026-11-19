"""Stage the shared native facade and presentation resources, never local application data."""
import argparse
import json
import re
from pathlib import Path
import shutil
import hashlib

def stage(core, ide, destination, cli, host_executable, *, sdk_source=None, cyui_source=None):
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
    for name in ['contracts.py', 'tool.py', 'requirements.txt', 'native_sdk.py', 'native_client.py', 'sdk_cli.py', 'core_host.py']:
        copy(core / 'app' / name, Path('app') / name)
    copy(core/'app/scripts/migrate_legacy_jobs.py',Path('app/scripts/migrate_legacy_jobs.py'))
    copy(core/'app/cymcp-source.lock.json',Path('app/cymcp-source.lock.json'))
    copy(core / 'app/THIRD_PARTY_NOTICES.md', Path('app/CORE_THIRD_PARTY_NOTICES.md'))
    host_notice = ide / 'app/THIRD_PARTY_NOTICES.md'
    copy(host_notice if host_notice.is_file() else core / 'app/THIRD_PARTY_NOTICES.md',
         Path('app/THIRD_PARTY_NOTICES.md'))
    # Preserve every canonical Core contract. Only ide_* presentation operations
    # are appended by the host; their handlers call the real UI thread.
    manifest=json.loads((core/'app/CyTool.json').read_text(encoding='utf-8'))
    manifest['name']='CyCodeIDE'
    manifest['standaloneExecutable']=host_executable
    catalogue=json.loads((core/'app/MCPMethods.json').read_text(encoding='utf-8'))
    additions=json.loads((ide/'app/ide-operations.json').read_text(encoding='utf-8'))['operations']
    canonical={entry['id'] for entry in catalogue['operations']}
    if any(not op['id'].startswith('ide_') or not op['nativeMethod'].startswith('ide.') or op['id'] in canonical for op in additions):
        raise ValueError('Host operations cannot replace a Core contract')
    if len({op['id'] for op in additions})!=len(additions):
        raise ValueError('Duplicate IDE operation')
    catalogue['operations'].extend(additions)
    catalogue['operationCount']=len(catalogue['operations'])
    manifest['operations']=catalogue['operations']
    copy(ide/'app/ide-operations.json',Path('app/ide-operations.json'))
    copy(core/'app/MCPMethods.json',Path('app/MCPMethods.json'))
    target=destination/'app/IDEAutomationMethods.json';target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(catalogue,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    target=destination/'app/CyTool.json'
    if not target.resolve().is_relative_to(destination):
        raise RuntimeError('Unsafe manifest staging destination')
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    for folder in ['resources/fonts', 'locales', 'licenses']:
        source_root = ide / 'app' / folder
        if not source_root.is_dir():
            source_root = core / 'app' / folder
        roots=[core/'app/licenses',source_root] if folder=='licenses' else [source_root]
        owned={}
        for source_root in roots:
            for source in source_root.rglob('*'):
                if source.is_file() and not source.is_symlink():
                    if 'CyToolsCore' in source.name:
                        continue
                    relative=source.relative_to(source_root)
                    # Keep canonical case on Windows and case-sensitive package authors.
                    key=relative.as_posix().casefold()
                    target=owned.setdefault(key,relative)
                    copy(source,Path('app')/folder/target)
    if sdk_source:
        copy(sdk_source/'THIRD_PARTY_NOTICES.md',Path('app/licenses/CyMCPSDK-NOTICES.md'))
    if cyui_source:
        copy(cyui_source/'third_party/md4c/LICENSE.md',Path('app/licenses/md4c.txt'))
    copy(ide / 'icons/CyCode.png', Path('icons/CyCode.png'))
    copy(cli, Path(cli.name))
    suffixes = {'.dll', '.so', '.dylib', '.json'}
    for source in (core / 'runtime/plugins').glob('*'):
        if source.is_file() and source.name.split('.')[0] in {'cycode_sample', 'cycode_unreal', 'cycode_svn', 'cycode_perforce', 'cycode_mercurial', 'cycode_diversion', 'sample', 'unreal', 'svn', 'perforce', 'mercurial', 'diversion'} and source.suffix in suffixes:
            copy(source, Path('runtime/plugins') / source.name)

def distribution_source_digest(ide):
    paths = {ide/'CMakeLists.txt', ide/'app/CMakeLists.txt'}
    paths.update((ide/'app').glob('*source.lock.json'))
    paths.add(ide/'app/ide-operations.json')
    for folder in ['src', 'include']:
        paths.update(p for p in (ide/'app'/folder).rglob('*') if p.is_file())
    paths.update(p for p in (ide/'app/resources').rglob('*')
                 if p.is_file() and (p.name.endswith('.rc.in') or p.suffix=='.manifest'))
    digest = hashlib.sha256()
    for path in sorted(paths):
        if path.is_symlink():
            raise ValueError('Distribution compiler input is redirected')
        digest.update(path.relative_to(ide).as_posix().encode('utf-8')+b'\0')
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def record_distribution(ide, metadata_path):
    info=json.loads(metadata_path.read_text(encoding='utf-8'))
    executable=Path(info['executable']);helper=Path(info['helper'])
    binaries=[executable,helper,executable.parent/'cycode_cli.exe']
    if any(not p.is_file() or p.is_symlink() for p in binaries):
        raise ValueError('Missing or redirected final distribution executable')
    receipt={'metadata':info,'sourceDigest':distribution_source_digest(ide),
             'binaries':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in binaries}}
    metadata_path.with_suffix('.receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['core', 'ide', 'destination', 'cli']:
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('--host-executable',required=True)
    parser.add_argument('--sdk-source',type=Path)
    parser.add_argument('--cyui-source',type=Path)
    parser.add_argument('--update-build-info',type=Path)
    args = parser.parse_args()
    stage(args.core, args.ide, args.destination, args.cli, args.host_executable,
          sdk_source=args.sdk_source,cyui_source=args.cyui_source)
    if args.update_build_info:
        record_distribution(args.ide,args.update_build_info)
