#pragma once
#include "../../../render/mock/GLES3/gl3.h"
#include <cstddef>
using GLsizeiptr = std::ptrdiff_t;
inline constexpr GLenum GL_CLAMP_TO_EDGE = 500;
inline constexpr GLenum GL_COLOR_ATTACHMENT0 = 501;
inline constexpr GLenum GL_COLOR_BUFFER_BIT = 502;
inline constexpr GLenum GL_COMPILE_STATUS = 503;
inline constexpr GLenum GL_DYNAMIC_DRAW = 504;
inline constexpr GLenum GL_FLOAT = 505;
inline constexpr GLenum GL_FRAGMENT_SHADER = 506;
inline constexpr GLenum GL_FRAMEBUFFER = 507;
inline constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 508;
inline constexpr GLenum GL_LINEAR = 509;
inline constexpr GLenum GL_LINK_STATUS = 510;
inline constexpr GLenum GL_RGBA = 511;
inline constexpr GLenum GL_RGBA8 = 512;
inline constexpr GLenum GL_TEXTURE_MAG_FILTER = 513;
inline constexpr GLenum GL_TEXTURE_MIN_FILTER = 514;
inline constexpr GLenum GL_TEXTURE_WRAP_S = 515;
inline constexpr GLenum GL_TEXTURE_WRAP_T = 516;
inline constexpr GLenum GL_UNSIGNED_BYTE = 517;
inline constexpr GLenum GL_VERTEX_SHADER = 518;
template <class... Args> inline void glAttachShader(Args...) {
  ++driverCalls["glAttachShader"];
}
template <class... Args> inline void glBindFramebuffer(Args...) {
  ++driverCalls["glBindFramebuffer"];
}
template <class... Args> inline void glBufferData(Args...) {
  ++driverCalls["glBufferData"];
}
template <class... Args> inline GLenum glCheckFramebufferStatus(Args...) {
  ++driverCalls["glCheckFramebufferStatus"];
  return GL_FRAMEBUFFER_COMPLETE;
}
template <class... Args> inline void glClear(Args...) {
  ++driverCalls["glClear"];
}
template <class... Args> inline void glClearColor(Args...) {
  ++driverCalls["glClearColor"];
}
template <class... Args> inline void glCompileShader(Args...) {
  ++driverCalls["glCompileShader"];
}
template <class... Args> inline GLuint glCreateProgram(Args...) {
  ++driverCalls["glCreateProgram"];
  return 1;
}
template <class... Args> inline GLuint glCreateShader(Args...) {
  ++driverCalls["glCreateShader"];
  return 1;
}
template <class... Args> inline void glDeleteFramebuffers(Args...) {
  ++driverCalls["glDeleteFramebuffers"];
}
template <class... Args> inline void glDeleteShader(Args...) {
  ++driverCalls["glDeleteShader"];
}
template <class... Args> inline void glEnableVertexAttribArray(Args...) {
  ++driverCalls["glEnableVertexAttribArray"];
}
template <class... Args> inline void glFramebufferTexture2D(Args...) {
  ++driverCalls["glFramebufferTexture2D"];
}
template <class... Args> inline void glGenBuffers(Args...) {
  ++driverCalls["glGenBuffers"];
}
template <class... Args> inline void glGenFramebuffers(Args...) {
  ++driverCalls["glGenFramebuffers"];
}
template <class... Args> inline void glGenTextures(Args...) {
  ++driverCalls["glGenTextures"];
}
template <class... Args> inline void glGenVertexArrays(Args...) {
  ++driverCalls["glGenVertexArrays"];
}
template <class... Args> inline void glGetProgramInfoLog(Args...) {
  ++driverCalls["glGetProgramInfoLog"];
}
template <class... Args> inline void glGetProgramiv(Args...) {
  ++driverCalls["glGetProgramiv"];
}
template <class... Args> inline void glGetShaderInfoLog(Args...) {
  ++driverCalls["glGetShaderInfoLog"];
}
template <class... Args> inline void glGetShaderiv(Args...) {
  ++driverCalls["glGetShaderiv"];
}
template <class... Args> inline GLint glGetUniformLocation(Args...) {
  ++driverCalls["glGetUniformLocation"];
  return 1;
}
template <class... Args> inline void glLinkProgram(Args...) {
  ++driverCalls["glLinkProgram"];
}
template <class... Args> inline void glShaderSource(Args...) {
  ++driverCalls["glShaderSource"];
}
template <class... Args> inline void glTexImage2D(Args...) {
  ++driverCalls["glTexImage2D"];
}
template <class... Args> inline void glTexParameteri(Args...) {
  ++driverCalls["glTexParameteri"];
}
template <class... Args> inline void glVertexAttribPointer(Args...) {
  ++driverCalls["glVertexAttribPointer"];
}
template <class... Args> inline void glViewport(Args...) {
  ++driverCalls["glViewport"];
}

template <class... Args> inline void glBufferSubData(Args...) {
  ++driverCalls["glBufferSubData"];
}

inline constexpr GLenum GL_REPEAT = 550, GL_DST_COLOR = 551;
