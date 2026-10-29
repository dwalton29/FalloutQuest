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
    water_text=(ROOT/'app/src/main/cpp/rendering/water/fo3-water.cpp').read_text().split('bool EnsureRenderer() {',1)[1]
    water=[re.search(r'static const char\* '+name+r' = R"\((.*?)\)";',water_text,re.S)[1] for name in ['vs','fs']]
    result={'world':world, 'terrain':terrain,'sky':sky,'water':water}
    def extract(section,name):
        return re.search(r'static const char\* '+name+r' = R"\((.*?)\)";',section,re.S)[1]
    adapt=runtime.split('bool Q1350EnsureAdaptationQ1350() {',1)[1]
    bloom_vertex=extract(runtime.split('GLuint Q1670CreateProgramQ1670(',1)[1],'vertexSource')
    bloom=runtime.split('bool Q1670EnsureProgramsQ1670() {',1)[1]
    post=runtime.split('bool Q1280EnsurePostProgram() {',1)[1]
    result['adapt']=[extract(adapt,'adaptVertex'),extract(adapt,'adaptFragment')]
    for kind,name in [('copy','copyFragment'),('bright','brightVerticalFragment'),('blur','horizontalFragment')]:
        result[kind]=[bloom_vertex,extract(bloom,name)]
    result['post']=[extract(post,'vertexSource'),extract(post,'fragmentSource')]
    ui=(ROOT/'app/src/main/cpp/ui/interaction/fo3-interaction-hud-assets.h').read_text()
    result['ui']=[extract(ui,'vs'),extract(ui,'fs')]
    notification=(ROOT/'app/src/main/cpp/ui/interaction/fo3-item-notification-renderer.h').read_text()
    result['notification']=[re.search(r'const char\* '+name+r'\s*=\s*R"\((.*?)\)";',notification,re.S)[1] for name in ['vs','fs']]
    loading=(ROOT/'app/src/main/cpp/ui/loading/fo3-loading-vr.h').read_text().split('inline bool EnsureProgram() {',1)[1]
    result['loading']=[re.search(r'const char\* '+name+r'\s*=\s*R"\((.*?)\)";',loading,re.S)[1].replace(';out',';\nout').replace(';in ',';\nin ') for name in ['vs','fs']]
    return result

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
    header = ('#version 450\n#extension GL_EXT_multiview : require\n'+
              ('layout(std140,set=0,binding=0) uniform Material {\n'+'\n'.join(members)+'\n};\n' if members else '')+
              'layout(std140,set=0,binding=1) uniform Stereo { mat4 fqViewProjection[2]; vec4 fqEye[2]; mat4 fqSkyViewProjection[2]; mat4 fqClipFromLeft[2]; vec4 fqTarget; };\n')
    varyings = list(dict.fromkeys(m[3] for m in IO.findall(pair[0]) if m[1]=='out'))
    converted = []
    for stage,source in enumerate(pair):
        source = re.sub(r'#version[^\n]*','',source)
        source = source.replace('gl_VertexID','gl_VertexIndex')
        source = re.sub(r'\bprecision\s+\w+\s+\w+\s*;','',source)
        source = UNIFORM.sub('',source)
        source = re.sub(r'\b(highp|mediump|lowp)\s+','',source)
        def io(match):
            space,direction,typ,name=match.groups()
            loc = 0 if stage==1 and direction=='out' else varyings.index(name)
            return f'{space}layout(location={loc}) {direction} {typ} {name};'
        source = IO.sub(io,source)
        # Preserve original GL clip coordinates for FO3 fog, remap position only.
        if stage==0 and kind in ('ui','notification','loading'):
            source=re.sub(r'gl_Position\s*=\s*uMvp\s*\*\s*vec4\((aPos|aPosition),\s*1(?:\.0)?\);',
                r'vec4 fqClip=fqClipFromLeft[gl_ViewIndex]*uMvp*vec4(\1,1.0);gl_Position=vec4(fqClip.x,(fqTarget.x>0.5?fqClip.y:-fqClip.y),(fqClip.z+fqClip.w)*0.5,fqClip.w);',source)
        if stage==0 and kind not in ('ui','notification','loading'):
            source = re.sub(r'\buMvp\b','fqSkyViewProjection[gl_ViewIndex]' if kind=='sky' else 'fqViewProjection[gl_ViewIndex]',source)
            if kind in ('sky','water'):
                source=source.replace('gl_Position = '+('fqSkyViewProjection' if kind=='sky' else 'fqViewProjection')+'[gl_ViewIndex] * vec4(aPosition, 1.0);',
                    'vec4 fqClip='+('fqSkyViewProjection' if kind=='sky' else 'fqViewProjection')+'[gl_ViewIndex]*vec4(aPosition,1.0); gl_Position=vec4(fqClip.x,(fqTarget.x>0.5?fqClip.y:-fqClip.y),(fqClip.z+fqClip.w)*0.5,fqClip.w);')
            source = source.replace('gl_Position = q1532Clip;',
                'gl_Position = vec4(q1532Clip.x, -q1532Clip.y, (q1532Clip.z+q1532Clip.w)*0.5, q1532Clip.w);')
            source = re.sub(r'\buEyePositionVertexQ1630\b','fqEye[gl_ViewIndex].xyz',source)
        else:
            source = re.sub(r'\buEyePosition\b','fqEye[gl_ViewIndex].xyz',source)
        if kind=='water':
            source = re.sub(r'\buMvp\b','fqViewProjection[gl_ViewIndex]',source)
            # Sample the matching native layer; Vulkan viewport Y is inverted
            # relative to the original projected GL coordinates. Depth stays [0,1].
            source = source.replace('displacedNdc * 0.5 + 0.5','vec2(displacedNdc.x, -displacedNdc.y) * 0.5 + 0.5')
            source = source.replace('reflectionClip.xy / reflectionClip.w * 0.5 + 0.5','vec2(reflectionClip.x, -reflectionClip.y) / reflectionClip.w * 0.5 + 0.5')
            for sampler_name in ('uSceneColor','uSceneDepth','uReflectionMap'):
                source = re.sub(r'texture\('+sampler_name+r', (\w+)\)', r'texture('+sampler_name+r', vec3(\1, float(gl_ViewIndex)))',source)
        fullscreen=kind in ('adapt','copy','bright','blur','post')
        if fullscreen:
            for sampler_name,_ in samplers:
                source=source.replace('texture('+sampler_name+',','fqSample_'+sampler_name+'(')
        sampler_text = '\n'.join(f'layout(set=0,binding={i+2}) uniform {"sampler2DArray" if fullscreen or (kind=="water" and name in ("uSceneColor","uSceneDepth","uReflectionMap")) else typ} {name};'
                                 for i,(name,typ) in enumerate(samplers))
        if fullscreen:
            sampler_text+='\n'+'\n'.join('vec4 fqSample_'+name+'(vec2 uv){return texture('+name+',vec3(uv,'+
                ('0.0' if kind=='adapt' else 'float(gl_ViewIndex)')+'));}' for name,_ in samplers)
        text=header+sampler_text+'\n'+source
        if stage==1 and kind in ('world','terrain'):
            prefix,main=text.split('void main()',1)
            constants='layout(constant_id=1) const bool fqNoLocalLights=false;\n'
            main=re.sub(r'\buLocalLightCount\b','(fqNoLocalLights?0:uLocalLightCount)',main)
            if kind=='world':
                constants+='layout(constant_id=0) const int fqLightingMode=0;\nlayout(constant_id=2) const bool fqNoEnvironmentPass=false;\n'
                main=re.sub(r'\buEnvironmentPassQ2050\b','(fqNoEnvironmentPass?0.0:uEnvironmentPassQ2050)',main)
                main=re.sub(r'\buNoLighting\b','(fqLightingMode==1?1.0:(fqLightingMode==2?0.0:uNoLighting))',main)
            text=prefix+constants+'void main()'+main
        converted.append(text)
    return converted,metadata,samplers,max(16,offset)

