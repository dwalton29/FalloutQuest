#!/usr/bin/env python3
"""Malformed SCDA envelopes must never be mistaken for validated bytecode."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
path = Path(__file__).resolve().parents[2] / 'tools/scripting/inventory_scda.py'
spec = importlib.util.spec_from_file_location('scda_inventory', path)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class EnvelopeTests(unittest.TestCase):
    def test_empty(self):
        self.assertEqual(m.decode_envelopes(b'', {0x10}), [])

    def test_dot_is_reference_index(self):
        data = struct.pack('<HHHH', 0x1c, 65535, 0x1021, 0)
        self.assertEqual(m.decode_envelopes(data, {0x1c, 0x1021}), [0x1c, 0x1021])

    def test_truncation_and_unknown(self):
        for data in (b'\x10', struct.pack('<HH', 0x10, 12), struct.pack('<HH', 0x9000, 0)):
            with self.assertRaises(ValueError):
                m.decode_envelopes(data, {0x10})

    def test_payload_not_rescanned_as_opcodes(self):
        data = struct.pack('<HHI', 0x15, 4, 0xdeadbeef)
        self.assertEqual(m.decode_envelopes(data, {0x15}), [0x15])


class FragmentRequirementTests(unittest.TestCase):
    def test_nested_queries_and_form_slots(self):
        from fragment_requirements import Dependencies
        # IF (reference.GetStage quest < 2) with a real argument payload.
        op = 0x103a
        query = b' r\x01\x00X' + struct.pack('<HHHcH', op, 5, 1, b'r', 2)
        expression = query + b' 2 <'
        payload = struct.pack('<HH', 1, len(expression)) + expression
        refs = [{'uses': set()}, {'uses': set()}]
        scan = Dependencies({op: {'name': 'GetStage', 'parameters': [{'type_id': 14}]}}, refs)
        scan.scan(struct.pack('<HH', 0x16, len(payload)) + payload, {0x16, op})
        self.assertFalse(scan.errors)
        self.assertEqual(scan.queries, {'GetStage'})
        self.assertEqual(scan.operators, {'<'})
        self.assertEqual([r['uses'] for r in refs], [{'expression scope'}, {'argument'}])

    def test_no_calls_in_strings_or_unknown_payloads(self):
        from fragment_requirements import Dependencies
        op = 0x1111
        scan = Dependencies({op: {'name': 'Example', 'parameters': [{'type_id': 0}]}}, [])
        string = b' X\x3a\x10\0\0'
        payload = struct.pack('<HH', 1, len(string)) + string
        line = struct.pack('<HH', op, len(payload)) + payload
        scan.scan(line + struct.pack('<HH', 0x1c, 7), {op, 0x1c})
        self.assertFalse(scan.queries)
        self.assertEqual(scan.errors, {'Reference:invalid reference slot 7'})

    def test_empty_slots_and_condition_scopes(self):
        from fragment_requirements import split_record, scoped_conditions
        r = {'type': 'QUST', 'id': 1, 'subs': [
            ('CTDA', b'quest'), ('INDX', b'\x0a\0'),
            ('QSDT', b'\0'), ('CTDA', b'other item'), ('SCHR', b'h'),
            ('QSDT', b'\0'), ('CTDA', b'own item'), ('SCHR', b'h'),
            ('SCDA', b'code'), ('SCRV', b'\x04\0\0\0'),
            ('QOBJ', b'\x02\0\0\0'), ('CTDA', b'objective')]}
        entries, gates = split_record(r)
        self.assertEqual([e['slot'] for e in entries], ['10/1'])
        self.assertEqual(entries[0]['refs'][0][0], 'SCRV')
        self.assertEqual(scoped_conditions('QUST', '10/1', gates, []), [('item', [b'own item'])])
        self.assertEqual(scoped_conditions('INFO', 'end', gates, gates['quest'])[-1],
                         ('parent_quest', [b'quest']))

    def test_repeated_gaps_and_rejected_previews(self):
        from fragment_requirements import read_probe, prepare_evidence
        k = 'SCPT:00000001:script'
        raw = [f'{k}@12\tdeferred_error\tunsupported command: Ada.Repair\n'] * 2
        raw += [f'{k}@16\tpreview_error\tQUST 2 stage 8: item 1: unsupported command: Foo\n']
        evidence = read_probe(raw)[k]
        gaps = set()
        prepare_evidence(evidence, {'repair':'Repair', 'foo':'Foo'}, gaps)
        self.assertEqual(gaps, {'unsupported command form/Repair'})
        self.assertEqual(evidence['blocks']['16']['preview_error'], ['unsupported command form/Foo'])

    def test_form_value_and_custom_message_substitutions(self):
        from fragment_requirements import Dependencies
        scan = Dependencies({0x11b9: {'name': 'ShowWarning', 'parameters': [{'type_id': 0}]}},
                            [{'uses':set()}])
        expr = b' Z\x01\x00'
        payload = b's\x01\0' + struct.pack('<H', len(expr)) + expr
        message = b'\x01\0\x02\0hi\x01\0s\x03\0'
        scan.scan(struct.pack('<HH', 0x15, len(payload)) + payload +
                  struct.pack('<HH', 0x11b9, len(message)) + message, {0x15, 0x11b9})
        self.assertFalse(scan.errors)
        self.assertEqual(scan.refs[0]['uses'], {'form ID value'})
        self.assertIn((0, 3, 's'), scan.locals)

    def test_event_listener_is_not_a_producer(self):
        from fragment_requirements import event_dispatch_sites
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'events.cpp'
            path.write_text('HasReferenceEvent(ref,"OnLoad"); // Event(ref,"OnHit")\n'
                            'Event(ref,entering?"OnTriggerEnter":"OnTriggerLeave",player);\n')
            sites = event_dispatch_sites(path.parent, ['onload', 'onhit', 'ontriggerenter', 'ontriggerleave'])
            self.assertEqual(sites['onload'], [])
            self.assertEqual(sites['onhit'], [])
            self.assertEqual(sites['ontriggerleave'], ['events.cpp:2'])


if __name__ == '__main__':
    unittest.main()
