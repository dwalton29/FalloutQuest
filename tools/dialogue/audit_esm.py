#!/usr/bin/env python3
"""Read original ESM records for dialogue/AI auditing; never copies assets to APK."""
import argparse,collections,json,struct,zlib

def records(data,start=0,end=None,topic=0):
    end=len(data) if end is None else end
    while start<end:
        tag=data[start:start+4].decode('ascii');size=struct.unpack_from('<I',data,start+4)[0]
        if tag=='GRUP':
            kind=struct.unpack_from('<I',data,start+12)[0]
            child_topic=struct.unpack_from('<I',data,start+8)[0] if kind==7 else topic
            yield from records(data,start+24,start+size,child_topic);start+=size;continue
        flags,form=struct.unpack_from('<II',data,start+8)
        payload=data[start+24:start+24+size];start+=24+size
        if flags&0x40000:payload=zlib.decompress(payload[4:])
        subs=[];at=0;extended=0
        while at<len(payload):
            name=payload[at:at+4].decode('ascii');length=struct.unpack_from('<H',payload,at+4)[0];at+=6
            if name=='XXXX':extended=struct.unpack_from('<I',payload,at)[0];at+=length;continue
            if extended:length=extended;extended=0
            subs.append((name,payload[at:at+length]));at+=length
        yield {'type':tag,'id':form,'flags':flags,'topic':topic,'subs':subs}

def values(r,name):return [v for n,v in r['subs'] if n==name]
def first(r,name,default=b''):return next(iter(values(r,name)),default)
def text(b):return b.rstrip(b'\0').decode('cp1252')
def form(b):return struct.unpack_from('<I',b)[0] if len(b)>=4 else 0

def audit(path,actor):
    all_records=list(records(open(path,'rb').read()));indexed={r['id']:r for r in all_records}
    counts=collections.Counter(r['type'] for r in all_records);npc=indexed[actor]
    print('COUNTS',json.dumps({k:counts[k] for k in ['DIAL','INFO','NPC_','PACK','IDLE','NAVM','NAVI','FACT','CSTY']}))
    print('NPC',hex(actor),text(first(npc,'EDID')),text(first(npc,'FULL')))
    for field in ['AIDT','ACBS','CNAM','ZNAM','RNAM','VTCK','TPLT']:print(field,first(npc,field).hex())
    print('FACTIONS',[(hex(form(v)),int.from_bytes(v[4:5],'little',signed=True)) for v in values(npc,'SNAM')])
    for pkid in values(npc,'PKID'):
        r=indexed[form(pkid)];schedule=first(r,'PSDT');schedule_values=struct.unpack('<bbBbi',schedule) if len(schedule)==8 else None
        print('PACK',hex(r['id']),text(first(r,'EDID')),'PKDT',first(r,'PKDT').hex(),'SCHEDULE(month,weekday,date,hour,duration)',schedule_values)
        for field in ['PLDT','PLD2','PTDT','PTD2','CTDA','POBA','POEA','POCA','INAM','TNAM','SCDA','SCTX']:
            for v in values(r,field):print(' ',field,text(v) if field=='SCTX' else v.hex())
    speaker_infos=[r for r in all_records if r['type']=='INFO' and (form(first(r,'ANAM'))==actor or any(len(v)>=16 and struct.unpack_from('<H',v,8)[0]==72 and form(v[12:])==actor for v in values(r,'CTDA')))]
    print('DIRECT ACTOR INFOS',len(speaker_infos),'FUNCTIONS',dict(collections.Counter(struct.unpack_from('<H',v,8)[0] for r in speaker_infos for v in values(r,'CTDA') if len(v)>=12)))
    for r in speaker_infos:
        if r['id'] in [0x3da20,0x3b82,0x3b83,0x3b84,0x3da07]:
            print('INFO',hex(r['id']),'TOPIC',hex(r['topic']),'QUEST',hex(form(first(r,'QSTI'))),'DATA',first(r,'DATA').hex(),'CTDA',len(values(r,'CTDA')),'LINKS',[hex(form(v)) for v in values(r,'TCLT')])
            print(' RESULTS',[text(v) for v in values(r,'SCTX')])
    return indexed
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('esm');parser.add_argument('--actor',type=lambda v:int(v,0),default=0xa60)
    args=parser.parse_args();audit(args.esm,args.actor)
