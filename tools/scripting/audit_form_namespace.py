#!/usr/bin/env python3
"""Read-only EDID metadata audit; does not execute scripts or copy game assets."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "dialogue"))
from audit_esm import records


def audit(path):
    data = Path(path).read_bytes()
    counts = collections.Counter()
    names = collections.Counter()
    for record in records(data):
        if record["flags"] & 0x20:
            continue
        counts["nondeleted_records"] += 1
        fields = [value for tag, value in record["subs"] if tag == "EDID"]
        if len(fields) > 1:
            counts["duplicate_edid_records"] += 1
        for value in fields:
            counts["edid_fields"] += 1
            if not value or value[-1] != 0 or b"\0" in value[:-1]:
                counts["malformed_edid_fields"] += 1
                continue
            name = value[:-1].lower()  # ASCII folding of original raw EDID bytes
            counts["empty_edids"] += not name
            names[name] += 1
    return {"esm_sha256": hashlib.sha256(data).hexdigest(),
            **{key: counts[key] for key in ("nondeleted_records", "edid_fields",
               "empty_edids", "duplicate_edid_records", "malformed_edid_fields")},
            "duplicate_normalized_names": sum(count > 1 for count in names.values()),
            "scope": "All nondeleted record classes; metadata only, not engine namespace semantics."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("esm")
    print(json.dumps(audit(parser.parse_args().esm), indent=2))
