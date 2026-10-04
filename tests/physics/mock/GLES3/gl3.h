#pragma once
#include <cstddef>
using GLuint=unsigned; using GLenum=unsigned; using GLint=int; using GLsizei=int; using GLboolean=unsigned char; using GLfloat=float; using GLsizeiptr=ptrdiff_t;
inline constexpr unsigned GL_ARRAY_BUFFER=0;
inline constexpr unsigned GL_COMPILE_STATUS=1;
inline constexpr unsigned GL_CURRENT_PROGRAM=2;
inline constexpr unsigned GL_DEPTH_TEST=3;
inline constexpr unsigned GL_FALSE=4;
inline constexpr unsigned GL_FLOAT=5;
inline constexpr unsigned GL_FRAGMENT_SHADER=6;
inline constexpr unsigned GL_LINES=7;
inline constexpr unsigned GL_LINK_STATUS=8;
inline constexpr unsigned GL_STATIC_DRAW=9;
inline constexpr unsigned GL_TRUE=10;
inline constexpr unsigned GL_VERTEX_ARRAY_BINDING=11;
inline constexpr unsigned GL_VERTEX_SHADER=12;
template<class... T> inline void glAttachShader(T...) { }
template<class... T> inline void glBindBuffer(T...) { }
template<class... T> inline void glBindVertexArray(T...) { }
template<class... T> inline void glBufferData(T...) { }
template<class... T> inline void glCompileShader(T...) { }
template<class... T> inline GLuint glCreateProgram(T...) { return 0; }
template<class... T> inline GLuint glCreateShader(T...) { return 0; }
template<class... T> inline void glDeleteBuffers(T...) { }
template<class... T> inline void glDeleteProgram(T...) { }
template<class... T> inline void glDeleteShader(T...) { }
template<class... T> inline void glDeleteVertexArrays(T...) { }
template<class... T> inline void glDisable(T...) { }
template<class... T> inline void glDrawArrays(T...) { }
template<class... T> inline void glEnable(T...) { }
template<class... T> inline void glEnableVertexAttribArray(T...) { }
template<class... T> inline void glGenBuffers(T...) { }
template<class... T> inline void glGenVertexArrays(T...) { }
template<class... T> inline void glGetIntegerv(T...) { }
template<class... T> inline void glGetProgramInfoLog(T...) { }
template<class... T> inline void glGetProgramiv(T...) { }
template<class... T> inline void glGetShaderInfoLog(T...) { }
template<class... T> inline void glGetShaderiv(T...) { }
template<class... T> inline GLint glGetUniformLocation(T...) { return 0; }
template<class... T> inline GLboolean glIsEnabled(T...) { return 0; }
template<class... T> inline void glLineWidth(T...) { }
template<class... T> inline void glLinkProgram(T...) { }
template<class... T> inline void glShaderSource(T...) { }
template<class... T> inline void glUniformMatrix4fv(T...) { }
template<class... T> inline void glUseProgram(T...) { }
template<class... T> inline void glVertexAttribPointer(T...) { }
