#pragma once
#include "fo3-resident-textures.h"
#include <GLES3/gl3.h>

namespace fo3scene {
// The caller restores bindings/pixel-store state. No cache or decode access.
inline bool UploadResidentImage(const Fo3RgbaTexture& image,
        ResidentTextureOwnership& owner, GLuint& id, void(*anisotropy)()) {
    id=0;size_t bytes=0;
    if(!ValidResidentImage(image,bytes))return false;
    glGenTextures(1,&id);
    if(!id)return false;
    glBindTexture(GL_TEXTURE_2D,id);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    if(anisotropy)anisotropy();
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,
                 GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    if(glGetError()!=GL_NO_ERROR||!owner.Adopt(id)) {
        glDeleteTextures(1,&id);id=0;return false;
    }
    return true;
}
inline bool UploadResidentCube(const Fo3RgbaCubeTexture& cube,
        ResidentTextureOwnership& owner,GLuint& id) {
    id=0;size_t bytes=0;
    if(!ValidResidentCube(cube,bytes))return false;
    glGenTextures(1,&id);
    if(!id)return false;
    glBindTexture(GL_TEXTURE_CUBE_MAP,id);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,
                    cube.mipLevels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAX_LEVEL,cube.mipLevels-1);
    for(int face=0;face<6;++face) {
        int w=cube.width;
        for(int mip=0;mip<cube.mipLevels;++mip) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,mip,GL_RGBA8,w,w,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,cube.rgbaLevels[face][mip].data());
            w=std::max(1,w>>1);
        }
    }
    if(glGetError()!=GL_NO_ERROR||!owner.Adopt(id)) {
        glDeleteTextures(1,&id);id=0;return false;
    }
    return true;
}
} // namespace fo3scene
