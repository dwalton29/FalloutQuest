#!/usr/bin/env python3
"""Conservative, reproducible 24h Megaton PACK/XTEL audit of original Fallout3.esm.

This is a STATIC PROJECTION. Unknown CTDA, script context, live NAVM,
furniture availability and player choices must never be reported as executed.
"""
import argparse
import collections
import json
import math
import struct
import subprocess
import sys
import zlib
from collections import deque
from pathlib import Path

KINDS = {"Sleep", "Eat", "Travel", "Sandbox", "Wander", "Guard"}
REJECT = 0x820

def read32(data, off=0):
    return struct.unpack_from("<I", data, off)[0]

def subrecords(data):
    at = 0
    wide = 0
    while at < len(data):
        if at + 6 > len(data):
            raise ValueError("truncated subrecord")
        tag = bytes(data[at:at+4])
        length = struct.unpack_from("<H", data, at+4)[0]
        at += 6
        if tag == b"XXXX":
            if length != 4 or at + 4 > len(data):
                raise ValueError("invalid XXXX")
            wide = read32(data, at)
            at += 4
            continue
        if wide:
            length,wide = wide,0
        if at + length > len(data):
            raise ValueError("subrecord overflow")
        yield tag,data[at:at+length]
        at += length

def graph_and_references(data):
    """Resolve CELL/world ownership from actual GRUP ancestry, not editor names."""
    doors = set()
    refs = {}
    edges = {}
    action_packages = set()
    compiled_scripts = set()
    stack = []
    at = 0
    while at < len(data):
        while stack and at >= stack[-1][0]:
            stack.pop()
        if at + 24 > len(data):
            raise ValueError("truncated record header")
        tag = bytes(data[at:at+4])
        size = read32(data, at+4)
        if tag == b"GRUP":
            if size < 24 or at + size > len(data):
                raise ValueError("invalid group boundary")
            stack.append((at+size,read32(data,at+12),read32(data,at+8)))
            at += 24
            continue
        end = at + 24 + size
        if end > len(data):
            raise ValueError("record exceeds ESM")
        form = read32(data,at+12)
        flags = read32(data,at+8)
        if tag == b"DOOR":
            doors.add(form)
        elif tag == b"PACK":
            payload = data[at+24:end]
            if flags & 0x40000:
                payload = zlib.decompress(payload[4:])
            fields = list(subrecords(payload))
            if any(key in (b"INAM",b"TNAM") and len(value)==4 and read32(value)!=0
                   for key,value in fields):
                action_packages.add(form)
            if any(key==b"SCDA" and len(value)>0 for key,value in fields):
                compiled_scripts.add(form)
        elif tag == b"REFR":
            cell = next((entry[2] for entry in reversed(stack)
                         if entry[1] in (6,8,9,10)),0)
            world = next((entry[2] for entry in reversed(stack)
                          if entry[1] == 1),0)
            payload = data[at+24:end]
            if flags & 0x40000:
                payload = zlib.decompress(payload[4:])
            base = parent = destination = 0
            for key,value in subrecords(payload):
                if key == b"NAME" and len(value) == 4:
                    base = read32(value)
                elif key == b"XESP" and len(value) >= 5:
                    parent = read32(value)
                elif key == b"XTEL" and len(value) in (28,32):
                    destination = read32(value)
            refs[form] = (cell,world,base,flags,parent)
            if destination:
                edges[form] = destination
        at = end
    outgoing = collections.defaultdict(list)
    for src,dst in edges.items():
        a,b = refs.get(src),refs.get(dst)
        if not a or not b or not a[0] or not b[0]:
            continue
        if a[2] not in doors or b[2] not in doors:
            continue
        if (a[3]|b[3]) & REJECT or a[4] or b[4]:
            continue
        outgoing[a[0]].append((b[0],src))
    for links in outgoing.values():
        links.sort()
    return refs,outgoing,action_packages,compiled_scripts

