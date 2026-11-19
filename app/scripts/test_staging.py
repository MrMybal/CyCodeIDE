"""Exercise the distribution boundary with inert source/data fixtures only."""
from pathlib import Path
import tempfile
import json
import unittest
from stage_runtime import stage

class StagingTests(unittest.TestCase):
    def test_owned_resources_without_user_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);core=root/'core';ide=root/'ide';dest=root/'distribution'
            def fixture(path,value='owned resource'):
                path.parent.mkdir(parents=True,exist_ok=True);path.write_text(value,encoding='utf-8')
            for name in ['CyTool.json','contracts.py','tool.py','requirements.txt','native_sdk.py','native_client.py','sdk_cli.py','core_host.py','THIRD_PARTY_NOTICES.md']:
                fixture(core/'app'/name)
            fixture(core/'app/scripts/migrate_legacy_jobs.py')
            fixture(core/'app/cymcp-source.lock.json','{}')
            fixture(core/'app/MCPMethods.json',json.dumps({'operations':[{'id':'fixture.operation','permissions':['workspace.read']}]}))
            fixture(ide/'app/ide-operations.json',json.dumps({'operations':[]}))
            fixture(core/'app/CyTool.json',json.dumps({'id':'cy.code','operations':[{'id':'fixture.operation','permissions':['workspace.read']}]}))
            fixture(ide/'app/THIRD_PARTY_NOTICES.md','IDE graphical dependency notices')
            fixture(core/'app/THIRD_PARTY_NOTICES.md','Core native dependency notices')
            fixture(core/'app/licenses/native.txt','native license')
            fixture(ide/'app/licenses/chat.txt','chat license')
            fixture(ide/'app/locales/ui.json')
            fixture(ide/'app/resources/fonts/font-notice.txt')
            fixture(ide/'icons/CyCode.png')
            fixture(core/'runtime/plugins/git-private.json','private plugin settings')
            fixture(core/'runtime/plugins/unreal.json')
            fixture(core/'data/settings/user.json','private state')
            fixture(ide/'data/workspaces/session.json','private session')
            fixture(core/'app/secret.env','private environment')
            cli=root/'cli-fixture.txt';fixture(cli)
            stage(core,ide,dest,cli,'CyCodeIDE')
            source=json.loads((core/'app/CyTool.json').read_text(encoding='utf-8'))
            host=json.loads((dest/'app/CyTool.json').read_text(encoding='utf-8'))
            self.assertEqual(host.pop('name'),'CyCodeIDE')
            self.assertEqual(host.pop('standaloneExecutable'),'CyCodeIDE')
            self.assertEqual(host,source)
            with self.assertRaises(ValueError):
                stage(core,ide,dest,cli,'../other-app')
            files={p.relative_to(dest).as_posix() for p in dest.rglob('*') if p.is_file()}
            self.assertEqual((dest/'app/THIRD_PARTY_NOTICES.md').read_text(encoding='utf-8'),'IDE graphical dependency notices')
            self.assertEqual((dest/'app/CORE_THIRD_PARTY_NOTICES.md').read_text(encoding='utf-8'),'Core native dependency notices')
            self.assertIn('icons/CyCode.png',files)
            self.assertIn('app/locales/ui.json',files)
            self.assertIn('runtime/plugins/unreal.json',files)
            self.assertIn('cli-fixture.txt',files)
            self.assertIn('app/licenses/native.txt',files)
            self.assertIn('app/licenses/chat.txt',files)
            self.assertEqual((dest/'app/MCPMethods.json').read_bytes(),(core/'app/MCPMethods.json').read_bytes())
            fixture(ide/'app/ide-operations.json',json.dumps({'operations':[{'id':'ide_fixture','nativeMethod':'ide.fixture'}]}))
            stage(core,ide,dest,cli,'CyCodeIDE')
            catalogue=json.loads((dest/'app/IDEAutomationMethods.json').read_text('utf-8'))
            self.assertEqual(catalogue['operations'][0],source['operations'][0])
            self.assertEqual(catalogue['operations'][-1]['id'],'ide_fixture')
            fixture(ide/'app/ide-operations.json',json.dumps({'operations':[{'id':'fixture.operation','nativeMethod':'ide.fixture'}]}))
            with self.assertRaises(ValueError):stage(core,ide,dest,cli,'CyCodeIDE')
            self.assertFalse(any('settings' in p or 'session' in p or 'secret' in p or 'git-private' in p for p in files))
            self.assertFalse(any('private' in p.read_text(encoding='utf-8') for p in dest.rglob('*') if p.is_file()))

if __name__=='__main__':
    unittest.main()
