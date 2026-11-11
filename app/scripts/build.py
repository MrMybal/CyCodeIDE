"""Configure and build CyCodeIDE; never execute GUI, CLI, helpers or tests."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import hashlib
import tarfile
import subprocess
import sys
from core_dependency import resolve
from editor_dependency import ensure_editor

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core-root', type=Path)
    parser.add_argument('--editor-root', type=Path)
    parser.add_argument('--deps', type=Path, default=ROOT/'runtime/deps')
    parser.add_argument('--cyui', type=Path)
    parser.add_argument('--configuration', default='Release')
    parser.add_argument('--build-dir', type=Path, default=ROOT/'runtime/build', help='Separate build output without replacing an existing configuration')
    parser.add_argument('--fetch', action='store_true', help='Fetch pinned native and graphical build sources')
    parser.add_argument('--plan', action='store_true')
    parser.add_argument('--without-documentation', action='store_true', help='Explicit compile-only check; release builds require local help')
    args = parser.parse_args()
    if args.plan:
        print(json.dumps({'core': str(args.core_root or 'pinned CyCode'), 'editor': str(args.editor_root or 'pinned Reader'),
            'buildDirectory': str(args.build_dir.resolve()),
            'documentation': not args.without_documentation, 'builds': ['CyCodeIDE', 'cycode_cli', 'providers'],
            'executesApplication': False, 'nativeTests': False}, indent=2))
        return 0
    core = resolve(args.core_root)
    editor = ensure_editor(args.editor_root)
    env = {key.upper(): value for key, value in os.environ.items()} if sys.platform == 'win32' else dict(os.environ)
    cmake = shutil.which('cmake')
    if sys.platform == 'win32':
        where = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        vs = Path(subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip())
        if not cmake:
            cmake = str(vs/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
        # Trusted fixed tool invocation, no project-provided shell command.
        output = subprocess.check_output('"'+str(vs/'VC/Auxiliary/Build/vcvars64.bat')+'" >nul && set', shell=True, env=env).decode(errors='replace')
        env.update(dict(line.split('=',1) for line in output.splitlines() if '=' in line and not line.startswith('=')))
        env = {key.upper(): value for key, value in env.items()}
        env['VSLANG'] = '1033'
    if not cmake:
        raise RuntimeError('Install CMake 3.24+')
    if args.fetch:
        # Native lock belongs to Core; GUI lock belongs to the shared editor.
        # Use the existing official checksum downloaders with an explicit destination.
        spec = importlib.util.spec_from_file_location('reader_fetch', editor/'scripts/fetch_dependencies.py')
        fetcher = importlib.util.module_from_spec(spec); spec.loader.exec_module(fetcher)
        fetcher.fetch(args.deps, core_only=False)
        spec = importlib.util.spec_from_file_location('core_fetch', core/'app/scripts/fetch_dependencies.py')
        native = importlib.util.module_from_spec(spec); spec.loader.exec_module(native)
        native.DEPS = args.deps; native.main([])
        for key,item in json.loads((ROOT/'app/dependencies.lock.json').read_text('utf-8')).items():
            target = args.deps/key
            marker = target/'.cycode-dependency'
            if marker.is_file():
                if marker.read_text('utf-8') != item['sha256']:
                    raise RuntimeError('Image dependency cache differs from lock: '+key)
                continue
            archive = args.deps/(key+'.tar.gz')
            if not archive.exists():
                native.download(item['url'],archive)
            if hashlib.sha256(archive.read_bytes()).hexdigest() != item['sha256']:
                raise RuntimeError('Image dependency checksum mismatch: '+key)
            target.mkdir(exist_ok=True)
            with tarfile.open(archive) as package:
                for member in package.getmembers():
                    parts = Path(member.name).parts
                    if len(parts)<2: continue
                    member.name = str(Path(*parts[1:]))
                    package.extract(member,target,filter='data')
            marker.write_text(item['sha256'],encoding='utf-8')
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    generator = ['-G','Visual Studio 17 2022','-A','x64'] if sys.platform == 'win32' else ['-G','Ninja']
    commands = [[cmake,'-S',str(ROOT),'-B',str(build),*generator,
        '-DBUILD_TESTING=OFF',f'-DCMAKE_BUILD_TYPE={args.configuration}',
        f'-DCYCODE_CORE_ROOT={core}',f'-DCYCODE_EDITOR_ROOT={editor}',f'-DCYCODE_DEPS={args.deps.resolve()}',
        f'-DCYUI_DIR={args.cyui or ""}',f'-DCYCODE_BUILD_DOCUMENTATION={"OFF" if args.without_documentation else "ON"}'],
        [cmake,'--build',str(build),'--config',args.configuration,'--parallel','4','--target','CyCode']]
    with (build/'build.log').open('w',encoding='utf-8') as stream:
        for command in commands:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT)
            stream.flush()
            if result.returncode:
                print((build/'build.log').read_text('utf-8',errors='replace')[-16000:]);return result.returncode
    print('CyCodeIDE built; no application or native test was executed. Log: '+str(build/'build.log'))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
