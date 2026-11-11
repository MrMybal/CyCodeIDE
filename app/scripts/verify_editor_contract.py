"""Check required presentation capabilities in canonical dependency contracts."""
import argparse
import json
from pathlib import Path

def verify(core, editor):
    methods=json.loads((core/'app/MCPMethods.json').read_text(encoding='utf-8'))
    available={item['nativeMethod'] for item in methods['operations']}
    required={
        'analysis.prepare','analysis.status','analysis.results','analysis.checkpoints','analysis.inspect',
        'documents.readPage','documents.readLines','documents.prepareByteEdit',
        'documents.savePaged','documents.savePagedAs','documents.closePaged',
        'documents.checkpointPaged','documents.pagedCheckpoints','documents.previewPagedCheckpoint',
        'documents.restorePagedCheckpoint','changes.preview','changes.apply','changes.undo',
        'changes.historyStatus','changes.historyPrepare','changes.status',
    }
    missing=required-available
    if missing:
        raise RuntimeError('Core lacks required native capabilities: '+', '.join(sorted(missing)))
    for name in ['PagedTextBackend.h','PagedTextEditor.h']:
        if not (editor/'editor/include/cycode'/name).is_file():
            raise RuntimeError('The selected Reader revision predates the shared Engine-backed paged editor. '
                'Resolve app/editor-source.lock.json or select a compatible --editor-root checkout.')
    print('Canonical Core capabilities and shared editor headers verified')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core',type=Path,required=True)
    parser.add_argument('--editor',type=Path,required=True)
    args=parser.parse_args()
    verify(args.core,args.editor)
