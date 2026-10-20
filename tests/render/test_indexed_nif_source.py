"""v236 safety: original NIF indices, fallback, GPU ownership and submission."""
from pathlib import Path
import re
import unittest
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
C=(ROOT/"app/src/main/cpp/core/fo3-runtime.cpp").read_text()
class IndexedNifSource(unittest.TestCase):
    def test_actual_index_policy_native_lod_and_dynamic_fallbacks(self):
        policy = 'bool Q236CanIndexStaticNif(' + C.split('bool Q236CanIndexStaticNif(',1)[1].split('void Q236PrepareIndexedStaticNif(',1)[0]
        program = r'''
#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include <cassert>
using GLsizei = int;
struct CpuObject {
    struct { std::string baseRecordType; } placement;
    struct { bool skinned=false; std::vector<uint32_t> indices; } mesh;
    std::vector<int> positionsGame;
    bool q237ImmutableNativeLod=false;
};
''' + policy + r'''
int main() {
    CpuObject c; c.positionsGame.resize(4); c.mesh.indices={0,1,2,0,2,3};
    assert(!Q236CanIndexStaticNif(c)); // Unknown/dynamic provenance stays expanded.
    c.q237ImmutableNativeLod=true; assert(Q236CanIndexStaticNif(c));
    c.mesh.skinned=true; assert(!Q236CanIndexStaticNif(c)); c.mesh.skinned=false;
    c.mesh.indices[5]=4; assert(!Q236CanIndexStaticNif(c)); c.mesh.indices[5]=3;
    c.mesh.indices.pop_back(); assert(!Q236CanIndexStaticNif(c));
    c.mesh.indices={0,1,2}; assert(!Q236CanIndexStaticNif(c));
    c.q237ImmutableNativeLod=false; c.mesh.indices={0,1,2,0,2,3};
    for (const char* type : {"STAT", "SCOL", "TREE"}) {
        c.placement.baseRecordType=type; assert(Q236CanIndexStaticNif(c));
    }
    for (const char* type : {"NPC_", "CREA", "DOOR", "ACTI", "WEAP"}) {
        c.placement.baseRecordType=type; assert(!Q236CanIndexStaticNif(c));
    }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'policy.cpp'; path.write_text(program)
            binary=Path(tmp)/'policy'
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
        # Both synchronous and threaded original LOD admission paths opt in.
        self.assertEqual(C.count('cpu.q237ImmutableNativeLod = true;'),2)

    def test_static_only_and_vertex_semantics(self):
        body=C.split("bool Q236CanIndexStaticNif(",1)[1].split("void Q236PrepareIndexedStaticNif(",1)[0]
        for x in ('t=="STAT"','t=="SCOL"','t=="TREE"','!cpu.mesh.skinned','corners%3u==0u'):
            self.assertIn(x,body)
        prepare=C.split("void Q236PrepareIndexedStaticNif(",1)[1].split("bool PrepareExpandedVertexStreamQ1960(",1)[0]
        self.assertIn('for(size_t i=0;i<n;++i)',prepare)
        self.assertIn('for(uint32_t index:cpu.mesh.indices)',prepare)
        self.assertIn('cpu.q236IndexedPrepared=true;',prepare)
        old=C.split("bool PrepareExpandedVertexStreamQ1960(",1)[1].split("bool UploadCpuObject(",1)[0]
        self.assertIn("if(Q236CanIndexStaticNif(cpu))",old)
        self.assertIn("for (uint32_t index : cpu.mesh.indices)",old)
        self.assertIn('cpu.q236IndexedPrepared=false;',old)
        self.assertIn('gpu.vertexCount=q236Indexed?static_cast<GLsizei>(cpu.mesh.indices.size()):gpu.q236UniqueVertices;',C)
    def test_buffers_shared_and_lifetime_accounting(self):
        shared=C.split("struct Q2017SharedGeometryEntry",1)[1].split("// Q19: decoded DDS",1)[0]
        for token in ['shared.q236IndexBuffer=gpu.q236IndexBuffer','gpu.q236IndexBuffer=shared.q236IndexBuffer','if (shared.q236IndexBuffer) glDeleteBuffers','if (entry.second.q236IndexBuffer)glDeleteBuffers']:
            self.assertIn(token,shared)
        upload=C.split('bool UploadCpuObject(',1)[1].split('struct Q1050V3',1)[0]
        self.assertLess(upload.index('glBindVertexArray(gpu.vao);'),upload.index('glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.q236IndexBuffer);'))
        self.assertLess(upload.index('glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.q236IndexBuffer);'),upload.index('glBindVertexArray(0);'))
        self.assertIn('GL_UNSIGNED_INT',C)
        for func,end in [('Q74DeleteGpuObjects','bool gQ2013ExteriorWarmupPending'),('Q1900DeleteGpuShape','void Q1900CancelPending'),('Q1990DeleteLodGpu','void Q1990ClearNativeLodGeometry'),('Q210DeletePlayerBody','gQ210PlayerBody.clear()')]:
            part=C.split(('void '+func+'('),1)[1].split(end,1)[0]
            self.assertIn('q236IndexBuffer',part,func)
    def test_shadow_and_primary_draw_paths(self):
        shadow=C.split('bool Q1050UpdateSunShadow()',1)[1].split('struct Q1050',1)[0]
        self.assertIn('if(object.q236Indexed)glDrawElements(',shadow)
        body=C.split('void DrawSceneObject(',1)[1].split('bool Q2017EligibleForInstancing',1)[0]
        self.assertIn('glDrawElementsInstanced(GL_TRIANGLES,object.vertexCount',body)
        self.assertIn('glDrawElements(GL_TRIANGLES,object.vertexCount',body)
        self.assertIn('glDrawArraysInstanced(',body)
        self.assertIn('glDrawArrays(GL_TRIANGLES, 0, object.vertexCount)',body)
if __name__=='__main__':unittest.main()
