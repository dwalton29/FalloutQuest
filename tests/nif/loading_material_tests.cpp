#include <cassert>
#include <fstream>
#include <iostream>
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"

// Host-only file reader; no proprietary game assets are shipped with the tests.
bool LoadFalloutMeshFile(const std::string& path, std::vector<uint8_t>& bytes,
                         std::string* resolved) {
    std::ifstream file(path, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(file), {});
    if (resolved) *resolved = path;
    return !bytes.empty();
}

static void U32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
}
int main(int argc, char** argv) {
    // Morph vectors must follow the same scaled/rotated ancestor chain as
    // geometry, while excluding translations.
    Fo3StaticNifMesh geometry;
    geometry.positions = {1,2,3}; geometry.normals = {0,0,1};
    geometry.tangents = {1,0,0}; geometry.bitangents = {0,1,0};
    NifTransform shape; shape.valid = true; shape.scale = 2;
    shape.translation[0] = 50;
    NifTransform parent; parent.valid = true; parent.scale = 3;
    const float rotated[9] = {0,-1,0, 1,0,0, 0,0,1};
    std::copy(rotated,rotated+9,parent.rotation);
    parent.translation[2] = 20;
    ApplyTransforms(geometry,shape,{parent},nullptr);
    assert(geometry.positions[0] == -12 && geometry.positions[1] == 156 &&
           geometry.positions[2] == 38);
    assert(geometry.geometryDeltaToModel[0] == 0 &&
           geometry.geometryDeltaToModel[1] == -6 &&
           geometry.geometryDeltaToModel[3] == 6 &&
           geometry.geometryDeltaToModel[8] == 6);
    NifHeader header; header.userVersion = 11; header.bsVersion = 34;
    std::vector<uint8_t> property;
    U32(property, INVALID_REF); U32(property, 0); U32(property, INVALID_REF);
    property.push_back(0); property.push_back(0); // NiShadeProperty flags
    U32(property, 1); U32(property, 0); U32(property, 1);
    U32(property, 0x3f800000); U32(property, 0); // environment scale, clamp
    const std::string expected = "textures\\interface\\main\\main_timer.dds";
    U32(property, static_cast<uint32_t>(expected.size()));
    property.insert(property.end(), expected.begin(), expected.end());
    std::string path;
    assert(ParseTileShaderProperty(property.data(), property.size(), header, path));
    assert(path == expected);
    for (size_t length = 0; length < property.size(); ++length) {
        path = "unchanged";
        assert(!ParseTileShaderProperty(property.data(), length, header, path));
        assert(path == "unchanged");
    }
    auto malformed = property;
    malformed[34] = 0xff; malformed[35] = 0xff;
    assert(!ParseTileShaderProperty(malformed.data(), malformed.size(), header, path));
    property.push_back(0);
    assert(!ParseTileShaderProperty(property.data(), property.size(), header, path));

    // Optional full integration with the user's extracted originals.
    for (int i = 1; i < argc; ++i) {
        std::vector<Fo3StaticNifMesh> meshes;
        assert(LoadFo3StaticNifMeshes(argv[i], meshes));
        const bool compass = std::string(argv[i]).find("loadinganim01") == std::string::npos;
        assert(meshes.size() == (compass ? 2u : 8u));
        for (const auto& mesh : meshes) {
            assert(mesh.noLighting && mesh.alphaBlend);
            assert(!mesh.diffuseTexturePath.empty());
            if (compass) assert(mesh.diffuseTexturePath == expected);
            else assert(mesh.diffuseTexturePath.find("textures\\interface\\loading\\") == 0);
        }
        std::cout << argv[i] << ": " << meshes.size() << " textured UI shapes\n";
    }
    std::cout << "Loading material tests passed\n";
}
