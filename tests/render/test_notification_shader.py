"""Compile/link and pixel-test the production notification shader with Mesa GLES.
Readbacks belong to this host test, never the app render loop.
"""
import ctypes as C
import ctypes.util
import os
import re
from pathlib import Path
os.environ['EGL_PLATFORM']='surfaceless'
egl=C.CDLL(ctypes.util.find_library('EGL') or 'libEGL.so.1')
def e(name,ret,*args):
    f=getattr(egl,name);f.restype=ret;f.argtypes=args;return f
ptr=C.c_void_p; integer=C.c_int; uint=C.c_uint
get=e('eglGetDisplay',ptr,ptr)
display=get(None)
assert e('eglInitialize',uint,ptr,ptr,ptr)(display,None,None)
assert e('eglBindAPI',uint,uint)(0x30A0)
attrs=(integer*13)(0x3033,1,0x3040,0x40,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038)
config=ptr();count=integer()
assert e('eglChooseConfig',uint,ptr,ptr,ptr,integer,ptr)(display,attrs,C.byref(config),1,C.byref(count)) and count.value
surface=e('eglCreatePbufferSurface',ptr,ptr,ptr,ptr)(display,config,(integer*5)(0x3057,1,0x3056,1,0x3038))
context=e('eglCreateContext',ptr,ptr,ptr,ptr,ptr)(display,config,None,(integer*3)(0x3098,3,0x3038))
assert context and e('eglMakeCurrent',uint,ptr,ptr,ptr,ptr)(display,surface,surface,context)
getproc=e('eglGetProcAddress',ptr,C.c_char_p)
def gl(name,ret,*args):
    address=getproc(name.encode());assert address,name
    return C.CFUNCTYPE(ret,*args)(address)

source=(Path(__file__).resolve().parents[2]/'app/src/main/cpp/ui/interaction/fo3-item-notification-renderer.h').read_text()
program=gl('glCreateProgram',uint)()
for name,kind in [('vs',0x8B31),('fs',0x8B30)]:
    shader=gl('glCreateShader',uint,uint)(kind)
    text=re.search(rf'{name}\s*=\s*R"\((.*?)\)";',source,re.S).group(1).encode()
    gl('glShaderSource',None,uint,integer,ptr,ptr)(shader,1,(C.c_char_p*1)(text),None)
    gl('glCompileShader',None,uint)(shader)
    status=integer();gl('glGetShaderiv',None,uint,uint,ptr)(shader,0x8B81,C.byref(status))
    if not status.value:
        log=C.create_string_buffer(8192);gl('glGetShaderInfoLog',None,uint,integer,ptr,ptr)(shader,8192,None,log);raise AssertionError(log.value.decode())
    gl('glAttachShader',None,uint,uint)(program,shader)
gl('glLinkProgram',None,uint)(program);status=integer()
gl('glGetProgramiv',None,uint,uint,ptr)(program,0x8B82,C.byref(status))
if not status.value:
    log=C.create_string_buffer(8192);gl('glGetProgramInfoLog',None,uint,integer,ptr,ptr)(program,8192,None,log);raise AssertionError(log.value.decode())
gl('glUseProgram',None,uint)(program)
def loc(name):return gl('glGetUniformLocation',integer,uint,C.c_char_p)(program,name.encode())
gl('glUniformMatrix4fv',None,integer,integer,uint,ptr)(loc('uMvp'),1,0,(C.c_float*16)(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))
gl('glUniform4f',None,integer,C.c_float,C.c_float,C.c_float,C.c_float)(loc('uTint'),1,1,1,1)
gl('glUniform1i',None,integer,integer)(loc('uTex'),0)
vao=uint();vbo=uint();texture=uint()
gl('glGenVertexArrays',None,integer,ptr)(1,C.byref(vao));gl('glBindVertexArray',None,uint)(vao)
gl('glGenBuffers',None,integer,ptr)(1,C.byref(vbo));gl('glBindBuffer',None,uint,uint)(0x8892,vbo)
values=[]
for x,y in [(-1,-1),(1,-1),(-1,1),(-1,1),(1,-1),(1,1)]:values.extend([x,y,0,.5,.5,0])
data=(C.c_float*len(values))(*values)
gl('glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8892,C.sizeof(data),data,0x88E4)
for i,count,offset in [(0,3,0),(1,2,12),(2,1,20)]:
    gl('glVertexAttribPointer',None,uint,integer,uint,uint,integer,ptr)(i,count,0x1406,0,24,ptr(offset))
    gl('glEnableVertexAttribArray',None,uint)(i)
gl('glGenTextures',None,integer,ptr)(1,C.byref(texture));gl('glBindTexture',None,uint,uint)(0x0DE1,texture)
for param in [0x2800,0x2801]:gl('glTexParameteri',None,uint,uint,integer)(0x0DE1,param,0x2600)
gl('glTexImage2D',None,uint,integer,integer,integer,integer,integer,uint,uint,ptr)(0x0DE1,0,0x1908,1,1,0,0x1908,0x1401,(C.c_ubyte*4)(255,255,255,255))
gl('glViewport',None,integer,integer,integer,integer)(0,0,1,1)
gl('glDisable',None,uint)(0x0B71);gl('glDisable',None,uint)(0x0BE2)
for eye in range(2):
    for age,expected in [(0,0),(1,128),(3,255),(7,128),(8,0),(-1,255)]:
        gl('glUniform1f',None,integer,C.c_float)(loc('uAge'),age)
        gl('glDrawArrays',None,uint,integer,integer)(4,0,6)
        pixel=(C.c_ubyte*4)();gl('glReadPixels',None,integer,integer,integer,integer,uint,uint,ptr)(0,0,1,1,0x1908,0x1401,pixel)
        assert abs(pixel[3]-expected)<=1,(eye,age,list(pixel),expected)
assert gl('glGetError',uint)()==0
print('Production notification GLES shader: both eyes, recovered fade curve and ordinary item alpha passed on Mesa')
