#!/usr/bin/env python3
"""Check native fusion admission using original raw Wasteland LAND data.
Extracts the production builder/decoders; no game data is copied into the repo.
Texture readiness is assumed from resolved paths, not a DDS/upload validation.
"""
from pathlib import Path
import argparse, os, subprocess, tempfile
R=Path(__file__).resolve().parents[2];render=(R/'app/src/main/cpp/rendering/terrain/fo3-terrain-render.inc').read_text();stream=(R/'app/src/main/cpp/rendering/terrain/fo3-terrain-stream.inc').read_text();data=(R/'app/src/main/cpp/rendering/terrain/fo3-terrain-data.inc').read_text()
def decl(s,start):
 a=s.index(start);b=s.index('{',a) if start.startswith('struct ') else s.index(') {',a)+2;i=b+1;n=1
 while n:
  n+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]+';\n'
prefix=r'''
#include "rendering/terrain/fo3-terrain.h"
#include "rendering/terrain/terrain-layer-fusion.h"
#include "rendering/terrain/terrain-paint-coverage.h"
#include "data/fo3-esm-reader.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <iostream>
#include <cassert>
#define Q75_LOGI(...) ((void)0)
#define Q75_LOGE(...) ((void)0)
constexpr uint32_t HEADER_SIZE_Q75=24;
auto FileSizeQ75(FILE*f){return fo3esm::FileSize(f);}
bool ReadExactQ75(FILE*f,void*p,size_t n){return fo3esm::ReadExact(f,p,n);}
uint32_t ReadLe32Q75(const uint8_t*p){return fo3esm::ReadU32(p);}
uint16_t ReadLe16Q75(const uint8_t*p){return fo3esm::ReadU16(p);}
float ReadLeFloatQ75(const uint8_t*p){float v;std::memcpy(&v,p,4);return v;}
bool ReadPayloadQ75(FILE*f,uint32_t n,uint32_t flags,std::vector<uint8_t>& p){return fo3esm::ReadPayload(f,{uint64_t(ftello(f)),n,flags,"LAND"},p);}
template<class F>void WalkSubrecordsQ75(const uint8_t*p,size_t n,F f){std::vector<uint8_t> v(p,p+n);fo3esm::WalkSubrecords(v,f);}
#include "rendering/terrain/fo3-terrain-texture.cpp"
#include "rendering/terrain/fo3-terrain-material.cpp"
constexpr int Q76B_LAND_SIDE=33,Q76B_LAND_QUADS=32,Q76_HEIGHT_SIDE=33;
constexpr size_t Q76B_HEIGHT_COUNT=1089,Q76_HEIGHT_COUNT=1089;
constexpr float Q76B_CELL_SIZE=4096,Q76B_VERTEX_SPACING=128,Q711_TEXTURE_REPEAT_GAME_UNITS=512,Q718_ALPHA_EPS=.001f,Q76_HEIGHT_SCALE=8;
std::unordered_map<uint32_t,std::vector<float>> gLandVertexColorsQ1060;
// Raw Wasteland LAND only: no child-world parent substitution in this probe.
uint32_t GetFo3MaterialParentLandQ1270(uint32_t){return 0;}
'''
for d,start in [(data,'bool DecodeVhgtQ76('),(data,'bool DecodeVnmlQ1010('),(data,'bool DecodeVclrQ1060('),(data,'const std::vector<float>& GetFo3TerrainVertexColorsQ1060('),(render,'struct Q76BV3 {'),(render,'struct Q711CpuBatch {'),(stream,'struct Q2000CpuCell {'),(render,'void Q237PrepareTerrainIndices('),(render,'Q76BV3 Q76BToVr('),(render,'Q76BV3 Q76BSub('),(render,'Q76BV3 Q76BNormalize('),(render,'Q76BV3 Q76BCross('),(render,'void Q718PushTriangle('),(render,'int Q711QuadrantForQuad('),(stream,'bool Q2000BuildCpuCell(')]:prefix+=decl(d,start)
prefix+=r'''
int main(){
 FILE*f=std::fopen(fo3assets::FalloutMasterPath().c_str(),"rb");assert(f);const uint64_t size=fo3esm::FileSize(f);
 struct Group{uint64_t end;uint32_t label,type;};std::vector<Group> groups;std::unordered_map<uint32_t,std::array<int32_t,2>> grids;
 size_t cells=0,folded=0,paints=0,priorDraws=0,afterDraws=0;
 for(uint64_t at=0;at+24<=size;){while(!groups.empty()&&at>=groups.back().end)groups.pop_back();fseeko(f,at,SEEK_SET);uint8_t h[24];assert(fo3esm::ReadExact(f,h,24));const auto n=fo3esm::ReadU32(h+4),id=fo3esm::ReadU32(h+12),flags=fo3esm::ReadU32(h+8);
 if(!std::memcmp(h,"GRUP",4)){groups.push_back({at+n,flags,id});at+=24;continue;}
 const uint64_t end=at+24+n;bool world=false;uint32_t cell=0;for(const auto& g:groups){if(g.type==1)world=g.label==0x3c;if(g.type==6)cell=g.label;}
 if(world&&(!std::memcmp(h,"CELL",4)||!std::memcmp(h,"LAND",4))){std::vector<uint8_t> payload;assert(fo3esm::ReadPayload(f,{at+24,n,flags,"LAND"},payload));
 if(!std::memcmp(h,"CELL",4)){fo3esm::WalkSubrecords(payload,[&](const char*t,const uint8_t*p,uint32_t len){if(!std::memcmp(t,"XCLC",4)&&len>=8)grids[id]={int32_t(fo3esm::ReadU32(p)),int32_t(fo3esm::ReadU32(p+4))};});}
 else if(grids.count(cell)){const auto grid=grids.at(cell);if((std::abs(grid[0]-1)<=1&&std::abs(grid[1]+7)<=1)||(std::abs(grid[0]-19)<=1&&std::abs(grid[1]+18)<=1)){
 Fo3TerrainCellQ76 land;land.cellFormId=cell;land.landFormId=id;land.gridX=grid[0];land.gridY=grid[1];fo3esm::WalkSubrecords(payload,[&](const char*t,const uint8_t*p,uint32_t len){if(!std::memcmp(t,"VHGT",4))DecodeVhgtQ76(p,len,land.heights);if(!std::memcmp(t,"VNML",4))DecodeVnmlQ1010(p,len,land.normals);if(!std::memcmp(t,"VCLR",4))DecodeVclrQ1060(p,len,gLandVertexColorsQ1060[id]);});
 Q2000CpuCell output;assert(Q2000BuildCpuCell(land,land.heights,land.gridX*4096.f,land.gridY*4096.f,0,0,-1,70,output));auto ready=[](const auto& b){return !b.texturePath.empty();};
 fo3terrain::RemoveCoveredPaint(output.batches,ready);size_t before=0;for(auto& b:output.batches)before+=!b.vertices.empty();const auto stats=fo3terrain::FuseLayers(output.batches,ready);size_t after=0;for(auto& b:output.batches)after+=!b.vertices.empty();
 std::cout<<"Authored LAND ("<<grid[0]<<","<<grid[1]<<") groups="<<stats.groups<<" folded="<<stats.triangles<<" fallback="<<stats.fallback<<" draws="<<before<<"->"<<after<<"\n";
 assert(after<=before);++cells;folded+=stats.triangles;paints+=stats.paintTriangles;priorDraws+=before;afterDraws+=after;
 }}}
 at=end;
 }
 std::fclose(f);assert(cells>0);std::cout<<"TOTAL cells="<<cells<<" folded="<<folded<<" paintTriangles="<<paints<<" draws="<<priorDraws<<"->"<<afterDraws<<"\n";
}
'''
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--esm',type=Path,required=True)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        work=Path(directory);cpp=work/'admission.cpp';cpp.write_text(prefix)
        (work/'Fallout3.esm').symlink_to(args.esm.resolve())
        executable=work/'admission'
        subprocess.run(['g++','-O2','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function',
                        '-I'+str(R/'app/src/main/cpp'),'-I'+str(R/'app/src/main/cpp/data'),str(cpp),
                        str(R/'app/src/main/cpp/data/fo3-esm-reader.cpp'),'-lz','-o',str(executable)],check=True)
        subprocess.run([str(executable)],env={**os.environ,'FALLOUTQUEST_DATA_ROOT':str(work)},check=True)
