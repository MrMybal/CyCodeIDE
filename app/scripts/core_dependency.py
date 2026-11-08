"""Resolve the native CyCode source dependency without downloading graphical sources."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
LOCK = ROOT / 'app/core-source.lock.json'
if not LOCK.is_file():
    LOCK = ROOT / 'core-source.lock.json'

def resolve(explicit=None):
    supplied = explicit or os.environ.get('CYCODE_CORE_ROOT')
    def validate(path):
        path = Path(path).resolve()
        if not (path / 'common/CMakeLists.txt').is_file() or (path / 'common/api-version.txt').read_text('utf-8').strip() != '1':
            raise RuntimeError('Expected CyCode common source API 1')
        return path
    if supplied:
        return validate(supplied)
    lock = json.loads(LOCK.read_text('utf-8'))
    revision = lock['revision']
    if not re.fullmatch('[0-9a-f]{40}', revision):
        raise RuntimeError('CyCode Core source revision must be pinned')
    target = ROOT / 'runtime/deps/cycode'
    env = {**os.environ, 'GIT_TERMINAL_PROMPT': '0', 'GCM_INTERACTIVE': 'Never'}
    def git(*args, capture=False):
        result = subprocess.run(['git', '-c', 'safe.directory='+target.as_posix(), '-C', str(target), *args],
            check=True, env=env, text=True, stdout=subprocess.PIPE if capture else None)
        return result.stdout.strip() if capture else None
    cloned = False
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'clone', '--no-checkout', '--filter=blob:none', lock['repository'], str(target)], check=True, env=env)
        cloned = True
    if not (target / '.git').exists() or git('remote', 'get-url', 'origin', capture=True) != lock['repository']:
        raise RuntimeError('Unexpected CyCode cache; supply an explicit trusted source')
    if not cloned and git('status', '--porcelain', capture=True):
        raise RuntimeError('Preserve modified CyCode cache; supply an explicit development source')
    if cloned or git('rev-parse', 'HEAD', capture=True) != revision:
        git('fetch', '--depth=1', 'origin', revision)
        git('checkout', '--detach', revision)
    return validate(target)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    args = parser.parse_args()
    print(resolve(args.source))
