#pragma once
// Host-only recording driver. Each test asserts actual emitted call counts;
// Android builds use the NDK's real GLES declarations.
#include <map>
#include <string>
#include <cstdint>
using GLenum=unsigned;using GLuint=unsigned;using GLint=int;using GLsizei=int;
using GLboolean=unsigned char;using GLfloat=float;using GLuint64=uint64_t;
inline constexpr GLboolean GL_TRUE=1, GL_FALSE=0;
inline constexpr GLenum GL_TEXTURE0=0x84c0;
inline std::map<std::string,unsigned> driverCalls;
inline GLint driverDisjoint=0;
inline constexpr GLenum GL_DEPTH_TEST=2;
inline constexpr GLenum GL_BLEND=3;
inline constexpr GLenum GL_CULL_FACE=4;
inline constexpr GLenum GL_POLYGON_OFFSET_FILL=5;
inline constexpr GLenum GL_SAMPLE_ALPHA_TO_COVERAGE=6;
inline constexpr GLenum GL_ARRAY_BUFFER=7;
inline constexpr GLenum GL_TEXTURE_2D=8;
inline constexpr GLenum GL_TEXTURE_CUBE_MAP=9;
inline constexpr GLenum GL_CURRENT_PROGRAM=10;
inline constexpr GLenum GL_VERTEX_ARRAY_BINDING=11;
inline constexpr GLenum GL_ARRAY_BUFFER_BINDING=12;
inline constexpr GLenum GL_ACTIVE_TEXTURE=13;
inline constexpr GLenum GL_DEPTH_FUNC=14;
inline constexpr GLenum GL_FRONT_FACE=15;
inline constexpr GLenum GL_CULL_FACE_MODE=16;
inline constexpr GLenum GL_BLEND_SRC_RGB=17;
inline constexpr GLenum GL_BLEND_DST_RGB=18;
inline constexpr GLenum GL_BLEND_SRC_ALPHA=19;
inline constexpr GLenum GL_BLEND_DST_ALPHA=20;
inline constexpr GLenum GL_TEXTURE_BINDING_2D=21;
inline constexpr GLenum GL_TEXTURE_BINDING_CUBE_MAP=22;
inline constexpr GLenum GL_DEPTH_WRITEMASK=23;
inline constexpr GLenum GL_POLYGON_OFFSET_FACTOR=24;
inline constexpr GLenum GL_POLYGON_OFFSET_UNITS=25;
inline constexpr GLenum GL_BACK=26;
inline constexpr GLenum GL_FRONT=27;
inline constexpr GLenum GL_CW=28;
inline constexpr GLenum GL_CCW=29;
inline constexpr GLenum GL_LEQUAL=30;
inline constexpr GLenum GL_LESS=31;
inline constexpr GLenum GL_ONE=32;
inline constexpr GLenum GL_ZERO=33;
inline constexpr GLenum GL_SRC_ALPHA=34;
inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA=35;
inline constexpr GLenum GL_TRIANGLES=36;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER=37;
inline constexpr GLenum GL_EXTENSIONS=38;
template<class... T> inline void glEnable(T...) { ++driverCalls["glEnable"]; }
template<class... T> inline void glDisable(T...) { ++driverCalls["glDisable"]; }
template<class... T> inline void glUseProgram(T...) { ++driverCalls["glUseProgram"]; }
template<class... T> inline void glBindVertexArray(T...) { ++driverCalls["glBindVertexArray"]; }
template<class... T> inline void glBindBuffer(T...) { ++driverCalls["glBindBuffer"]; }
template<class... T> inline void glActiveTexture(T...) { ++driverCalls["glActiveTexture"]; }
template<class... T> inline void glBindTexture(T...) { ++driverCalls["glBindTexture"]; }
template<class... T> inline void glDepthMask(T...) { ++driverCalls["glDepthMask"]; }
template<class... T> inline void glDepthFunc(T...) { ++driverCalls["glDepthFunc"]; }
template<class... T> inline void glCullFace(T...) { ++driverCalls["glCullFace"]; }
template<class... T> inline void glFrontFace(T...) { ++driverCalls["glFrontFace"]; }
template<class... T> inline void glPolygonOffset(T...) { ++driverCalls["glPolygonOffset"]; }
template<class... T> inline void glBlendFuncSeparate(T...) { ++driverCalls["glBlendFuncSeparate"]; }
template<class... T> inline void glUniform1f(T...) { ++driverCalls["glUniform1f"]; }
template<class... T> inline void glUniform1i(T...) { ++driverCalls["glUniform1i"]; }
template<class... T> inline void glUniform2f(T...) { ++driverCalls["glUniform2f"]; }
template<class... T> inline void glUniform3f(T...) { ++driverCalls["glUniform3f"]; }
template<class... T> inline void glUniform4f(T...) { ++driverCalls["glUniform4f"]; }
template<class... T> inline void glUniform1fv(T...) { ++driverCalls["glUniform1fv"]; }
template<class... T> inline void glUniform2fv(T...) { ++driverCalls["glUniform2fv"]; }
template<class... T> inline void glUniform3fv(T...) { ++driverCalls["glUniform3fv"]; }
template<class... T> inline void glUniform4fv(T...) { ++driverCalls["glUniform4fv"]; }
template<class... T> inline void glUniformMatrix4fv(T...) { ++driverCalls["glUniformMatrix4fv"]; }
template<class... T> inline void glDrawArrays(T...) { ++driverCalls["glDrawArrays"]; }
template<class... T> inline void glDrawArraysInstanced(T...) { ++driverCalls["glDrawArraysInstanced"]; }
template<class... T> inline void glDeleteProgram(T...) { ++driverCalls["glDeleteProgram"]; }
template<class... T> inline void glDeleteTextures(T...) { ++driverCalls["glDeleteTextures"]; }
template<class... T> inline void glDeleteBuffers(T...) { ++driverCalls["glDeleteBuffers"]; }
template<class... T> inline void glDeleteVertexArrays(T...) { ++driverCalls["glDeleteVertexArrays"]; }
inline GLboolean glIsEnabled(GLenum) { ++driverCalls["glIsEnabled"];return GL_FALSE; }
inline void glGetIntegerv(GLenum,GLint* out) { ++driverCalls["glGetIntegerv"];*out=driverDisjoint; }
inline void glGetBooleanv(GLenum,GLboolean* out) { ++driverCalls["glGetBooleanv"];*out=GL_TRUE; }
inline void glGetFloatv(GLenum,GLfloat* out) { ++driverCalls["glGetFloatv"];*out=0; }
inline const unsigned char* glGetString(GLenum) { return nullptr; }
