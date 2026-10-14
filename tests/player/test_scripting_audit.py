#!/usr/bin/env python3
"""Malformed SCDA envelopes must never be mistaken for validated bytecode."""
import importlib.util
from pathlib import Path
import struct
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


if __name__ == '__main__':
    unittest.main()
