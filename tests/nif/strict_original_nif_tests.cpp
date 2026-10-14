#include <cassert>
#include <fstream>
#include <iostream>
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "../../app/src/main/cpp/world/fo3-resident-resources.h"

// Optional original-file integration. Corruption is in memory only; neither
// the supplied NIF nor proprietary fixture bytes are written to the repository.
static std::vector<uint8_t> fixture;
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved) {
    bytes=fixture;if(resolved)*resolved=path;return !bytes.empty();
}
int main(int argc,char** argv) {
    if(argc!=2)return 77;
    std::ifstream file(argv[1],std::ios::binary);
    fixture.assign(std::istreambuf_iterator<char>(file),{});assert(!fixture.empty());
    std::vector<Fo3StaticNifMesh> complete;
    assert(LoadFo3StaticNifMeshes("original-fixture.nif",complete,true)&&complete.size()>1);
    NifHeader header;assert(ParseHeader(fixture,header));
    const auto original=fixture;
    fo3scene::ResidentResourceSelection selection;
    Fo3WorldPlacement placement;placement.refFormId=1;placement.baseFormId=2;placement.modelPath="original-fixture.nif";
    selection.incomingPlacements.push_back(placement);
    std::atomic<bool> cancelled{false};std::string error;
    auto build=[](const auto& p,auto& parts){return LoadFo3StaticNifMeshes(p.modelPath,parts,true);};
    auto skip=[](const auto&){return false;};
    std::vector<Fo3StaticNifMesh> prepared;
    assert(fo3scene::PrepareResidentCpuParts(selection,prepared,build,skip,cancelled,error));
    assert(prepared.size()==complete.size());
    bool partialRejected=false;
    for(const auto& mesh:complete) {
        ShapeObject shape;
        assert(ParseShapeObject(BlockData(original,header,mesh.shapeBlock),
            header.blockSizes[mesh.shapeBlock],header,shape));
        assert(shape.dataRef<header.numBlocks);
        fixture=original;
        const size_t offset=header.blockOffsets[shape.dataRef];
        std::fill(fixture.begin()+offset,fixture.begin()+offset+header.blockSizes[shape.dataRef],0xff);
        std::vector<Fo3StaticNifMesh> permissive,strict;
        if(!LoadFo3StaticNifMeshes("damaged-fixture.nif",permissive)||permissive.empty())continue;
        assert(permissive.size()<complete.size());
        assert(!LoadFo3StaticNifMeshes("damaged-fixture.nif",strict,true)&&strict.empty());
        prepared.clear();prepared.emplace_back();prepared[0].modelPath="retained-output";
        assert(!fo3scene::PrepareResidentCpuParts(selection,prepared,build,skip,cancelled,error));
        assert(prepared.size()==1&&prepared[0].modelPath=="retained-output");
        partialRejected=true;break;
    }
    assert(partialRejected);
    fixture=original;
    assert(LoadFo3StaticNifMeshes("original-fixture.nif",complete,true));
    std::cout<<"Original multi-part NIF complete decode passes; partial selected geometry rejects in candidate mode while ordinary mode remains available\n";
}
