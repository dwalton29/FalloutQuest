#pragma once
#include "loading-work-budget.h"
#include <GLES3/gl3.h>

namespace fqloading {
// Caller owns the immutable RGBA image for the transfer lifetime. Allocation
// and mip generation remain explicit indivisible driver operations; neither is
// claimed to have a hard duration bound. Texture becomes public only at Ready.
struct ImageTransfer {
    GLuint id=0;int row=0;bool mipmaps=false,rowsComplete=false,ready=false,failed=false;
    void Cancel(){if(id)glDeleteTextures(1,&id);id=0;row=0;rowsComplete=ready=failed=false;}
    template<class Image>
    bool Step(const Image& image,bool mip,bool repeat,void(*anisotropy)()=nullptr) {
        if(failed)return false;
        if(ready)return true;
        if(!id) {
            if(image.width<=0||image.height<=0||image.rgba.size()!=size_t(image.width)*size_t(image.height)*4){failed=true;return false;}
            mipmaps=mip;int levels=1;
            if(mip)for(int d=std::max(image.width,image.height);d>1;d>>=1)++levels;
            glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mip?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
            if(anisotropy)anisotropy();
            glTexStorage2D(GL_TEXTURE_2D,levels,GL_RGBA8,image.width,image.height);
        } else if(!rowsComplete) {
            glBindTexture(GL_TEXTURE_2D,id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            const int rows=int(TransferRows(size_t(image.width),size_t(image.height-row)));
            glTexSubImage2D(GL_TEXTURE_2D,0,0,row,image.width,rows,GL_RGBA,GL_UNSIGNED_BYTE,
                image.rgba.data()+size_t(row)*size_t(image.width)*4);
            row+=rows;rowsComplete=row==image.height;
            if(rowsComplete&&!mipmaps)ready=true;
        } else {
            glBindTexture(GL_TEXTURE_2D,id);glGenerateMipmap(GL_TEXTURE_2D);ready=true;
        }
        if(glGetError()!=GL_NO_ERROR){failed=true;ready=false;}
        return ready;
    }
    GLuint Take(){GLuint result=id;id=0;row=0;rowsComplete=ready=false;return result;}
};
} // namespace fqloading
