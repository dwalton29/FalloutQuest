#pragma once
// Include before the legacy GL interception macros: this isolated pass restores
// driver state and the caller invalidates the world cache at its boundary.
#include <GLES3/gl3.h>
#include "rendering/render-submission.h"
#include "fo3-motion-capture.h"
#include "fo3-motion-vectors.h"
#include "fo3-dynamic-motion.h"
#include <string>
namespace fqmotion {
struct GlState {
    GLint draw=0,read=0,viewport[4]{},program=0,vao=0,array=0,active=0,depthFunc=0,front=0,cull=0;
    GLint unpack=0,alignment=0,rowLength=0,skipRows=0,skipPixels=0;
    static constexpr unsigned units[]={0,1,6,7};
    GLint textures[4]{},samplers[4]{};
    GLboolean depthMask=0,colour[4]{};
    GLfloat range[2]{},offsetFactor=0,offsetUnits=0;
    static constexpr GLenum caps[]={GL_DEPTH_TEST,GL_BLEND,GL_CULL_FACE,GL_POLYGON_OFFSET_FILL,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_RASTERIZER_DISCARD};
    GLboolean enabled[9]{};
    GlState(){
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetBooleanv(GL_COLOR_WRITEMASK,colour);
        glGetFloatv(GL_DEPTH_RANGE,range);glGetIntegerv(GL_FRONT_FACE,&front);glGetIntegerv(GL_CULL_FACE_MODE,&cull);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&offsetFactor);glGetFloatv(GL_POLYGON_OFFSET_UNITS,&offsetUnits);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);
        for(unsigned i=0;i<9;++i)enabled[i]=glIsEnabled(caps[i]);
        for(unsigned i=0;i<4;++i){glActiveTexture(GL_TEXTURE0+units[i]);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);}
    }
    ~GlState(){
        for(unsigned i=0;i<4;++i){glActiveTexture(GL_TEXTURE0+units[i]);glBindTexture(GL_TEXTURE_2D,textures[i]);glBindSampler(units[i],samplers[i]);}
        glActiveTexture(active);glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,array);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glDepthFunc(depthFunc);glDepthMask(depthMask);glColorMask(colour[0],colour[1],colour[2],colour[3]);glDepthRangef(range[0],range[1]);
        glFrontFace(front);glCullFace(cull);glPolygonOffset(offsetFactor,offsetUnits);
        for(unsigned i=0;i<9;++i){if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);}
    }
};
class Gles {
    GLuint staticProgram_=0,dynamicProgram_=0,debugProgram_=0,fbo_=0,vao_=0,debugSampler_=0;
    struct PreviousGpu {
        GLuint positions=0,palette=0;
        size_t positionBytes=0,paletteRows=0;
        uint64_t uploaded=0,used=0;
    };
    std::unordered_map<uint64_t,PreviousGpu> previousGpu_;
    size_t previousBytes_=0;
    uint64_t uploadedBytes_=0,allocations_=0;
    bool failed_=false;
    static void Delete(PreviousGpu& gpu){if(gpu.positions)glDeleteBuffers(1,&gpu.positions);if(gpu.palette)glDeleteTextures(1,&gpu.palette);}
    std::string error_;
    GLuint Program(const char* vertex,const char* fragment){
        GLuint result=glCreateProgram();
        for(unsigned stage=0;stage<2;++stage){
            GLuint shader=glCreateShader(stage?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER);const char* source=stage?fragment:vertex;
            glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
            if(!ok){char log[2048]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);error_=log;glDeleteShader(shader);glDeleteProgram(result);return 0;}
            glAttachShader(result,shader);glDeleteShader(shader);
        }
        glLinkProgram(result);GLint ok=0;glGetProgramiv(result,GL_LINK_STATUS,&ok);
        if(!ok){char log[2048]{};glGetProgramInfoLog(result,sizeof(log),nullptr,log);error_=log;glDeleteProgram(result);return 0;}return result;
    }
    static void Matrix(GLuint p,const char* name,const fqspacewarp::Matrix& value){glUniformMatrix4fv(glGetUniformLocation(p,name),1,GL_FALSE,value.data());}
    static void Int(GLuint p,const char* name,int value){glUniform1i(glGetUniformLocation(p,name),value);}
    static void Float(GLuint p,const char* name,float value){glUniform1f(glGetUniformLocation(p,name),value);}
    static void Attribute(GLuint index,GLint count,GLsizei stride,size_t offset){glEnableVertexAttribArray(index);glVertexAttribPointer(index,count,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offset));}
