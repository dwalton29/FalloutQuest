// Opaque world migration: layered 2x render-to-texture, then depth-preserving
// layer import into the existing per-eye 2x HDR/post renderer.
void QMVShutdown() {
    auto& s=gMultiview;
    if(s.program) glDeleteProgram(s.program);
    if(s.copyProgram) glDeleteProgram(s.copyProgram);
    if(s.copyVao) glDeleteVertexArrays(1,&s.copyVao);
    if(s.color) glDeleteTextures(1,&s.color);
    if(s.depth) glDeleteTextures(1,&s.depth);
    if(s.fbo) glDeleteFramebuffers(1,&s.fbo);
    const bool supported=s.supported; const auto attach=s.attach;
    s=QMVState{}; s.supported=supported; s.attach=attach;
    gMultiviewDrawing=false;
}

bool QMVBuildUniformMap() {
    auto& s=gMultiview;
    GLint count=0,length=0;
    glGetProgramiv(gProgram,GL_ACTIVE_UNIFORMS,&count);
    glGetProgramiv(gProgram,GL_ACTIVE_UNIFORM_MAX_LENGTH,&length);
    std::vector<char> name(static_cast<size_t>(std::max(length,1)));
    for(GLint i=0;i<count;++i) {
        GLint size=0; GLenum type=0;
        glGetActiveUniform(gProgram,static_cast<GLuint>(i),length,nullptr,&size,&type,name.data());
        switch(type) {
            case GL_FLOAT:case GL_FLOAT_VEC2:case GL_FLOAT_VEC3:case GL_FLOAT_VEC4:
            case GL_FLOAT_MAT4:case GL_INT:case GL_BOOL:case GL_SAMPLER_2D:case GL_SAMPLER_CUBE:break;
            default:Q6H_LOGE("Multiview unsupported uniform type=0x%X name=%s",type,name.data());return false;
        }
        const std::string base(name.data());
        for(GLint element=0;element<size;++element) {
            std::string indexed=base;
            if(size>1) {
                const auto at=indexed.find("[0]");
                if(at==std::string::npos) return false;
                indexed.replace(at,3,"["+std::to_string(element)+"]");
            }
            const GLint source=glGetUniformLocation(gProgram,indexed.c_str());
            const GLint target=glGetUniformLocation(s.program,indexed.c_str());
            if(source<0) continue;
            if(s.locations.size()<=static_cast<size_t>(source)) s.locations.resize(source+1,-1);
            s.locations[source]=target;
            if(target>=0) s.uniforms.push_back({source,target,type});
        }
    }
    s.mvp=glGetUniformLocation(s.program,"uStereoMvp[0]");
    s.eye=glGetUniformLocation(s.program,"uStereoEye[0]");
    return s.mvp>=0 && s.eye>=0;
}

void QMVCopyUniforms() {
    const auto& s=gMultiview;
    for(const auto& uniform:s.uniforms) {
        GLfloat value[16]{}; GLint integer=0;
        switch(uniform.type) {
            case GL_FLOAT: glGetUniformfv(gProgram,uniform.source,value);glUniform1fv(uniform.target,1,value);break;
            case GL_FLOAT_VEC2: glGetUniformfv(gProgram,uniform.source,value);glUniform2fv(uniform.target,1,value);break;
            case GL_FLOAT_VEC3: glGetUniformfv(gProgram,uniform.source,value);glUniform3fv(uniform.target,1,value);break;
            case GL_FLOAT_VEC4: glGetUniformfv(gProgram,uniform.source,value);glUniform4fv(uniform.target,1,value);break;
            case GL_FLOAT_MAT4: glGetUniformfv(gProgram,uniform.source,value);glUniformMatrix4fv(uniform.target,1,GL_FALSE,value);break;
            default:glGetUniformiv(gProgram,uniform.source,&integer);glUniform1i(uniform.target,integer);break;
        }
    }
    glUniformMatrix4fv(s.mvp,2,GL_FALSE,gStereoMvp[0]);
    glUniform3fv(s.eye,2,gStereoEye[0]);
}

