"""Execute the production sliced LAND uploader against a byte-recording GL driver."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RENDER = (ROOT/'app/src/main/cpp/rendering/terrain/fo3-terrain-render.inc').read_text()
STREAM = (ROOT/'app/src/main/cpp/rendering/terrain/fo3-terrain-stream.inc').read_text()

def declaration(source, start):
    return start + source.split(start, 1)[1].split('};', 1)[0] + '};\n'

class TerrainUpload(unittest.TestCase):
    def test_indexed_and_fallback_buffers_survive_arbitrary_chunk_boundaries(self):
        prefix = r'''
#include "rendering/terrain/fo3-terrain-bounds.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
using GLuint=unsigned; using GLenum=unsigned; using GLsizei=int;
using GLsizeiptr=std::ptrdiff_t; using GLintptr=std::ptrdiff_t;
constexpr unsigned GL_ARRAY_BUFFER=1,GL_ELEMENT_ARRAY_BUFFER=2,GL_STATIC_DRAW=3,GL_FLOAT=4;
constexpr bool GL_FALSE=false;
GLuint arrayBuffer=0,vao=0,nextId=100;
std::map<GLuint,std::vector<uint8_t>> bytes;
std::map<GLuint,GLuint> elements;
void glGenBuffers(int,GLuint* p){*p=++nextId;}
void glGenVertexArrays(int,GLuint* p){*p=++nextId;}
void glBindVertexArray(GLuint v){vao=v;}
void glBindBuffer(GLenum target,GLuint v){if(target==GL_ARRAY_BUFFER)arrayBuffer=v;else elements[vao]=v;}
void glBufferData(GLenum,GLsizeiptr size,const void*,GLenum){bytes[arrayBuffer].resize(size,0xcd);}
void glBufferSubData(GLenum,GLintptr offset,GLsizeiptr size,const void* data){
    assert(offset>=0 && size>0 && size<=17);
    assert(size_t(offset+size)<=bytes[arrayBuffer].size());
    std::memcpy(bytes[arrayBuffer].data()+offset,data,size);
}
void glVertexAttribPointer(unsigned,int,GLenum,bool,GLsizei,const void*){}
void glEnableVertexAttribArray(unsigned){}
#define Q76B_LOGW(...) ((void)0)
constexpr size_t Q2000_GPU_BYTES_PER_UPDATE=17;
constexpr uint64_t Q2000_GPU_SPIKE_US=8000;
uint64_t Q2000ElapsedUs(std::chrono::steady_clock::time_point){return 1;}
struct Q2000CpuCell{int32_t gridX=1,gridY=2;};
'''
        prefix += declaration(RENDER, 'struct Q711TerrainBatch : fo3terrain::Bounds {')
        prefix += declaration(RENDER, 'struct Q711CpuBatch {')
        prefix += declaration(STREAM, 'struct Q2000VboUploadState {')
        prefix += r'''
Q2000VboUploadState gQ2000VboUpload;
uint64_t gQ2000TargetMaxGpuStepUs=0;
std::unordered_map<std::string,GLuint> q1910TerrainTextureCache{{"paint",11}};
std::unordered_map<std::string,GLuint> q1810TerrainNormalTextureCache{{"normal",12}};
std::vector<Q711TerrainBatch> gQ2000CurrentCellBatches;
'''
        upload = 'bool Q2000AdvanceOneBatch(' + STREAM.split('bool Q2000AdvanceOneBatch(',1)[1].split('void Q2000PublishCurrentCell(',1)[0]
        main = r'''
int main(){
    for(bool indexed:{false,true}){
        Q711CpuBatch cpu;cpu.texturePath="paint";cpu.normalPath="normal";
        cpu.vertices.resize(36);for(size_t i=0;i<cpu.vertices.size();++i)cpu.vertices[i]=float(i)/10;
        if(indexed)cpu.indices={0,1,2,0,2,1};
        Q2000CpuCell cell;bool complete=false;size_t steps=0;
        while(!complete){size_t used=0;complete=Q2000AdvanceOneBatch(cell,cpu,used);assert(used<=17);assert(++steps<100);}
        assert(!gQ2000VboUpload.active && !gQ2000VboUpload.vbo);
        const auto& gpu=gQ2000CurrentCellBatches.back();
        const size_t vertexBytes=cpu.vertices.size()*sizeof(float);
        assert(gpu.vertexCount==(indexed?6:3));
        assert(gpu.indexOffset==(indexed?vertexBytes:0));
        const auto& buffer=bytes[gpu.vbo];
        assert(buffer.size()==vertexBytes+cpu.indices.size()*sizeof(uint32_t));
        assert(std::memcmp(buffer.data(),cpu.vertices.data(),vertexBytes)==0);
        if(indexed){
            assert(elements[gpu.vao]==gpu.vbo);
            assert(std::memcmp(buffer.data()+vertexBytes,cpu.indices.data(),cpu.indices.size()*sizeof(uint32_t))==0);
        }else assert(elements[gpu.vao]==0);
    }
    // Unresolved paint is skipped; it must not publish an untextured layer.
    Q711CpuBatch missing;missing.alphaLayer=true;missing.vertices.resize(36);
    size_t used=0;Q2000CpuCell cell;
    assert(Q2000AdvanceOneBatch(cell,missing,used));
    assert(gQ2000CurrentCellBatches.size()==2 && used==0);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp=Path(tmp)/'upload.cpp';cpp.write_text(prefix+upload+main)
            binary=Path(tmp)/'upload'
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'app/src/main/cpp'),str(cpp),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__=='__main__':unittest.main()
