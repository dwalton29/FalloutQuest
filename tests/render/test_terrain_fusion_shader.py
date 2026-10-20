"""Compare the production fused LAND shader with the retained v237 multipass shader.
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
# Compare actual production programs, rather than a second CPU implementation.
ROOT=Path(__file__).resolve().parents[2]
source=(ROOT/'app/src/main/cpp/rendering/terrain/fo3-terrain-render.inc').read_text()
frozen=(ROOT/'app/src/main/cpp/rendering/terrain/fo3-terrain-v237-shaders.h').read_text()
for key,extension in [('kV237VertexShader','vert'),('kV237FragmentShader','frag')]:
    data=re.search(key+r'\s*=\s*R"FQ239\((.*?)\)FQ239";',frozen,re.S)
    assert data,key
    assert data.group(1)==(ROOT/f'tests/render/fixtures/terrain_v237.{extension}').read_text()
assert 'q239Legacy ? fo3terrain::kV237VertexShader : vertexSource' in source
assert 'q239Legacy ? fo3terrain::kV237FragmentShader : fragmentSource' in source
def program(texts):
    p=gl('glCreateProgram',uint)()
    for text,kind in zip(texts,[0x8B31,0x8B30]):
        s=gl('glCreateShader',uint,uint)(kind);encoded=text.encode()
        gl('glShaderSource',None,uint,integer,ptr,ptr)(s,1,(C.c_char_p*1)(encoded),None)
        gl('glCompileShader',None,uint)(s);ok=integer()
        gl('glGetShaderiv',None,uint,uint,ptr)(s,0x8B81,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(8192);gl('glGetShaderInfoLog',None,uint,integer,ptr,ptr)(s,8192,None,log)
            raise AssertionError(log.value.decode())
        gl('glAttachShader',None,uint,uint)(p,s)
    gl('glLinkProgram',None,uint)(p);ok=integer();gl('glGetProgramiv',None,uint,uint,ptr)(p,0x8B82,C.byref(ok))
    if not ok.value:
        log=C.create_string_buffer(8192);gl('glGetProgramInfoLog',None,uint,integer,ptr,ptr)(p,8192,None,log)
        raise AssertionError(log.value.decode())
    return p
legacy=program([(ROOT/f'tests/render/fixtures/terrain_v237.{e}').read_text() for e in ['vert','frag']])
fused_shaders=[re.search(n+r'\s*=\s*R"\((.*?)\)";',source,re.S).group(1).lstrip() for n in ['vertexSource','fragmentSource']]
fused=program(fused_shaders)
unshared=program([fused_shaders[0],fused_shaders[1].replace('#version 300 es',
    '#version 300 es\n#define FQ239_UNSHARED_LIGHTS 1',1)])
loc=gl('glGetUniformLocation',integer,uint,C.c_char_p)
current=0
def ui(name,value):gl('glUniform1i',None,integer,integer)(loc(current,name.encode()),value)
def uf(name,*values):gl('glUniform'+str(len(values))+'f',None,integer,*([C.c_float]*len(values)))(loc(current,name.encode()),*values)
def mat(name):gl('glUniformMatrix4fv',None,integer,integer,uint,ptr)(loc(current,name.encode()),1,0,(C.c_float*16)(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))
textures=[]
def texture(unit,colour):
    t=uint();gl('glGenTextures',None,integer,ptr)(1,C.byref(t));textures.append(t.value)
    gl('glActiveTexture',None,uint)(0x84C0+unit);gl('glBindTexture',None,uint,uint)(0x0DE1,t)
    for key in [0x2800,0x2801]:gl('glTexParameteri',None,uint,uint,integer)(0x0DE1,key,0x2600)
    gl('glTexImage2D',None,uint,integer,integer,integer,integer,integer,uint,uint,ptr)(0x0DE1,0,0x8058,1,1,0,0x1908,0x1401,(C.c_ubyte*4)(*colour))
    return t.value
colours=[(163,110,45,255),(61,150,90,255),(200,40,97,255),(30,60,180,255)]
diffuse=[texture(i,c) for i,c in enumerate(colours)]
normals=[texture(i+4,c) for i,c in enumerate([(128,128,255,20),(144,110,250,180),(92,157,245,120),(125,130,255,250)])]
shadow=texture(8,(255,255,255,255))
vao=uint();gl('glGenVertexArrays',None,integer,ptr)(1,C.byref(vao));gl('glBindVertexArray',None,uint)(vao)
vbo=uint();gl('glGenBuffers',None,integer,ptr)(1,C.byref(vbo));gl('glBindBuffer',None,uint,uint)(0x8892,vbo)
def vertices(paint,layer):
    values=[]
    for c,(x,y) in enumerate([(-1,-1),(3,-1),(-1,3)]):
        alphas=[(.1+c*.2),(.7-c*.1),(.05+c*.3)]
        if c==0: alphas[1]=0.0  # Exact zero-alpha sparse-layer case.
        values += [x,y,0,0,1,0,x*.1,y*.1,1 if layer==0 else alphas[layer-1],.8,.9,.7]
        if paint:values+=alphas
    gl('glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8892,len(values)*4,(C.c_float*len(values))(*values),0x88E4)
    stride=60 if paint else 48
    for index,size,offset in [(0,3,0),(1,3,3),(2,2,6),(3,1,8),(4,3,9)]+([(5,3,12)] if paint else []):
        gl('glVertexAttribPointer',None,uint,integer,uint,uint,integer,ptr)(index,size,0x1406,0,stride,ptr(offset*4))
        gl('glEnableVertexAttribArray',None,uint)(index)
    if not paint:gl('glDisableVertexAttribArray',None,uint)(5)
def setup(p,mode,lit):
    global current
    current=p;gl('glUseProgram',None,uint)(p)
    mat('uMvp');mat('uLightMvp')
    uf('uFogNearVertexQ1532',0);uf('uFogFarVertexQ1532',7);uf('uFogPowerVertexQ1532',1.4)
    uf('uAmbientColor',.4,.3,.2);uf('uSunlightColor',1.5,1.2,.9);uf('uSunDirection',.1,1,.2)
    uf('uEyePosition',0,3,0);uf('uFogColor',.3,.4,.6)
    ui('uLocalLightCount',3 if lit else 0)
    for i,(pos,col) in enumerate([
        ((0,3,0,20),(.7,.2,.4,1.1)),
        ((-1,1,0,8),(.4,.8,.3,1.7)),
        ((30,0,0,2),(.1,.1,.1,1.0)), # Outside-radius light.
    ]):
        uf(f'uLocalLightPosRadius[{i}]',*pos)
        uf(f'uLocalLightColorFalloff[{i}]',*col)
    ui('uShadowMap',8);uf('uShadowsEnabled',1 if lit else 0);uf('uShadowTexelSize',1,1)
    ui('uRoundMode',mode);ui('uUseTexture',1);ui('uUseNormal',1);uf('uTerrainSpecularEnabled',1)
    for i in range(4):
        for unit,tex in [(i,diffuse[i]),(i+4,normals[i])]:
            gl('glActiveTexture',None,uint)(0x84C0+unit);gl('glBindTexture',None,uint,uint)(0x0DE1,tex)
    gl('glActiveTexture',None,uint)(0x84C8);gl('glBindTexture',None,uint,uint)(0x0DE1,shadow)
    for i in range(3):
        ui(f'uPaintDiffuse{i}',i+1);ui(f'uPaintNormal{i}',i+5);uf(f'uPaintFlags{i}',1,float(i%2))
def read(mode):
    data=(C.c_float*(16*16*4))() if mode==1 else (C.c_ubyte*(16*16*4))()
    gl('glReadPixels',None,integer,integer,integer,integer,uint,uint,ptr)(0,0,16,16,0x1908,0x1406 if mode==1 else 0x1401,data)
    return list(data)
fbo=uint();gl('glGenFramebuffers',None,integer,ptr)(1,C.byref(fbo))
max_error=0
for mode,fmt in [(1,0x881A),(2,0x8058)]:
    target=texture(9,(0,0,0,0))
    gl('glTexImage2D',None,uint,integer,integer,integer,integer,integer,uint,uint,ptr)(0x0DE1,0,fmt,16,16,0,0x1908,0x1406 if mode==1 else 0x1401,None)
    gl('glBindFramebuffer',None,uint,uint)(0x8D40,fbo)
    gl('glFramebufferTexture2D',None,uint,uint,uint,uint,integer)(0x8D40,0x8CE0,0x0DE1,target,0)
    assert gl('glCheckFramebufferStatus',uint,uint)(0x8D40)==0x8CD5
    gl('glViewport',None,integer,integer,integer,integer)(0,0,16,16)
    for layers in [1,2,3]:
        for lit in [False,True]:
            setup(legacy,mode,lit)
            gl('glDisable',None,uint)(0x0BE2)
            for layer in range(layers+1):
                if layer:
                    gl('glEnable',None,uint)(0x0BE2);gl('glBlendFunc',None,uint,uint)(0x0302,0x0303)
                ui('uDiffuse',layer);ui('uNormalGloss',layer+4);uf('uTerrainSpecularEnabled',1 if layer==0 else float((layer-1)%2))
                vertices(False,layer);gl('glDrawArrays',None,uint,integer,integer)(4,0,3)
            old=read(mode)
            setup(fused,mode,lit);ui('uDiffuse',0);ui('uNormalGloss',4);ui('uPaintCount',layers)
            gl('glDisable',None,uint)(0x0BE2);vertices(True,0);gl('glDrawArrays',None,uint,integer,integer)(4,0,3)
            new=read(mode)
            setup(unshared,mode,lit);ui('uDiffuse',0);ui('uNormalGloss',4);ui('uPaintCount',layers)
            gl('glDisable',None,uint)(0x0BE2);vertices(True,0);gl('glDrawArrays',None,uint,integer,integer)(4,0,3)
            original_lighting=read(mode)
            shared_error=max(abs(a-b) for a,b in zip(original_lighting,new))
            assert shared_error <= (.002 if mode==1 else 2),(mode,layers,lit,'shared',shared_error)
            error=max(abs(a-b) for a,b in zip(old,new));max_error=max(max_error,error)
            # FP16 blend arithmetic and UNORM blending have implementation-specific
            # rounding. Bound differences to two representable steps near 1.
            assert error <= (.002 if mode==1 else 2),(mode,layers,lit,error)
            assert gl('glGetError',uint)()==0
print('Production shared/unshared fused shaders match v237 multipass: 1–3 layers, fog, normals, specular, shadows, local lights; FP16/RGBA8. Max error:',max_error)
