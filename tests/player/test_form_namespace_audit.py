"""Synthetic metadata tests for the read-only original form-name audit."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec = importlib.util.spec_from_file_location("form_audit", Path(__file__).resolve().parents[2] / "tools/scripting/audit_form_namespace.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def sub(tag, data):
    return tag + struct.pack("<H", len(data)) + data


def record(tag, form, payload, flags=0):
    if flags & 0x40000:
        payload = struct.pack("<I", len(payload)) + zlib.compress(payload)
    return tag + struct.pack("<IIIII", len(payload), flags, form, 0, 0) + payload


class NamespaceAuditTests(unittest.TestCase):
    def audit(self, data):
        with tempfile.TemporaryDirectory(prefix="fq-form-audit-") as directory:
            path = Path(directory) / "fixture.esm"
            path.write_bytes(data)
            return module.audit(path)

    def test_empty_record(self):
        result = self.audit(record(b"ZZZZ", 1, b""))
        self.assertEqual((result["nondeleted_records"], result["edid_fields"]), (1, 0))

    def test_cross_class_case_collision_and_compression(self):
        result = self.audit(record(b"STAT", 1, sub(b"EDID", b"Name\0")) +
                            record(b"ZZZZ", 2, sub(b"EDID", b"nAmE\0"), 0x40000))
        self.assertEqual(result["duplicate_normalized_names"], 1)
        self.assertEqual(result["malformed_edid_fields"], 0)

    def test_deleted_alias_does_not_participate(self):
        result = self.audit(record(b"STAT", 1, sub(b"EDID", b"Name\0")) +
                            record(b"ZZZZ", 2, sub(b"EDID", b"NAME\0"), 0x20))
        self.assertEqual((result["nondeleted_records"], result["duplicate_normalized_names"]), (1, 0))

    def test_duplicate_and_malformed_fields(self):
        payload = b"".join(sub(b"EDID", value) for value in (b"Name\0", b"Name\0", b"", b"Name", b"N\0tail\0"))
        result = self.audit(record(b"STAT", 1, payload))
        self.assertEqual((result["duplicate_edid_records"], result["malformed_edid_fields"], result["duplicate_normalized_names"]), (1, 3, 1))

    def test_empty_name_is_reported(self):
        self.assertEqual(self.audit(record(b"STAT", 1, sub(b"EDID", b"\0")))["empty_edids"], 1)


if __name__ == "__main__":
    unittest.main()
