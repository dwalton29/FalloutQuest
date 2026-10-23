#!/usr/bin/env python3
"""Port canonical world/LAND programs without maintaining a divergent material shader.

Only interfaces change: explicit locations, std140 uniforms, descriptor bindings,
GL clip-depth -> Vulkan clip-depth and gl_ViewIndex cameras. Lighting remains the
production GLSL. Outputs are build products, never checked-in SPIR-V blobs.
"""
import argparse
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UNIFORM = re.compile(r'\buniform\s+(?:(?:highp|mediump|lowp)\s+)?(\w+)\s+(\w+)(\[\d+\])?\s*;')
IO = re.compile(r'(?m)^(\s*)(in|out)\s+(?:(?:highp|mediump|lowp)\s+)?(\w+)\s+(\w+)\s*;')

def sources():
    runtime = (ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
    factory = runtime.split('GLuint CreateQ6HProgram() {',1)[1]
    world = [re.search(r'static const char\* '+name+r' = R"\((.*?)\)";',factory,re.S)[1]
             for name in ['vertexSource','fragmentSource']]
    land = (ROOT/'app/src/main/cpp/rendering/terrain/fo3-terrain-v237-shaders.h').read_text()
    terrain = re.findall(r'R"FQ239\((.*?)\)FQ239"',land,re.S)
    sky_text=(ROOT/'app/src/main/cpp/rendering/environment/fo3-pc-sky.h').read_text()
    sky=[re.search(r'static const char\* '+name+r' = R"\((.*?)\)";',sky_text,re.S)[1]
         for name in ['vsSource','fsSource']]
    return {'world':world, 'terrain':terrain,'sky':sky}

def port(pair,kind="world"):
    uniforms = {}
    for source in pair:
        for typ, name, array in UNIFORM.findall(source):
            value = (typ, int(array[1:-1]) if array else 0)
            if name in uniforms and uniforms[name] != value:
                raise ValueError('Uniform mismatch: '+name)
            uniforms[name] = value
    # Scalars/vectors each get one 16-byte slot; matrices and arrays use std140.
    # Explicit offsets make host writes independent of compiler struct padding.
    members, metadata, samplers = [], [], []
    offset = 0
    for name,(typ,count) in uniforms.items():
        if typ.startswith('sampler'):
            samplers.append((name,typ)); continue
        size = (64 if typ == 'mat4' else 16) * max(1,count)
        members.append(f'layout(offset={offset}) {typ} {name}{"["+str(count)+"]" if count else ""};')
        metadata.append((name,offset,size,typ,count))
        offset += size
    # Cameras use a separate shared block, while material constants use dynamic UBOs.
    header = ('#version 450\n#extension GL_EXT_multiview : require\n'
              'layout(std140,set=0,binding=0) uniform Material {\n'+'\n'.join(members)+'\n};\n'
              'layout(std140,set=0,binding=1) uniform Stereo { mat4 fqViewProjection[2]; vec4 fqEye[2]; mat4 fqSkyViewProjection[2]; };\n')
    varyings = list(dict.fromkeys(m[3] for m in IO.findall(pair[0]) if m[1]=='out'))
    converted = []
    for stage,source in enumerate(pair):
        source = re.sub(r'#version[^\n]*','',source)
        source = re.sub(r'\bprecision\s+\w+\s+\w+\s*;','',source)
        source = UNIFORM.sub('',source)
        source = re.sub(r'\b(highp|mediump|lowp)\s+','',source)
        def io(match):
            space,direction,typ,name=match.groups()
            loc = 0 if name=='fragColor' else varyings.index(name)
            return f'{space}layout(location={loc}) {direction} {typ} {name};'
        source = IO.sub(io,source)
        # Preserve original GL clip coordinates for FO3 fog, remap position only.
        if stage==0:
            source = re.sub(r'\buMvp\b','fqSkyViewProjection[gl_ViewIndex]' if kind=='sky' else 'fqViewProjection[gl_ViewIndex]',source)
            if kind=='sky':
                source=source.replace('gl_Position = fqSkyViewProjection[gl_ViewIndex] * vec4(aPosition, 1.0);',
                    'vec4 fqClip=fqSkyViewProjection[gl_ViewIndex]*vec4(aPosition,1.0); gl_Position=vec4(fqClip.x,-fqClip.y,(fqClip.z+fqClip.w)*0.5,fqClip.w);')
            source = source.replace('gl_Position = q1532Clip;',
                'gl_Position = vec4(q1532Clip.x, -q1532Clip.y, (q1532Clip.z+q1532Clip.w)*0.5, q1532Clip.w);')
            source = re.sub(r'\buEyePositionVertexQ1630\b','fqEye[gl_ViewIndex].xyz',source)
        else:
            source = re.sub(r'\buEyePosition\b','fqEye[gl_ViewIndex].xyz',source)
        sampler_text = '\n'.join(f'layout(set=0,binding={i+2}) uniform {typ} {name};'
                                 for i,(name,typ) in enumerate(samplers))
        converted.append(header+sampler_text+'\n'+source)
    return converted,metadata,samplers,offset

def build(output,compiler):
    output.mkdir(parents=True,exist_ok=True)
    cpp=['#pragma once','#include <cstdint>','#include <cstddef>','namespace fqvk::shaders {',
         'struct Field { const char* name; uint32_t offset, size, components, count; bool integer; };',
         'struct Sampler { const char* name; bool cube; };']
    for name,pair in sources().items():
        converted,meta,samplers,size = port(pair,name)
        cpp.append(f'inline constexpr uint32_t {name}UniformSize={size};')
        cpp.append(f'inline constexpr Field {name}Fields[]={{')
        for field,offset,length,typ,count in meta:
            components={'mat4':16,'vec4':4,'vec3':3,'vec2':2,'float':1,'int':1}[typ]
            cpp.append(f'{{"{field}",{offset},{length},{components},{max(1,count)},{str(typ=="int").lower()}}},')
        cpp.append('};')
        cpp.append(f'inline constexpr Sampler {name}Samplers[]={{'+','.join(
            '{"'+field+'",'+str(typ=='samplerCube').lower()+'}' for field,typ in samplers)+'};')
        for stage,text in zip(['vert','frag'],converted):
            path=output/f'{name}.{stage}';path.write_text(text)
            spv=path.with_suffix(path.suffix+'.spv')
            command = ([compiler,'-V','--target-env','vulkan1.1',str(path),'-o',str(spv)]
                       if 'glslang' in Path(compiler).name else
                       [compiler,'--target-env=vulkan1.1',str(path),'-o',str(spv)])
            subprocess.run(command,check=True)
            data=spv.read_bytes()
            words=struct.unpack('<'+'I'*(len(data)//4),data)
            cpp.append(f'inline constexpr uint32_t {name}_{stage}[]={{'+','.join(hex(x) for x in words)+'};')
    cpp.append('}')
    (output/'fo3-vulkan-shaders.h').write_text('\n'.join(cpp)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--compiler',required=True)
    args=p.parse_args();build(args.output,args.compiler)
