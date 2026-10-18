#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
using GLuint=unsigned;using GLenum=unsigned;using GLint=int;using GLsizei=int;
constexpr GLenum GL_TEXTURE_2D=1,GL_TEXTURE_MIN_FILTER=2,GL_TEXTURE_MAG_FILTER=3,GL_LINEAR_MIPMAP_LINEAR=4,
 GL_LINEAR=5,GL_TEXTURE_WRAP_S=6,GL_TEXTURE_WRAP_T=7,GL_REPEAT=8,GL_CLAMP_TO_EDGE=9,GL_RGBA8=10,
 GL_UNPACK_ALIGNMENT=11,GL_RGBA=12,GL_UNSIGNED_BYTE=13,GL_NO_ERROR=0;
namespace driver {
inline GLuint next=1;inline unsigned allocations=0,deletions=0,mips=0,transfers=0;
inline int width=0,height=0,levels=0;inline GLenum error=0;
inline std::vector<uint8_t> pixels;inline std::vector<int> writes;
}
inline void glGenTextures(GLsizei,GLuint* id){*id=driver::next++;}
inline void glDeleteTextures(GLsizei,const GLuint*){++driver::deletions;}
inline void glBindTexture(GLenum,GLuint){}
inline void glTexParameteri(GLenum,GLenum,GLint){}
inline void glPixelStorei(GLenum,GLint){}
inline GLenum glGetError(){auto e=driver::error;driver::error=0;return e;}
inline void glTexStorage2D(GLenum,int levels,GLenum,int width,int height){
 ++driver::allocations;driver::width=width;driver::height=height;driver::levels=levels;
 driver::pixels.assign(size_t(width)*height*4,0);driver::writes.assign(height,0);
}
inline void glTexSubImage2D(GLenum,int,int x,int y,int width,int height,GLenum,GLenum,const void* data){
 assert(x==0&&width==driver::width&&y>=0&&height>0&&y+height<=driver::height);
 ++driver::transfers;auto src=static_cast<const uint8_t*>(data);
 std::copy(src,src+size_t(width)*height*4,driver::pixels.begin()+size_t(y)*width*4);
 for(int row=y;row<y+height;++row)++driver::writes[row];
}
inline void glGenerateMipmap(GLenum){
 assert(std::all_of(driver::writes.begin(),driver::writes.end(),[](int n){return n==1;}));++driver::mips;
}
