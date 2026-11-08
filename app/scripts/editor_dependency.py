"""Resolve the pinned shared source library, or an explicit development checkout."""
import json
import os
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]

def _validate(path):
    if not (path/'editor/CMakeLists.txt').is_file() or (path/'editor/api-version.txt').read_text('utf-8').strip()!='1':
        raise RuntimeError('Expected the CyCodeReader shared editor source API version 1')
    return path

def ensure_editor(explicit=None):
    supplied=explicit or os.environ.get('CYCODE_EDITOR_ROOT')
    if supplied: return _validate(Path(supplied).resolve())
    lock=json.loads((ROOT/'app/editor-source.lock.json').read_text('utf-8'))
    revision=lock['revision']
    if not re.fullmatch('[0-9a-f]{40}',revision): raise RuntimeError('Shared editor source revision has not been pinned')
    target=ROOT/'runtime/deps/cycode-reader'
    env={**os.environ,'GIT_TERMINAL_PROMPT':'0','GCM_INTERACTIVE':'Never'}
    def git(*args,capture=False):
        result=subprocess.run(['git','-c','safe.directory='+target.as_posix(),'-C',str(target),*args],env=env,check=True,
            stdout=subprocess.PIPE if capture else None,text=True)
        return result.stdout.strip() if capture else None
    cloned=False
    if not target.exists():
        target.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['git','clone','--no-checkout','--filter=blob:none',lock['repository'],str(target)],env=env,check=True)
        cloned=True
    if not (target/'.git').exists() or git('remote','get-url','origin',capture=True)!=lock['repository']:
        raise RuntimeError('Unexpected shared editor cache; select a trusted checkout with --editor-root')
    head=subprocess.run(['git','-c','safe.directory='+target.as_posix(),'-C',str(target),'rev-parse','--verify','HEAD'],env=env,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
    if not cloned and git('status','--porcelain',capture=True):
        raise RuntimeError('Shared editor cache contains changes; use --editor-root for development')
    if cloned or head.returncode or head.stdout.strip()!=revision:
        # An uninitialized clone has only untracked source files after checkout;
        # any pre-existing modifications must remain untouched.
        git('fetch','--depth=1','origin',revision)
        git('checkout','--detach',revision)
    return _validate(target)
