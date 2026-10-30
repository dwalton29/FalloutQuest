#!/usr/bin/env python3
"""Verify/recover stealth GameSetting records from a supplied FO3 PE32 executable."""
import argparse, collections, json, math, re, struct
from pathlib import Path

def recover(path):
    data=path.read_bytes();pe=struct.unpack_from('<I',data,60)[0]
    assert data[:2]==b'MZ' and data[pe:pe+4]==b'PE\0\0'
    base=struct.unpack_from('<I',data,pe+52)[0]
    count,size=struct.unpack_from('<H',data,pe+6)[0],struct.unpack_from('<H',data,pe+20)[0]
    sections=[]
    for i in range(count):
        _,_,va,length,raw=struct.unpack_from('<8sIIII',data,pe+24+size+40*i)
        sections.append((base+va,raw,length))
    def offset(va):
        return next((raw+va-start for start,raw,n in sections if start<=va<start+n),None)
    def address(at):
        return next((va+at-raw for va,raw,n in sections if raw<=at<raw+n),None)
    candidates=[]
    pattern=rb'(?<=\x00)([fis](?:Sneak|Detection|CombatSearch|MoveSneak|SoundLevel)[A-Za-z0-9_]*)(?=\x00)'
    for match in re.finditer(pattern,data):
        key=match[1].decode();va=address(match.start(1))
        if va is None:continue
        for hit in re.finditer(re.escape(struct.pack('<I',va)),data):
            at=hit.start()
            if at<8 or at%4:continue
            vt=struct.unpack_from('<I',data,at-8)[0]
            if offset(vt) is None:continue
            candidates.append((key,at,vt))
    # All recovered GameSetting instances must share the same class vtable.
    vt=collections.Counter(row[2] for row in candidates).most_common(1)[0][0]
    values={};proof={}
    for key,at,table in candidates:
        if table!=vt:continue
        if key[0]=='s':
            value_at=offset(struct.unpack_from('<I',data,at-4)[0])
            if value_at is None:continue
            end=data.find(b'\0',value_at,value_at+512)
            if end<0:continue
            value=data[value_at:end].decode('cp1252')
        else:
            value=struct.unpack_from('<f' if key[0]=='f' else '<i',data,at-4)[0]
            if not math.isfinite(value) or abs(value)>1000000:continue
        values[key]=value;proof[key]={'record_file_offset':hex(at-8),'name_va':hex(struct.unpack_from('<I',data,at)[0]),'vtable_va':hex(vt)}
    return values,proof

def header(values):
    out='#pragma once\n#include <unordered_map>\n#include <string>\nnamespace fo3sneak {\n// Recovered native GameSetting records; Fallout3.esm overrides these defaults.\ninline std::unordered_map<std::string,float> NativeSettings(){return {\n'
    for k,v in sorted(values.items()):
        if k[0]!='s':out+=' {'+json.dumps(k)+','+repr(float(v))+'f},\n'
    out+='};}\ninline const char* Label(unsigned indicator){\n static constexpr const char* labels[]={'+','.join(json.dumps(values[k])for k in ['sSneakHidden','sSneakDetected','sSneakCaution','sSneakDanger'])+'};\n return labels[indicator<4?indicator:0];\n}\n}\n'
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--exe',type=Path,required=True);p.add_argument('--output',type=Path);p.add_argument('--check',type=Path);p.add_argument('--proof',type=Path);a=p.parse_args()
    values,proof=recover(a.exe);text=header(values)
    if a.check:assert a.check.read_text()==text,'Stealth defaults differ from original executable records'
    if a.output:a.output.write_text(text)
    if a.proof:a.proof.write_text(json.dumps(proof,indent=2)+'\n')
    print(f'Checked/recovered {len(values)} original stealth GameSettings')
if __name__=='__main__':main()
