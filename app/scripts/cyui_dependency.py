"""Resolve CyCode's exact CyUI revision without modifying developer checkouts."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

APP = Path(__file__).resolve().parents[1]


def cache_directory(dependencies):
    lock = json.loads((APP / 'cyui-source.lock.json').read_text(encoding='utf-8'))
    return Path(dependencies) / ('cyui-' + lock['revision'][:12])


def resolve(destination, source=None):
    lock = json.loads((APP / 'cyui-source.lock.json').read_text(encoding='utf-8'))
    revision = lock['revision']
    if not re.fullmatch('[0-9a-f]{40}', revision):
        raise RuntimeError('CyUI must be pinned to a full commit hash')
    path = (source or destination).resolve()
    env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='Never')

    def git(*args):
        return subprocess.check_output(['git', '-c', f'safe.directory={path.as_posix()}',
                                       '-C', str(path), *args], env=env, text=True,
                                       stderr=subprocess.PIPE).strip()

    if not source and not path.exists():
        path.mkdir(parents=True)
        git('init', '--quiet')
        git('remote', 'add', 'origin', lock['repository'])
        git('fetch', '--quiet', '--depth=1', 'origin', revision)
        git('checkout', '--quiet', '--detach', revision)
    if git('rev-parse', 'HEAD') != revision:
        raise RuntimeError('CyUI checkout differs from app/cyui-source.lock.json; select a matching clean checkout')
    if git('status', '--porcelain', '--untracked-files=all'):
        raise RuntimeError('CyUI checkout contains local changes; keep them in a separate development checkout')
    return path


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--source', default='')
    args = parser.parse_args()
    try:
        print(resolve(args.destination, Path(args.source) if args.source else None))
    except (RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
