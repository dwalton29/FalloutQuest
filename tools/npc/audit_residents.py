#!/usr/bin/env python3
"""Audit authored Megaton resident placement/package chains. No game assets copied."""
import collections,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'dialogue'))
from audit_esm import records,first,values,text,form
from audit_ai_combat import TYPES
data=Path(sys.argv[1]).read_bytes()
index={r['id']:r for r in records(data)}
residents=[]
def walk(at,end,world=0,cell=0):
    while at<end:
        tag=data[at:at+4];size=struct.unpack_from('<I',data,at+4)[0]
        if tag==b'GRUP':
            label,kind=struct.unpack_from('<II',data,at+8)
            walk(at+24,at+size,label if kind==1 else world,label if kind==6 else cell)
            at+=size
        else:
            ident=struct.unpack_from('<I',data,at+12)[0];r=index[ident]
            cellRecord=index.get(cell)
            if tag==b'ACHR' and cellRecord and (world==0xa74 or text(first(cellRecord,'EDID')).lower().startswith('megaton')):
                npc=index.get(form(first(r,'NAME')))
                if npc and npc['type']=='NPC_' and not r['flags']&0x820:
                    effective=npc;seen=set()
                    while form(first(effective,'TPLT')) and len(first(effective,'ACBS'))>=24 and struct.unpack_from('<H',first(effective,'ACBS'),22)[0]&16:
                        if effective['id'] in seen:break
                        seen.add(effective['id']);effective=index[form(first(effective,'TPLT'))]
                    chain=[]
                    for value in values(effective,'PKID'):
                        p=index[form(value)];kind=first(p,'PKDT')[4];schedule=first(p,'PSDT')
                        chain.append(dict(id=f'{p["id"]:08X}',editor=text(first(p,'EDID')),type=TYPES[kind] if kind<len(TYPES) else str(kind),
                            schedule=struct.unpack('<bbBbi',schedule) if len(schedule)==8 else None,
                            location=struct.unpack('<IIi',first(p,'PLDT')) if len(first(p,'PLDT'))==12 else None,
                            conditions=[struct.unpack_from('<H',v,8)[0] for v in values(p,'CTDA')],
                            scripted=any(v.strip(b'\0 \r\n') for v in values(p,'SCTX'))))
                    residents.append(dict(reference=f'{ident:08X}',base=f'{npc["id"]:08X}',name=text(first(npc,'FULL')),editor=text(first(npc,'EDID')),
                        cell=f'{cell:08X}',cellEditor=text(first(cellRecord,'EDID')),world=f'{world:08X}',packages=chain))
            at+=24+size
walk(0,len(data))
distribution=collections.Counter(p['type'] for actor in residents for p in actor['packages'])
print(json.dumps(dict(exterior=sum(a['world']=='00000A74' for a in residents),total=len(residents),packageEntries=dict(distribution),actors=residents),indent=2))