bool QMVEnsure(GLsizei width,GLsizei height) {
    auto& s=gMultiview;
    if(!s.supported || s.failed || !s.attach || !gStereoMvpReady || width<=0 || height<=0) return false;
    if(s.program && s.width==width && s.height==height && s.sourceProgram==gProgram) return true;
    QMVShutdown();
    GLint fbo=0,program=0,vao=0,active=0,array=0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&array);
    s.sourceProgram=gProgram; s.width=width;s.height=height;
    s.program=CreateQ6HProgram(true);
    bool ok=s.program && QMVBuildUniformMap();
    const char* vertex=R"(#version 300 es
        out highp vec2 uv;
        void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv=p;gl_Position=vec4(p*2.0-1.0,0,1);})";
    const char* fragment=R"(#version 300 es
        precision highp float;
        uniform highp sampler2DArray uColor;
        uniform highp sampler2DArray uDepth;
        uniform float uLayer;
        in highp vec2 uv;
        out vec4 fragColor;
        void main(){vec3 p=vec3(uv,uLayer);float d=texture(uDepth,p).r;
            vec4 c=texture(uColor,p);if(d>=1.0 || c.a<=0.0)discard;
            // Only fully opaque materials enter this target. Resolved alpha is
            // geometric coverage, so recover sample colour before 2x A2C import.
            fragColor=vec4(c.rgb/max(c.a,0.0001),c.a);gl_FragDepth=d;})";
    const GLuint vs=CompileQ6HShader(GL_VERTEX_SHADER,vertex),fs=CompileQ6HShader(GL_FRAGMENT_SHADER,fragment);
    if(vs && fs) {
        s.copyProgram=glCreateProgram();glAttachShader(s.copyProgram,vs);glAttachShader(s.copyProgram,fs);
        glLinkProgram(s.copyProgram);GLint linked=0;glGetProgramiv(s.copyProgram,GL_LINK_STATUS,&linked);ok=ok && linked;
    } else ok=false;
    if(vs) glDeleteShader(vs);
    if(fs) glDeleteShader(fs);
    if(ok) {
        while(glGetError()!=GL_NO_ERROR){}
        glGenTextures(1,&s.color);glBindTexture(GL_TEXTURE_2D_ARRAY,s.color);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,q1280PostInternalFormat,width,height,2);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenTextures(1,&s.depth);glBindTexture(GL_TEXTURE_2D_ARRAY,s.depth);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_DEPTH_COMPONENT24,width,height,2);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&s.fbo);glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);
        s.attach(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,s.color,0,2,0,2);
        s.attach(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,s.depth,0,2,0,2);
        ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE && glGetError()==GL_NO_ERROR;
        glGenVertexArrays(1,&s.copyVao);
        glUseProgram(s.copyProgram);
        s.copyLayer=glGetUniformLocation(s.copyProgram,"uLayer");
        glUniform1i(glGetUniformLocation(s.copyProgram,"uColor"),6);
        glUniform1i(glGetUniformLocation(s.copyProgram,"uDepth"),7);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(fbo));glUseProgram(static_cast<GLuint>(program));
    glBindVertexArray(static_cast<GLuint>(vao));glBindTexture(GL_TEXTURE_2D_ARRAY,static_cast<GLuint>(array));
    glActiveTexture(static_cast<GLenum>(active));
    if(!ok) {QMVShutdown();s.failed=true;Q6H_LOGW("Multiview setup failed: sequential fallback");}
    else Q6H_LOGI("Multiview opaque target ready: %dx%d layers=2 samples=2 shader=OVR_multiview2",width,height);
    return ok;
}

bool QMVRenderOpaque(GLsizei width,GLsizei height) {
    // Layer import requires the existing 2x post target, preserving the quality
    // policy and rejecting the 1x allocation fallback for this migration stage.
    if(!q1280PostActive || !q2060MsaaActive || q2060MsaaSamples!=2 || !QMVEnsure(width,height)) return false;
    auto& s=gMultiview;
    GLint fbo=0,program=0,vao=0,depthFunc=0,active=0,viewport[4]{};
    GLboolean depthMask=GL_TRUE;GLfloat clear[4]{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_VIEWPORT,viewport);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    const bool depthTest=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);
    const bool a2c=glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE),scissor=glIsEnabled(GL_SCISSOR_TEST);
    GLint arrays[2]{};
    for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE6+i);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&arrays[i]);}
    bool ok=true;
    if(s.frame!=gStereoFrame || s.storage!=gObjects.data() || s.objectCount!=gObjects.size()) {
        const auto renderStarted=std::chrono::steady_clock::now();
        s.drawCalls=0;
        while(glGetError()!=GL_NO_ERROR){}
        glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);glViewport(0,0,width,height);
        glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glUseProgram(s.program);QMVCopyUniforms();
        gMultiviewDrawing=true;
        Q1990RenderNativeLod(false);Q2017RenderOpaqueDetailedInstanced();
        gMultiviewDrawing=false;
        // Switching away resolves both layers of the implicit MSAA attachments.
        glBindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(fbo));
        ok=glGetError()==GL_NO_ERROR;
        s.renderUs=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-renderStarted).count());
        if(ok){s.frame=gStereoFrame;s.storage=gObjects.data();s.objectCount=gObjects.size();++s.drawSubmissions;}
    }
    if(ok) {
        const auto importStarted=std::chrono::steady_clock::now();
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glDisable(GL_SCISSOR_TEST);
        glUseProgram(s.copyProgram);glBindVertexArray(s.copyVao);
        glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D_ARRAY,s.color);
        glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D_ARRAY,s.depth);
        glUniform1f(s.copyLayer,static_cast<float>(gCurrentStereoEye));
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDrawArrays(GL_TRIANGLES,0,3);
        ok=glGetError()==GL_NO_ERROR;
        s.importUs=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-importStarted).count());
    }
    glBindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(fbo));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glUseProgram(static_cast<GLuint>(program));glBindVertexArray(static_cast<GLuint>(vao));
    glDepthFunc(static_cast<GLenum>(depthFunc));glDepthMask(depthMask);glClearColor(clear[0],clear[1],clear[2],clear[3]);
    if(depthTest)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
    if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if(a2c)glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
    for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE6+i);glBindTexture(GL_TEXTURE_2D_ARRAY,static_cast<GLuint>(arrays[i]));}
    glActiveTexture(static_cast<GLenum>(active));
    if(!ok){s.failed=true;gMainBatches.frame=~uint64_t{0};Q6H_LOGE("Multiview draw failed: sequential fallback");}
    else if(gCurrentStereoEye==0 && s.drawSubmissions%240u==1u)
        Q6H_LOGI("Multiview opaque: frame=%llu worldSubmissions=%llu drawCalls=%llu submitCpuUs=%llu importCpuUs=%llu layers=2 LAND=sequential",
            static_cast<unsigned long long>(gStereoFrame),static_cast<unsigned long long>(s.drawSubmissions),static_cast<unsigned long long>(s.drawCalls),
            static_cast<unsigned long long>(s.renderUs),static_cast<unsigned long long>(s.importUs));
    return ok;
}
