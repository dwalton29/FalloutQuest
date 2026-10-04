"""Execute the production world vertex shader with GLES transform feedback.
Uses Mesa surfaceless EGL in host CI; no synchronisation is added to the app.
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
source=(Path(__file__).resolve().parents[2]/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
program_source=source.split('GLuint CreateQ6HProgram()',1)[1].split('void Q1070ApplyStaticAnisotropy',1)[0]
create_shader=gl('glCreateShader',uint,uint)
shader_source=gl('glShaderSource',None,uint,integer,ptr,ptr)
compile_shader=gl('glCompileShader',None,uint)
get_shader=gl('glGetShaderiv',None,uint,uint,ptr)
get_program=gl('glGetProgramiv',None,uint,uint,ptr)
program=gl('glCreateProgram',uint)()
for name,kind in [('vertexSource',0x8B31),('fragmentSource',0x8B30)]:
    text=re.search(rf'{name}\s*=\s*R"\((.*?)\)";',program_source,re.S).group(1).lstrip().encode()
    shader=create_shader(kind)
    shader_source(shader,1,(C.c_char_p*1)(text),None);compile_shader(shader)
    status=integer();get_shader(shader,0x8B81,C.byref(status))
    if not status.value:
        log=C.create_string_buffer(8192);gl('glGetShaderInfoLog',None,uint,integer,ptr,ptr)(shader,8192,None,log)
        raise AssertionError(log.value.decode())
    gl('glAttachShader',None,uint,uint)(program,shader)
varyings=(C.c_char_p*4)(b'vPosition',b'vNormal',b'vTangent',b'vBitangent')
gl('glTransformFeedbackVaryings',None,uint,integer,ptr,uint)(program,4,varyings,0x8C8C)
gl('glLinkProgram',None,uint)(program)
status=integer();get_program(program,0x8B82,C.byref(status))
if not status.value:
    log=C.create_string_buffer(8192);gl('glGetProgramInfoLog',None,uint,integer,ptr,ptr)(program,8192,None,log)
    raise AssertionError(log.value.decode())
gl('glUseProgram',None,uint)(program)
loc=gl('glGetUniformLocation',integer,uint,C.c_char_p)
def uniform(name,value):gl('glUniform1i',None,integer,integer)(loc(program,name.encode()),value)
def matrix(name,value):gl('glUniformMatrix4fv',None,integer,integer,uint,ptr)(loc(program,name.encode()),1,0,(C.c_float*16)(*value))
identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
matrix('uMvp',identity);matrix('uLightMvp',identity)
for name,value in [('uFogFarVertexQ1532',1000),('uFogPowerVertexQ1532',1)]:
    gl('glUniform1f',None,integer,C.c_float)(loc(program,name.encode()),value)
for name,values in [('uSunDirectionVertexQ1540',(0,1,0)),('uEyePositionVertexQ1630',(100,200,300))]:
    gl('glUniform3f',None,integer,C.c_float,C.c_float,C.c_float)(loc(program,name.encode()),*values)
vao=uint();gl('glGenVertexArrays',None,integer,ptr)(1,C.byref(vao));gl('glBindVertexArray',None,uint)(vao)
bind=gl('glBindBuffer',None,uint,uint);data=gl('glBufferData',None,uint,C.c_ssize_t,ptr,uint)
def buffer(values):
    result=uint();gl('glGenBuffers',None,integer,ptr)(1,C.byref(result));bind(0x8892,result)
    data(0x8892,len(values)*4,(C.c_float*len(values))(*values),0x88E4)
    return result
vertex=buffer([2,3,4,1,0,0,0,1,0,0,0,1,.25,.75,1,1,1,1])
attribute=gl('glVertexAttribPointer',None,uint,integer,uint,uint,integer,ptr)
enable_attribute=gl('glEnableVertexAttribArray',None,uint)
for index,components,offset in [(0,3,0),(1,3,3),(2,3,6),(3,3,9),(4,2,12),(5,4,14)]:
    attribute(index,components,0x1406,0,72,ptr(offset*4));enable_attribute(index)
weights=buffer([1,0,0,0,1,0,0,0])
for index,offset in [(10,0),(11,4)]:
    attribute(index,4,0x1406,0,32,ptr(offset*4));enable_attribute(index)
tex=uint();gl('glGenTextures',None,integer,ptr)(1,C.byref(tex))
gl('glActiveTexture',None,uint)(0x84C6);gl('glBindTexture',None,uint,uint)(0x0DE1,tex)
for key,value in [(0x2801,0x2600),(0x2800,0x2600)]:gl('glTexParameteri',None,uint,uint,integer)(0x0DE1,key,value)
position=[0,1.5,0,0,-1,0,0,0,0,0,1,0,.2,-.4,.6,1]
direction=[0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1]
palette=identity+identity+position+direction
upload=(C.c_float*64)(*palette)
gl('glTexImage2D',None,uint,integer,integer,integer,integer,integer,uint,uint,ptr)(0x0DE1,0,0x8814,8,2,0,0x1908,0x1406,upload)
uniform('uActorPalette',6)
for name,unit in [('uDiffuse',0),('uNormalGloss',1),('uGlow',2),('uShadowMap',3),('uEnvironmentCubeQ2050',4),('uEnvironmentMaskQ2050',5)]:uniform(name,unit)
feedback=uint();gl('glGenBuffers',None,integer,ptr)(1,C.byref(feedback));bind(0x8C8E,feedback);data(0x8C8E,48,None,0x88E8)
gl('glBindBufferBase',None,uint,uint,uint)(0x8C8E,0,feedback)
gl('glEnable',None,uint)(0x8C89)
def run(mode,weight,root=False):
    uniform('uActorSkinMode',mode);gl('glUniform1f',None,integer,C.c_float)(loc(program,b'uObjectTransformEnabledQ2017'),float(root))
    transform=identity.copy();transform[12:15]=[10,20,30];matrix('uObjectTransformQ2017',transform)
    bind(0x8892,weights);data(0x8892,32,(C.c_float*8)(weight,0,0,0,1,0,0,0),0x88E4)
    gl('glBeginTransformFeedback',None,uint)(0)
    gl('glDrawArrays',None,uint,integer,integer)(0,0,1)
    gl('glEndTransformFeedback',None)()
    bind(0x8C8E,feedback)
    p=gl('glMapBufferRange',ptr,uint,C.c_ssize_t,C.c_ssize_t,uint)(0x8C8E,0,48,1)
    assert p
    actual=list((C.c_float*12).from_address(p))
    assert gl('glUnmapBuffer',uint,uint)(0x8C8E)
    error=gl('glGetError',uint)();assert error==0,hex(error)
    base=[2,3,4];skinned=[-2.8,2.6,4.6]
    if mode==0 or weight==0:expected=base
    elif mode==2:expected=skinned
    else:expected=[weight*s+(1-weight)*b if weight<.999 else weight*s for s,b in zip(skinned,base)]
    if root:expected=[v+r for v,r in zip(expected,[10,20,30])]
    for a,b in zip(actual[:3],expected):assert abs(a-b)<1e-4,(mode,weight,actual,expected)
    if mode!=0 and (weight>=.999 or mode==2 and weight>0):
        for a,b in zip(actual[3:],[0,1,0,-1,0,0,0,0,1]):assert abs(a-b)<1e-4,(actual,b)
for mode,weight in [(0,1),(1,1),(1,.5),(1,1.2),(1,0),(2,.5),(2,0)]:run(mode,weight,mode==1)
print('Production GLES GPU skinning transform-feedback parity passed')
