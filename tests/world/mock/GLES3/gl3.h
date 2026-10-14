#pragma once
#include <cstdint>
#include <map>
#include <tuple>
#include <vector>
using GLenum=unsigned;using GLuint=uint32_t;using GLint=int;using GLsizei=int;
inline constexpr GLenum GL_TEXTURE_2D=1,GL_TEXTURE_CUBE_MAP=2,
 GL_UNPACK_ALIGNMENT=3,GL_TEXTURE_WRAP_S=4,GL_TEXTURE_WRAP_T=5,
 GL_TEXTURE_WRAP_R=6,GL_REPEAT=7,GL_CLAMP_TO_EDGE=8,
 GL_TEXTURE_MIN_FILTER=9,GL_TEXTURE_MAG_FILTER=10,GL_LINEAR=11,
 GL_LINEAR_MIPMAP_LINEAR=12,GL_RGBA8=13,GL_RGBA=14,GL_UNSIGNED_BYTE=15,
 GL_TEXTURE_MAX_LEVEL=16,GL_TEXTURE_CUBE_MAP_POSITIVE_X=100,GL_NO_ERROR=0;
namespace mock {
inline GLuint next=0;
inline int operation=0,failAt=-1;
inline GLenum error=0;
inline bool zero=false;
inline std::map<GLuint,int> deleted;
inline std::vector<std::tuple<GLenum,int,int,int>> uploads;
inline std::map<std::pair<GLenum,GLenum>,GLint> parameters;
inline int mipmaps=0;
inline void step(){if(operation++==failAt)error=999;}
inline void reset(){operation=0;failAt=-1;error=0;zero=false;uploads.clear();parameters.clear();mipmaps=0;}
}
inline void glGenTextures(GLsizei count,GLuint* out){mock::step();for(int i=0;i<count;++i)out[i]=mock::zero?0:++mock::next;}
inline void glBindTexture(GLenum,GLuint){mock::step();}
inline void glPixelStorei(GLenum,GLint){mock::step();}
inline void glTexParameteri(GLenum target,GLenum name,GLint value){mock::step();mock::parameters[{target,name}]=value;}
inline void glTexImage2D(GLenum target,GLint level,GLint,GLsizei w,GLsizei h,GLint,GLenum,GLenum,const void*){
    mock::step();mock::uploads.emplace_back(target,level,w,h);
}
inline void glGenerateMipmap(GLenum){mock::step();++mock::mipmaps;}
inline GLenum glGetError(){auto error=mock::error;mock::error=0;return error;}
inline void glDeleteTextures(GLsizei count,const GLuint* ids){for(int i=0;i<count;++i)++mock::deleted[ids[i]];}