def hops(graph,from_cell,to_cell):
    if from_cell == to_cell:
        return 0
    queue = deque([(from_cell,0)])
    visited = {from_cell}
    while queue:
        cell,n = queue.popleft()
        for next_cell,_ in graph.get(cell,()):
            if next_cell == to_cell:
                return n+1
            if next_cell not in visited:
                visited.add(next_cell)
                queue.append((next_cell,n+1))
    return None

def active(schedule,hour):
    if schedule is None:
        return "unknown"
    month,weekday,date,start,duration = schedule
    if month != -1 or weekday != -1 or date != 0:
        return "dated"
    if start < 0:
        return "active"
    if duration <= 0:
        return "inactive"
    return "active" if duration >= 24 or (hour-start)%24 < duration else "inactive"

def project(actor,hour,refs,graph,action_packages,compiled_scripts):
    prior = []
    for package in actor["packages"]:
        sched = active(package["schedule"],hour)
        if sched == "inactive":
            continue
        pkg = package["id"]
        if sched != "active":
            prior.append(pkg + ": unsupported " + sched + " schedule")
            continue
        if package["conditions"]:
            prior.append(pkg + ": CTDA functions " + str(package["conditions"]) +
                         " require live quest/actor context")
            continue
        if package["scripted"] or int(pkg,16) in action_packages or int(pkg,16) in compiled_scripts:
            prior.append(pkg + ": script or authored procedure action requires live execution")
            continue
        loc = package["location"]
        if package["type"] not in KINDS or not loc or loc[0] != 0:
            prior.append(pkg + ": unsupported off-scene procedure or PLDT")
            continue
        target = refs.get(loc[1])
        if not target or not target[0]:
            prior.append(pkg + ": destination REFR unavailable")
            continue
        steps = hops(graph,int(actor["cell"],16),target[0])
        if steps is None:
            prior.append(pkg + ": no XTEL route from original home CELL")
            continue
        uncertain = bool(prior)
        return {
            "status":"candidate_only" if uncertain else "schedule_candidate",
            "package":pkg,"type":package["type"],"editor":package["editor"],
            "target_cell":"%08X" % target[0],"topological_hops":steps,
            "preceding_uncertainties":prior[:8]
        }
    return {"status":"blocked_or_unproven","preceding_uncertainties":prior[:8]}

def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("esm")
    cli.add_argument("--output",default="megaton-24h.json")
    args = cli.parse_args()
    source = Path(__file__).with_name("audit_residents.py")
    census = json.loads(subprocess.check_output([sys.executable,str(source),args.esm]))
    refs,graph,action_packages,compiled_scripts = graph_and_references(Path(args.esm).read_bytes())
    summary = collections.Counter()
    rows = []
    for actor in census["actors"]:
        previous = None
        changes = []
        counts = collections.Counter()
        for hour in range(24):
            result = project(actor,hour,refs,graph,action_packages,compiled_scripts)
            counts[result["status"]] += 1
            summary[result["status"]] += 1
            identity = (result["status"],result.get("package"),
                        result.get("target_cell"),
                        tuple(result["preceding_uncertainties"]))
            if identity != previous:
                changes.append({"hour":hour,**result})
                previous = identity
        rows.append({
            "name":actor["name"],"reference":actor["reference"],
            "home_cell":actor["cell"],"package_count":len(actor["packages"]),
            "hourly_categories":dict(counts),"transitions":changes
        })
    rows.sort(key=lambda a:(a["name"],a["reference"]))
    result = {
        "scope":"Original Fallout3.esm Megaton (hourly STATIC PROJECTION ONLY)",
        "all_residents":len(rows),
        "exterior_residents":census["exterior"],
        "usable_directed_xtel_links":sum(map(len,graph.values())),
        "hourly_totals":dict(summary),"actors":rows,
        "warning":"Never interpret candidates as executed packages or successful NAVM/animation"
    }
    Path(args.output).write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:result[k] for k in
                      ("all_residents","exterior_residents","usable_directed_xtel_links","hourly_totals")},indent=2))

if __name__ == "__main__":
    main()
