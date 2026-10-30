"""Execute the production cached door lookup across a stale world context."""
from pathlib import Path
import argparse
import importlib.util
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--esm')
    args = parser.parse_args()
    source = (ROOT / 'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
    begin = source.index('bool ResolveDoorTeleportCachedQ1698(')
    brace = source.index('{', begin)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    production = source[begin:end]
    original = ''
    if args.esm:
        spec = importlib.util.spec_from_file_location('audit', ROOT / 'tools/dialogue/audit_esm.py')
        audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(audit)
        records = audit.records(Path(args.esm).read_bytes())
        door = next(r for r in records if audit.text(audit.first(r, 'EDID')).lower() == 'megmoriartysmaindoorexref')
        xtel = audit.first(door, 'XTEL')
        assert len(xtel) >= 28
        words = struct.unpack('<7I', xtel[:28])
        original = f'''{{Fo3DoorTeleport expected;expected.valid=true;expected.destinationDoorRefFormId={words[0]}u;
          uint32_t bits[]={{{','.join(str(v)+'u' for v in words[1:])}}};
          float* fields[]={{&expected.x,&expected.y,&expected.z,&expected.rx,&expected.ry,&expected.rz}};
          for(int i=0;i<6;++i)std::memcpy(fields[i],&bits[i],4);
          original[{door['id']}u]=expected;Fo3DoorTeleport actual;
          assert(ResolveDoorTeleportCachedQ1698({door['id']}u,true,actual));
          assert(actual.valid&&actual.destinationDoorRefFormId==expected.destinationDoorRefFormId&&actual.x==expected.x&&actual.z==expected.z);
          assert(ResolveDoorTeleportCachedQ1698({door['id']}u,false,actual));
          std::cout<<"Original Megaton saloon XTEL survives previous Wasteland scope\\n";}}'''
    harness = r'''
#include "world/fo3-cell-traversal.h"
#include <unordered_map>
#include <mutex>
#include <cassert>
#include <cstring>
#include <iostream>
#define Q6H_LOGI(...) ((void)0)
#define Q6H_LOGW(...) ((void)0)
std::unordered_map<uint32_t,Fo3DoorTeleport> original;
int genericCalls=0;
bool LookupFo3WastelandDoorTeleportQ1920(uint32_t id,Fo3DoorTeleport* out,bool* ready){
 *ready=true;*out={};if(id!=1)return false;out->valid=true;out->destinationDoorRefFormId=11;return true;
}
bool ResolveFo3DoorTeleportQ1700(uint32_t id,Fo3DoorTeleport* out){
 ++genericCalls;*out={};const auto found=original.find(id);if(found==original.end())return false;*out=found->second;return out->valid;
}
''' + production + r'''
int main(){
 Fo3DoorTeleport door;original[2].valid=true;original[2].destinationDoorRefFormId=22;
 assert(ResolveDoorTeleportCachedQ1698(1,true,door)&&door.destinationDoorRefFormId==11&&genericCalls==0);
 assert(ResolveDoorTeleportCachedQ1698(2,true,door)&&door.destinationDoorRefFormId==22&&genericCalls==1);
 assert(ResolveDoorTeleportCachedQ1698(2,false,door)&&door.destinationDoorRefFormId==22&&genericCalls==1);
 assert(!ResolveDoorTeleportCachedQ1698(3,true,door)&&!door.valid&&genericCalls==2);
 assert(!ResolveDoorTeleportCachedQ1698(3,false,door)&&genericCalls==2);
''' + original + '\nstd::cout<<"Production door lookup: indexed hit, cross-world fallback, load/swing classification and bounded caching passed\\n";\n}\n'
    with tempfile.TemporaryDirectory(prefix='fq-door-lookup-') as folder:
        folder = Path(folder)
        cpp, executable = folder / 'test.cpp', folder / 'test'
        cpp.write_text(harness)
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'app/src/main/cpp'), str(cpp), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

if __name__ == '__main__':
    main()
