"""Use Core's exact SDK pin, preferring a clean local source and preserving dirty SDKs."""
import json
import os
from pathlib import Path
import subprocess

def resolve(core, explicit=None):
    lock=json.loads((Path(core)/'app/cymcp-source.lock.json').read_text(encoding='utf-8'))
    local=Path(explicit or os.environ.get('CYCODE_CYMCP_SOURCE') or Path(__file__).resolve().parents[3]/'CyMCPSDK')
    if local.exists():
        source=local.resolve()
    elif explicit:
        raise FileNotFoundError('The selected SDK checkout is unavailable')
    else:
        source=Path(__file__).resolve().parents[2]/'runtime/deps'/('cymcp-'+lock['revision'])
        if not source.exists():
            source.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run(['git','clone','--no-checkout',lock['repository'],str(source)],check=True)
            subprocess.run(['git','-c','safe.directory='+source.as_posix(),'-C',str(source),'checkout','--detach',lock['revision']],check=True)
    git=['git','-c','safe.directory='+source.as_posix(),'-C',str(source)]
    observed=subprocess.check_output([*git,'rev-parse','HEAD'],text=True).strip()
    dirty=subprocess.check_output([*git,'status','--porcelain','--untracked-files=no'],text=True).strip()
    if observed!=lock['revision'] or dirty:
        raise ValueError('Preserve local SDK changes and select a clean checkout at '+lock['revision'])
    return source