def no_discard_fragment(source, kind):
    """Equivalent only when the draw's original discard guards are disabled.

    Keep the canonical material and ABI; remove complete inactive branches,
    rather than forcing early tests on alpha-tested or spatially clipped draws.
    Fail the build if a newly added discard needs another eligibility guard.
    """
    guards = ['if (uWaterReflectionClipEnabledQ2090 > 0.5']
    if kind == 'world':
        guards.append('if (uNativeLodClipEnabledQ1810 > 0.5)')
    for guard in guards:
        begin = source.index(guard)
        end = source.index('{', begin) + 1
        depth = 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        source = source[:begin] + source[end:]
    if kind == 'world':
        alpha = 'if (uAlphaTest > 0.5 && alpha < uAlphaThreshold) discard;'
        assert source.count(alpha) == 1
        source = source.replace(alpha, '')
    if re.search(r'\bdiscard\s*;', source):
        raise ValueError('Unaccounted discard in '+kind)
    return source

def build(output,compiler):
    output.mkdir(parents=True,exist_ok=True)
    cpp=['#pragma once','#include <cstdint>','#include <cstddef>','#include <iterator>','namespace fqvk::shaders {',
         'struct Field { const char* name; uint32_t offset, size, components, count; bool integer; };',
         'struct Sampler { const char* name; bool cube; };']
    for name,pair in sources().items():
        converted,meta,samplers,size = port(pair,name)
        cpp.append(f'inline constexpr uint32_t {name}UniformSize={size};')
        cpp.append(f'inline constexpr Field {name}Fields[]={{')
        for field,offset,length,typ,count in meta:
            components={'mat4':16,'vec4':4,'vec3':3,'vec2':2,'float':1,'int':1}[typ]
            cpp.append(f'{{"{field}",{offset},{length},{components},{max(1,count)},{str(typ=="int").lower()}}},')
        if not meta:cpp.append('{nullptr,0,0,0,0,false},')
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
    for name in ('world', 'terrain'):
        text = no_discard_fragment(port(sources()[name], name)[0][1], name)
        path = output/f'{name}_solid.frag';path.write_text(text)
        spv = path.with_suffix('.frag.spv')
        command = ([compiler,'-V','--target-env','vulkan1.1',str(path),'-o',str(spv)]
                   if 'glslang' in Path(compiler).name else
                   [compiler,'--target-env=vulkan1.1',str(path),'-o',str(spv)])
        subprocess.run(command,check=True)
        data=spv.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
        cpp.append(f'inline constexpr uint32_t {name}_solid_frag[]={{'+','.join(hex(x) for x in words)+'};')
    cpp.append('struct SolidFragment {const uint32_t* code; size_t bytes;};')
    cpp.append('inline constexpr SolidFragment solidFragments[]={{world_solid_frag,sizeof(world_solid_frag)},{terrain_solid_frag,sizeof(terrain_solid_frag)}};')
    cpp.append('struct Program { const Field* fields; size_t fieldCount; const Sampler* samplers; size_t samplerCount; uint32_t uniformSize; const uint32_t* vertex; size_t vertexBytes; const uint32_t* fragment; size_t fragmentBytes; };')
    cpp.append('inline constexpr Program programs[]={')
    for name in sources():
        cpp.append('{'+f'{name}Fields,{len(port(sources()[name],name)[1])},{name}Samplers,std::size({name}Samplers),{name}UniformSize,{name}_vert,sizeof({name}_vert),{name}_frag,sizeof({name}_frag)'+'},')
    cpp.append('};')
    motion=(ROOT/'app/src/main/cpp/rendering/vulkan/fo3-vulkan-motion-shaders.h').read_text()
    for name,text in re.findall(r'const char\* (\w+)=R"FQMV\((.*?)\)FQMV"',motion,re.S):
        stage=name.rsplit('_',1)[1]
        path=output/f'motion_{name}.{stage}';path.write_text(text)
        spv=path.with_suffix(path.suffix+'.spv')
        command=([compiler,'-V','--target-env','vulkan1.1',str(path),'-o',str(spv)] if 'glslang' in Path(compiler).name else [compiler,'--target-env=vulkan1.1',str(path),'-o',str(spv)])
        subprocess.run(command,check=True)
        data=spv.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
        cpp.append(f'inline constexpr uint32_t motion_{name}[]={{'+','.join(hex(x) for x in words)+'};')
    # Additional outputs reuse the original material/film math verbatim.
    variants={}
    for name in ('world','terrain','sky','water'):
        original=port(sources()[name],name)[0][1]
        for solid in ((False,True) if name in ('world','terrain') else (False,)):
            text=no_discard_fragment(original,name) if solid else original
            colour=re.search(r'layout\(location=0\) out vec4 (\w+)',text)[1]
            text=text.replace('void main()', 'void fqOriginalMain()',1)
            variants[name+('_solid' if solid else '')+'_split']=text+f'\nlayout(location=1) out vec4 fqOpaque;void main(){{fqOriginalMain();fqOpaque={colour};}}'
    text=port(sources()['post'],'post')[0][1]
    colour=re.search(r'layout\(location=0\) out vec4 (\w+)',text)[1]
    uv=re.search(r'layout\(location=0\) in vec2 (\w+)',text)[1]
    text=text.replace('void main()', 'void fqOriginalMain()',1)
    variants['composition_split']=text+f'''\nlayout(set=0,binding=5) uniform sampler2DArray fqFullFinal;
layout(location=1) out vec4 fqNativeOverlay;
layout(constant_id=0) const bool fqCompactFilm=false;
vec3 fqSrgb(vec3 c){{c=clamp(c,0.0,1.0);return mix(12.92*c,1.055*pow(c,vec3(1.0/2.4))-0.055,greaterThan(c,vec3(0.0031308)));}}
void main(){{fqOriginalMain();
 vec3 base=vec3(unpackHalf2x16(packHalf2x16({colour}.rg)),unpackHalf2x16(packHalf2x16({colour}.bb)).x);
 vec3 full=texture(fqFullFinal,vec3({uv},float(gl_ViewIndex))).rgb;
 vec3 fullCode=floor((fqCompactFilm?fqSrgb(full):clamp(full,0.0,1.0))*255.0+0.5);
 vec3 baseCode=floor((fqCompactFilm?fqSrgb(base):clamp(base,0.0,1.0))*255.0+0.5);
 bool changed=any(greaterThan(abs(fullCode-baseCode),vec3(1)));
 {colour}=vec4(base,1);fqNativeOverlay=changed?vec4(full,1):vec4(0);
}}'''
    for name,text in variants.items():
        path=output/f'{name}.frag';path.write_text(text);spv=path.with_suffix('.frag.spv')
        command=([compiler,'-V','--target-env','vulkan1.1',str(path),'-o',str(spv)] if 'glslang' in Path(compiler).name else [compiler,'--target-env=vulkan1.1',str(path),'-o',str(spv)])
        subprocess.run(command,check=True)
        data=spv.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
        cpp.append(f'inline constexpr uint32_t {name}_frag[]={{'+','.join(hex(x) for x in words)+'};')
    cpp.append('}')
    (output/'fo3-vulkan-shaders.h').write_text('\n'.join(cpp)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--compiler',required=True)
    args=p.parse_args();build(args.output,args.compiler)
