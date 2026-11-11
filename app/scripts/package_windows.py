"""Author CyCodeIDE update packages using the pinned Core's reviewed helpers.

Never install, launch a payload, publish a release or change GitHub Actions.
"""
import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from stage_runtime import distribution_source_digest

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = 'CyCodeIDE'
REPOSITORY = 'MrMybal/CyCodeIDE'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', '-c', 'safe.directory='+root.as_posix(),
                                   '-C', str(root), *args]).decode('utf-8').strip()


def configuration(build):
    info = json.loads((build/'ide-distribution-Release.json').read_text(encoding='utf-8'))
    app_version = re.search(r'project\(CyCodeIDE VERSION ([0-9.]+)',
                            (ROOT/'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
    expected = {'product': PRODUCT, 'repository': REPOSITORY, 'version': app_version,
                'configuration': 'Release', 'platform': 'windows-x64', 'updates': 'ON'}
    if any(info.get(key) != value for key, value in expected.items()):
        raise ValueError('Build a matching Release with CyCodeIDE updates enabled before packaging')
    stage = build/'app/Release'
    for key, name in [('executable', 'CyCodeIDE.exe'), ('helper', 'CyCodeIDEUpdater.exe')]:
        if Path(info[key]).resolve() != (stage/name).resolve():
            raise ValueError('Build metadata does not belong to this distribution directory')
    core = Path(info['coreRoot']).resolve(strict=True)
    lock = json.loads((ROOT/'app/core-source.lock.json').read_text(encoding='utf-8'))
    if git(core, 'rev-parse', 'HEAD') != lock['revision'] or git(core, 'status', '--porcelain'):
        raise ValueError('Package authoring requires the exact clean pinned Core sources')
    receipt_path=build/'ide-distribution-Release.receipt.json'
    if not receipt_path.is_file():
        raise ValueError('Complete the final IDE link/staging before packaging')
    receipt=json.loads(receipt_path.read_text(encoding='utf-8'))
    if receipt.get('metadata')!=info or receipt.get('sourceDigest')!=distribution_source_digest(ROOT):
        raise ValueError('Rebuild: package identity or compiler inputs changed after the last final link')
    for name in ['CyCodeIDE.exe','CyCodeIDEUpdater.exe','cycode_cli.exe']:
        if digest((stage/name).read_bytes())!=receipt.get('binaries',{}).get(name):
            raise ValueError('Rebuild: distribution executable changed after the final link')
    return app_version, core, stage


def gather(core, stage, app_version, build, output, mcp_bundle, tools_root, release, installer, product):
    entries = {}

    def include(source, name):
        source = Path(source)
        if not source.is_file() or source.is_symlink() or not release.package_path(name, product=product):
            raise ValueError('Missing, redirected or unsafe package file: '+name)
        data = source.read_bytes()
        if name in entries and entries[name] != data:
            raise ValueError('Conflicting package source: '+name)
        entries[name] = data

    for name in ['CyCodeIDE.exe', 'CyCodeIDEUpdater.exe', 'cycode_cli.exe']:
        include(stage/name, name)
    for name in ['CyCodeIDE.exe', 'CyCodeIDEUpdater.exe']:
        if release.executable_version(stage/name) != release.version_tuple(app_version):
            raise ValueError('IDE/helper executable version does not match the package')
    for name in ['LICENSE', 'COPYRIGHT', 'README.md', 'README.fr.md', 'VERSION.md', 'VERSION.fr.md']:
        include(ROOT/name, name)
    # Keep the distributed README/version links useful offline; historical
    # extraction evidence remains linked to the exact corresponding source.
    for name in ['Updates.md', 'Releases/v0.2.3.md', 'Releases/v0.2.3.fr.md']:
        include(ROOT/'Docs'/name, 'app/Docs/'+name)
    source_url='https://github.com/MrMybal/CyCodeIDE/blob/'+git(ROOT,'rev-parse','HEAD')+'/Docs/Extraction.md'
    for name in ['README.md','README.fr.md','VERSION.md','VERSION.fr.md']:
        text=entries[name].decode('utf-8').replace('](Docs/Updates.md)','](app/Docs/Updates.md)')
        text=text.replace('](Docs/Releases/','](app/Docs/Releases/').replace('](Docs/Extraction.md)',']('+source_url+')')
        entries[name]=text.encode('utf-8')
    for name in ['CyCode.png', 'CyCode.ico', 'CyCode-document.png', 'CyCode-document.ico']:
        include(ROOT/'icons'/name, 'icons/'+name)
    for name in ['CyTool.json', 'contracts.py', 'tool.py', 'requirements.txt',
                 'LICENSE-CyToolsCore.txt', 'THIRD_PARTY_NOTICES.md', 'CORE_THIRD_PARTY_NOTICES.md']:
        include(stage/'app'/name, 'app/'+name)
    for name in ['core_host.py', 'MCPMethods.json', 'mcp-runtime.lock.json', 'runtime-tools.lock.json']:
        include(core/'app'/name, 'app/'+name)
    for name in ['core-source.lock.json', 'editor-source.lock.json', 'cyui-source.lock.json']:
        include(ROOT/'app'/name, 'app/'+name)
    for folder in ['locales', 'licenses', 'resources/fonts', 'Documentation']:
        source = stage/'app'/folder
        if not source.is_dir():
            raise ValueError('Missing complete distribution resources: '+folder)
        for path in sorted(source.rglob('*')):
            if path.is_file():
                include(path, 'app/'+folder+'/'+path.relative_to(source).as_posix())
    documentation = importlib.import_module('verify_documentation')
    documentation.verify(stage/'app/Documentation')
    for provider in ['sample', 'unreal', 'svn', 'perforce', 'mercurial', 'diversion']:
        include(core/'runtime/plugins'/('cycode_'+provider+'.dll'), 'extensions/cycode_'+provider+'.dll')
        include(core/'app/plugins'/provider/'plugin.json', 'extensions/'+provider+'.json')
    # The Core's locked ripgrep and real isolated MCP runtime remain part of the
    # IDE package, rather than silently disappearing on the first update.
    tools = tools_root or core/'runtime/tools'
    stamp = json.loads((tools/'ripgrep.json').read_text(encoding='utf-8'))
    locked = json.loads((core/'app/runtime-tools.lock.json').read_text(encoding='utf-8'))['ripgrep']
    if (stamp['archiveSha256'] != locked['platforms']['windows-x64']['sha256'] or
            stamp['binarySha256'] != digest((tools/'rg.exe').read_bytes())):
        raise ValueError('Use the Core checksum-verified ripgrep runtime')
    include(tools/'rg.exe', 'tools/rg.exe')
    for name in ['COPYING', 'LICENSE-MIT', 'UNLICENSE']:
        include(tools/'licenses/ripgrep'/name, 'tools/licenses/ripgrep/'+name)
    bundle = importlib.import_module('mcp_bundle')
    lock = json.loads((core/'app/mcp-runtime.lock.json').read_text(encoding='utf-8'))
    location = mcp_bundle or core/'runtime/mcp-bundles'/bundle.lock_digest(lock)
    runtime = bundle.load_bundle(location, lock)
    if entries.keys() & runtime.keys():
        raise ValueError('MCP bundle collides with another package component')
    entries.update(runtime)
    entries.update(installer.system_runtime(build, 'Release', output))
    entries['PACKAGE.md'] = (f'# CyCodeIDE {app_version} — Windows x64\n\n'
        'Extract the complete folder and run CyCodeIDE.exe. Keep app/, icons/, tools/ and extensions/.\n'
        'The native Core, providers, isolated MCP runtime and bilingual offline manual are included.\n'
        'Compilers, language servers, VCS clients and an OpenGL driver remain external requirements.\n'
        'Updates use only stable releases from https://github.com/MrMybal/CyCodeIDE/releases.\n'
        'Download and installation require explicit actions. Save or discard open documents before restart.\n'
        'User data and unknown files are preserved; changed managed files block replacement.\n'
        'Use CYCODEIDE_UPDATE_TOKEN, GH_TOKEN or GITHUB_TOKEN for private releases. Never store tokens in files.\n'
        'This unsigned local package has not been installed or validated on a clean machine.\n'
        'No settings, credentials, build trees or local workspace samples are included.\n').encode('utf-8')
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'runtime/releases')
    parser.add_argument('--mcp-bundle', type=Path)
    parser.add_argument('--tools-root', type=Path, help='Core checksum-verified runtime tools')
    parser.add_argument('--iscc', type=Path)
    parser.add_argument('--portable-only', action='store_true')
    parser.add_argument('--allow-local-changes', action='store_true',
                        help='Explicit local review package, never evidence of a published source revision')
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    app_version, core, stage = configuration(build)
    dirty = bool(git(ROOT, 'status', '--porcelain'))
    if dirty and not args.allow_local_changes:
        raise ValueError('Preserve local changes; commit reviewed sources before release packaging')
    sys.path.insert(0, str(core/'app/scripts'))
    release = importlib.import_module('release_package')
    installer = importlib.import_module('windows_installer')
    product = release.UpdateProduct(PRODUCT, REPOSITORY)
    entries = gather(core, stage, app_version, build, output, args.mcp_bundle, args.tools_root,
                     release, installer, product)
    revision = git(ROOT, 'rev-parse', 'HEAD')
    provenance = {'product': PRODUCT, 'repository': REPOSITORY, 'sourceCommit': revision,
                  'sourceDirty': dirty, 'license': 'GPL-3.0-only',
                  'dependencies': {name: json.loads((ROOT/'app'/(name+'-source.lock.json')).read_text(encoding='utf-8'))
                                   for name in ['core', 'editor', 'cyui']}}
    required = (release.REQUIRED - {'CyCode.exe', 'CyCodeUpdater.exe'}) | {
        'CyCodeIDE.exe', 'CyCodeIDEUpdater.exe'}
    entries = release.payload(entries, app_version, provenance, product=product, required=required)
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH') or git(ROOT, 'show', '-s', '--format=%ct', 'HEAD'))
    timestamp = time.gmtime(max(epoch, 315532800))[:6]
    output.mkdir(parents=True, exist_ok=True)
    archive = output/f'CyCodeIDE-{app_version}-windows-x64.zip'
    if archive.exists():
        raise FileExistsError('Preserve the previous package; choose another --output directory')
    release.write_archive(archive, entries, app_version, timestamp, update=True,
                          product=product, required=required)
    if archive.stat().st_size > release.MAXIMUM:
        raise ValueError('Portable ZIP exceeds the native update download budget')
    artifacts = [archive]
    if not args.portable_only:
        artifacts.append(installer.compile_installer(entries, app_version, output, args.iscc, product=product,
            script=ROOT/'app/packaging/windows.iss', required=required))
    checksums = {p.name: digest(p.read_bytes()) for p in artifacts}
    (output/'SHA256SUMS.txt').write_text(''.join(value+'  '+name+'\n' for name, value in checksums.items()),
                                       encoding='utf-8')
    result = {'product': PRODUCT, 'version': app_version, 'sourceCommit': revision,
              'sourceDirty': dirty, 'files': len(entries), 'sha256': checksums,
              'artifacts': [str(p) for p in artifacts], 'payloadExecuted': False, 'published': False}
    (output/'package-review.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
