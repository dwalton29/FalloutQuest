#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Fo3NifSkinBone {
    std::string name;
    // Bind-pose bone origin in NIF model/game coordinates after the authored
    // NiNode hierarchy has been evaluated.
    float bindPosition[3]{0.0f, 0.0f, 0.0f};
};

struct Fo3StaticNifMesh {
    std::vector<float> positions;    // xyz in NIF model space after NIF transforms
    std::vector<float> normals;      // xyz
    std::vector<float> tangents;     // xyz
    std::vector<float> bitangents;   // xyz
    std::vector<float> texcoords;    // uv
    std::vector<float> vertexColors; // rgba, if authored
    std::vector<uint32_t> indices;   // non-degenerate GL_TRIANGLES list

    std::string modelPath;
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string glowTexturePath;
    // Fallout 3 BSShaderTextureSet slots recovered from the supplied Megaton NIFs:
    // 4 = EnvironmentCubeMap (e.g. textures\effects\Chrome_e.dds)
    // 5 = CustomEnvMask (material-specific *_m.dds).
    std::string environmentCubeTexturePath;
    std::string environmentMaskTexturePath;

    float specularColor[3]{1.0f, 1.0f, 1.0f};
    float emissiveColor[3]{0.0f, 0.0f, 0.0f};
    float glossiness = 10.0f;
    float alpha = 1.0f;
    float emissiveMult = 1.0f;
    float environmentMapScale = 1.0f;
    uint32_t shaderFlags1 = 0u;
    uint32_t shaderFlags2 = 0u;
    bool noLighting = false;
    bool alphaBlend = false;
    bool alphaTest = false;
    float alphaThreshold = 0.5f;
    // Gamebryo NiAlphaProperty bits 1-4 / 5-8. Defaults preserve the old
    // conventional SRC_ALPHA / ONE_MINUS_SRC_ALPHA path when no property exists.
    uint8_t alphaSourceBlend = 6u;
    uint8_t alphaDestBlend = 7u;

    // Fallout 3 BSShaderNoLightingProperty carries four view-angle falloff
    // floats after File Name (Bethesda NIF version >= 27). These modulate alpha.
    bool noLightingFalloff = false;
    float noLightingFalloffParams[4]{0.0f, 1.0f, 1.0f, 1.0f};

    // NiStencilProperty draw mode controls face winding / two-sided rendering.
    // Full stencil-buffer actions are intentionally not required for this first
    // visual-fidelity path; draw mode alone is authored independently of enable.
    bool stencilDrawModePresent = false;
    uint8_t stencilDrawMode = 0u;

    // Q21.1: legacy Gamebryo actor skinning. Four packed influences per source
    // vertex, indexing skinBones in NiSkinInstance order.
    bool skinned = false;
    std::vector<Fo3NifSkinBone> skinBones;
    std::vector<uint16_t> skinBoneIndices; // 4 per vertex, 0xffff = unused
    std::vector<float> skinBoneWeights;     // 4 per vertex
};

// Loads every fully renderable NiTriStrips/NiTriShape geometry block in an
// arbitrary Fallout 3 NIF, preserving each shape as its own material draw.
// Unsupported shapes are skipped without discarding the rest of the model.
bool LoadFo3StaticNifMeshes(const std::string& modelPath,
                            std::vector<Fo3StaticNifMesh>& outMeshes);

// Compatibility helper used by earlier milestones: returns the first supported
// renderable shape from the same generalized loader.
bool LoadFo3StaticNif(const std::string& modelPath, Fo3StaticNifMesh& outMesh);


// Q21.0 actor-skin diagnostic. This exposes the authored hierarchy needed to
// bootstrap a VR body without pretending static-NIF decoding is skinning.
struct Fo3NifSkinProbe {
    bool loaded = false;
    uint32_t blocks = 0;
    uint32_t nodes = 0;
    uint32_t skinInstances = 0;
    uint32_t skinDataBlocks = 0;
    uint32_t skinPartitions = 0;
    uint32_t referencedBones = 0;
    std::vector<std::string> nodeNames;
    std::vector<std::string> boneNames;
};

bool ProbeFo3NifSkin(const std::string& modelPath, Fo3NifSkinProbe& outProbe);