public:
    Gles()=default;Gles(const Gles&)=delete;Gles& operator=(const Gles&)=delete;
    bool Failed()const{return failed_;}
    uint64_t UploadedBytes()const{return uploadedBytes_;}
    uint64_t Allocations()const{return allocations_;}
    const std::string& Error()const{return error_;}
    bool Initialize(){
        if(fqrender::nativeOnly)return false;
        if(staticProgram_&&dynamicProgram_&&debugProgram_)return true;
        GlState state;Shutdown();error_.clear();failed_=false;staticProgram_=Program(fqspacewarp::StaticVertex,fqspacewarp::StaticFragment);
        if(staticProgram_)dynamicProgram_=Program(fqspacewarp::DynamicVertex,fqspacewarp::DynamicFragment);
        if(dynamicProgram_)debugProgram_=Program(fqspacewarp::StaticVertex,fqspacewarp::DebugFragment);
        if(!staticProgram_||!dynamicProgram_||!debugProgram_){Shutdown();return false;}
        glGenFramebuffers(1,&fbo_);glGenVertexArrays(1,&vao_);
        glGenSamplers(1,&debugSampler_);glSamplerParameteri(debugSampler_,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glSamplerParameteri(debugSampler_,GL_TEXTURE_MAG_FILTER,GL_NEAREST);return true;
    }
    void Shutdown(){
        if(staticProgram_)glDeleteProgram(staticProgram_);if(dynamicProgram_)glDeleteProgram(dynamicProgram_);
        if(debugProgram_)glDeleteProgram(debugProgram_);if(debugSampler_)glDeleteSamplers(1,&debugSampler_);
        if(fbo_)glDeleteFramebuffers(1,&fbo_);if(vao_)glDeleteVertexArrays(1,&vao_);
        for(auto& entry:previousGpu_)Delete(entry.second);previousGpu_.clear();previousBytes_=0;
        staticProgram_=dynamicProgram_=debugProgram_=debugSampler_=fbo_=vao_=0;
    }
    bool Render(const Capture& capture,unsigned eyeIndex,GLuint sourceDepth,GLuint motion,GLuint depth,int width,int height,const std::array<float,3>& scale){
        if(fqrender::nativeOnly)return false;
        if(!staticProgram_||!dynamicProgram_||!capture.history||!capture.mainDrawn||!sourceDepth||!motion||!depth||width<=0||height<=0||eyeIndex>1)return false;
        const auto& history=*capture.history;const auto& current=history.CurrentFrame();
        const auto& prior=history.SubmittedFrames()?history.PreviousFrame():current;
        fqspacewarp::Matrix inverse{};if(!fqspacewarp::Inverse(current.viewProjection[eyeIndex],inverse))return false;
        GlState state;
        // Previous-pose buffers survive both stereo eyes and subsequent frames.
        // Upload each identity once per logical frame; retire departed geometry.
        for(auto it=previousGpu_.begin();it!=previousGpu_.end();){
            if(it->second.used+1<current.serial){previousBytes_-=it->second.positionBytes+it->second.paletteRows*sizeof(fqskin::PaletteRow);Delete(it->second);it=previousGpu_.erase(it);}
            else ++it;
        }
        // An earlier GL error must not be attributed to a successful producer.
        // Errors from this pass are consumed below and reported via false.
        while(glGetError()!=GL_NO_ERROR){}
        glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,motion,0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        const GLenum buffer=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&buffer);glReadBuffer(buffer);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){failed_=true;error_="incomplete motion framebuffer";return false;}
        glViewport(0,0,width,height);for(GLenum cap:GlState::caps)glDisable(cap);
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthRangef(0,1);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        for(unsigned unit:GlState::units)glBindSampler(unit,0);
        glBindVertexArray(vao_);glUseProgram(staticProgram_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sourceDepth);
        Int(staticProgram_,"uDepth",0);Matrix(staticProgram_,"uInverseCurrentViewProjection",inverse);Matrix(staticProgram_,"uPreviousViewProjection",prior.viewProjection[eyeIndex]);
        glUniform3fv(glGetUniformLocation(staticProgram_,"uMotionScale"),1,scale.data());glDrawArrays(GL_TRIANGLES,0,3);
        glUseProgram(dynamicProgram_);glCullFace(GL_BACK);
        Matrix(dynamicProgram_,"uCurrentVP",current.viewProjection[eyeIndex]);Matrix(dynamicProgram_,"uPreviousVP",prior.viewProjection[eyeIndex]);
        glUniform3fv(glGetUniformLocation(dynamicProgram_,"uMotionScale"),1,scale.data());
        Int(dynamicProgram_,"uDiffuse",0);Int(dynamicProgram_,"uPalette",6);Int(dynamicProgram_,"uPreviousPalette",7);
        Int(dynamicProgram_,"uSceneDepth",1);glUniform2f(glGetUniformLocation(dynamicProgram_,"uTargetSize"),float(width),float(height));
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,sourceDepth);
        bool complete=true;
        for(const auto& draw:capture.draws){
            const auto* shape=history.Current(draw.identity);if(!shape){complete=false;continue;}
            const auto* previous=history.Previous(draw.identity,shape->geometry);
            if(!previous||previous->palette.size()!=shape->palette.size()||previous->deformedPositions.size()!=shape->deformedPositions.size()){previous=shape;complete=false;}
            if(draw.count<=0||draw.vertices<=0||!draw.vbo||(draw.skinMode&&(!draw.skinAttributes||!draw.paletteTexture||shape->palette.empty()))){complete=false;continue;}
            auto& gpu=previousGpu_[draw.identity];gpu.used=current.serial;
            const size_t bytes=previous->deformedPositions.size()*sizeof(float),rows=previous->palette.size();
            const size_t growth=(bytes>gpu.positionBytes?bytes-gpu.positionBytes:0)+(rows>gpu.paletteRows?(rows-gpu.paletteRows)*sizeof(fqskin::PaletteRow):0);
            if(growth>fqspacewarp::History::MaxHistoryBytes-previousBytes_){complete=false;continue;}
            if(gpu.uploaded!=current.serial){
                if(bytes){
                    if(!gpu.positions)glGenBuffers(1,&gpu.positions);glBindBuffer(GL_ARRAY_BUFFER,gpu.positions);
                    if(bytes>gpu.positionBytes){glBufferData(GL_ARRAY_BUFFER,bytes,nullptr,GL_DYNAMIC_DRAW);previousBytes_+=bytes-gpu.positionBytes;gpu.positionBytes=bytes;++allocations_;}
                    glBufferSubData(GL_ARRAY_BUFFER,0,bytes,previous->deformedPositions.data());
                    uploadedBytes_+=bytes;
                }
                if(draw.skinMode){
                    if(!gpu.palette)glGenTextures(1,&gpu.palette);glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,gpu.palette);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                    if(rows>gpu.paletteRows){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,8,rows,0,GL_RGBA,GL_FLOAT,nullptr);previousBytes_+=(rows-gpu.paletteRows)*sizeof(fqskin::PaletteRow);gpu.paletteRows=rows;++allocations_;}
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,8,rows,GL_RGBA,GL_FLOAT,previous->palette.data());
                    uploadedBytes_+=rows*sizeof(fqskin::PaletteRow);
                }
                gpu.uploaded=current.serial;
            }
            glBindBuffer(GL_ARRAY_BUFFER,draw.vbo);Attribute(0,3,18*sizeof(float),0);Attribute(4,2,18*sizeof(float),12*sizeof(float));Attribute(5,4,18*sizeof(float),14*sizeof(float));
            if(previous->deformedPositions.empty())Attribute(12,3,18*sizeof(float),0);
            else{
                if(previous->deformedPositions.size()!=size_t(draw.vertices)*3){complete=false;continue;}
                glBindBuffer(GL_ARRAY_BUFFER,gpu.positions);Attribute(12,3,3*sizeof(float),0);
            }
            Int(dynamicProgram_,"uSkinMode",draw.skinMode);
            if(draw.skinMode){
                glBindBuffer(GL_ARRAY_BUFFER,draw.skinAttributes);Attribute(10,4,8*sizeof(float),0);Attribute(11,4,8*sizeof(float),4*sizeof(float));
                glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,draw.paletteTexture);glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,gpu.palette);
            }else {glDisableVertexAttribArray(10);glDisableVertexAttribArray(11);}
            Matrix(dynamicProgram_,"uModel",shape->model);Matrix(dynamicProgram_,"uPreviousModel",previous->model);
            Int(dynamicProgram_,"uAlphaTest",draw.alphaTest);Int(dynamicProgram_,"uVertexAlpha",draw.vertexAlpha);Float(dynamicProgram_,"uAlpha",draw.alpha);Float(dynamicProgram_,"uThreshold",draw.threshold);
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,draw.diffuse);
            if(draw.cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);glFrontFace(draw.clockwise?GL_CW:GL_CCW);
            if(draw.decal){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-.65f,-1);}else glDisable(GL_POLYGON_OFFSET_FILL);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,draw.indexBuffer);
            if(draw.indexed)glDrawElements(GL_TRIANGLES,draw.count,GL_UNSIGNED_INT,nullptr);else glDrawArrays(GL_TRIANGLES,0,draw.count);
        }
        const bool error=glGetError()!=GL_NO_ERROR;
        if(error){failed_=true;error_="GLES motion pass error";}
        return complete&&!error;
    }
    bool Visualize(GLuint target,int width,int height,GLuint motion){
        if(fqrender::nativeOnly)return false;
        if(!debugProgram_||!motion)return false;
        GlState state;glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,width,height);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){failed_=true;error_="incomplete visualization framebuffer";return false;}
        for(GLenum cap:GlState::caps)glDisable(cap);
        glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glBindVertexArray(vao_);glUseProgram(debugProgram_);
        glActiveTexture(GL_TEXTURE0);glBindSampler(0,debugSampler_);glBindTexture(GL_TEXTURE_2D,motion);Int(debugProgram_,"uMotion",0);glDrawArrays(GL_TRIANGLES,0,3);
        if(glGetError()!=GL_NO_ERROR){failed_=true;error_="GLES visualization error";return false;}return true;
    }
};
} // namespace fqmotion
