"""Inert host catalogue checks: no app, SDK server, CLI or native fixture is run."""
import json
from pathlib import Path
import tempfile
import unittest
from stage_runtime import stage

APP=Path(__file__).resolve().parents[1]

class AutomationContracts(unittest.TestCase):
    def test_ui_mutations_have_explicit_permissions(self):
        ops=json.loads((APP/'ide-operations.json').read_text('utf-8'))['operations']
        self.assertEqual(len({op['id'] for op in ops}),8)
        for op in ops:
            self.assertTrue(op['nativeMethod'].startswith('ide.'))
            self.assertFalse(op['inputSchema']['additionalProperties'])
            if op['id'] in {'ide_state','ide_commands','ide_selection'}:
                self.assertEqual(op['permissions'],[])
            else:self.assertTrue(op['permissions'])
        close=next(op for op in ops if op['id']=='ide_close_document')
        self.assertIn('expectedRevision',close['inputSchema']['required'])
        preferences=next(op for op in ops if op['id']=='ide_editor_preferences')
        self.assertEqual(preferences['permissions'],['settings.write'])

if __name__=='__main__':unittest.main()
