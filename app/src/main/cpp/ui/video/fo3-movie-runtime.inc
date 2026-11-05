#include "video/fo3-movie-playback.h"
#include "video/fo3-movie-controls.h"
namespace fo3movieui {
static fo3movie::Playback playback;
static const fo3player::Player* owner=nullptr;static uint64_t generation=0,serial=0;
static fo3movie::Controls controls;static bool anchored=false;
static std::array<float,3> anchor{};static float yaw=0;
static GLuint texture=0;static std::shared_ptr<const fo3movie::Decoded> uploaded;
static fo3menuui::Document document;
static fo3dialogue::Panel failure;
static void Reset(){
    playback.Stop();uploaded.reset();document.surface.Shutdown();document={};
    if(texture){glDeleteTextures(1,&texture);texture=0;}
    fo3menuui::Shared().dynamicImages.erase("fq_movie_frame");
    owner=nullptr;serial=generation=0;failure={};controls={};anchored=false;fo3audio::Movie(0);
}
static void Upload(const std::shared_ptr<const fo3movie::Decoded>& frame){
    if(!frame||frame==uploaded)return;
    if(!texture)texture=fo3hudassets::UploadTexture(frame->width,frame->height,frame->rgba.data());
    else{
        glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,frame->width,frame->height,0,GL_RGBA,GL_UNSIGNED_BYTE,frame->rgba.data());
#ifdef FQ_ENABLE_VULKAN
        fqvk::Texture(texture,frame->width,frame->height,frame->rgba.data(),false,false);
#endif
    }
    fo3menuui::Shared().dynamicImages["fq_movie_frame"]=texture;uploaded=frame;
    auto& l=document.layout;l.width=1800;l.height=1012.5f;
    if(!document.ready){fo3menuui::Measure(l);
        document.ready=l.Load("movie",[](const std::string&,std::string& xml){xml="<menu name='Movie'><image name='Frame'><x>0</x><y>0</y><width>1800</width><height>1012.5</height><filename>fq_movie_frame</filename><systemcolor>0</systemcolor></image></menu>";return true;});
    }
    // Keep the source aspect ratio on a comfortable fixed theatre plane.
    const float aspect=float(frame->width)/frame->height;float width=1800,height=1012.5f;
    if(owner&&(owner->Snapshot().movie.flags&8)){height=width/aspect;if(height>1012.5f){height=1012.5f;width=height*aspect;}}
    l.Set("Frame","x",(1800-width)*.5);l.Set("Frame","y",(1012.5-height)*.5);l.Set("Frame","width",double(width));l.Set("Frame","height",double(height));document.Build();
}
}
void ShutdownFo3Movie(){fo3movieui::Reset();}
void PauseFo3Movie(){fo3movieui::playback.Tick(0,false,gFo3Title.volume[0]);}
void UpdateFo3Movie(double now,bool a,bool b,bool focused,bool inputFocused,const float* head){
    using namespace fo3movieui;
    if(!gPlayerSession||gPlayerSession->saveBlocked||!gPlayerSession->player.Snapshot().movie.owner){if(owner)Reset();return;}
    auto& player=gPlayerSession->player;const auto request=player.Snapshot().movie;
    if(owner!=&player||generation!=player.ScriptRuntimeGeneration()||serial!=request.serial){
        Reset();owner=&player;generation=player.ScriptRuntimeGeneration();serial=request.serial;
        const auto path=fo3movie::ResolvePath(fo3assets::FalloutDataPath(""),request.path);
        playback.Start(path);fo3audio::Movie(request.flags);
    }
    if(focused&&!anchored){yaw=head[3];anchor={head[0]-std::sin(yaw)*2.f,head[1],head[2]-std::cos(yaw)*2.f};anchored=true;}
    playback.Tick(now,focused,gFo3Title.volume[0]);const auto state=playback.Status();
    const bool failed=state==fo3movie::Playback::State::Failed;
    const bool requested=controls.Step(request.serial,a,b,focused&&inputFocused,failed,(request.flags&1)!=0);
    const bool complete=state==fo3movie::Playback::State::Finished||requested;
    if(complete){if(player.CompleteMovie(request.serial))FlushFo3PlayerState();Reset();return;}
    if(failed&&!failure.visible)Q6H_LOGW("MOVIE FAILED file=%s reason=%s",request.path.c_str(),playback.Error().c_str());
    failure.visible=failed;failure.yaw=yaw;failure.anchor=anchor;
    failure.title="Movie unavailable";failure.subtitle=request.path+"\nInstall the original Data/Video files, or continue without this movie.";
    failure.choices={"Continue without movie"};failure.selected=0;failure.revision=request.serial;
    if(!failed)Upload(playback.Frame());
}
void RenderFo3Movie(const float* vp){
    using namespace fo3movieui;if(!owner)return;
    auto model=fo3anim::Identity();model[0]=std::cos(yaw);model[2]=-std::sin(yaw);model[5]=1;model[8]=std::sin(yaw);model[10]=std::cos(yaw);
    model[12]=anchor[0];model[13]=anchor[1];model[14]=anchor[2];model[15]=1;
    fo3anim::Matrix view{};std::copy_n(vp,16,view.begin());const auto mvp=fo3anim::Multiply(view,model);
    if(failure.visible&&gPlayerSession)fo3menuui::Message(mvp.data(),failure,gPlayerSession->player);
    else if(uploaded)fo3menuui::World(document.surface,document.layout,mvp.data());
}
