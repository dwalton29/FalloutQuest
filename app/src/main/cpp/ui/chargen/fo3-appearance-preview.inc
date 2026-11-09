#include "ui/chargen/fo3-appearance-menu.h"

namespace fo3chargen {
using ::fo3chargen::Menu;
struct PreviewKey {
    fo3character::Appearance appearance;
    uint32_t screen=0;
    std::array<float,4> uv{0,0,1,1};
    bool female=false;
    bool operator==(const PreviewKey& other)const{
        return appearance==other.appearance&&screen==other.screen&&uv==other.uv&&female==other.female;
    }
};

static fo3scene::Preparation<Fo3RgbaTexture> preparation;
static PreviewKey pending,completed;
static bool hasCompleted=false;
static std::array<float,4> textureUv{0,0,1,1};
static uint64_t generation=UINT64_MAX,pendingGeneration=0;
static const fo3player::Player* playerOwner=nullptr;
static uint32_t cell=0;
static Menu menu;
static std::shared_ptr<const std::vector<fo3character::Control>> controls;
static fo3dialogue::Panel panel;

static const GpuObject* Screen(){
    const auto found=std::find_if(gObjects.begin(),gObjects.end(),[](const auto& object){
        return object.geneScreen&&!Fo3WorldReferenceHidden(object.refFormId);
    });
    return found==gObjects.end()?nullptr:&*found;
}

static bool RenderHead(const PreviewKey& key,const fo3character::Race& race,
                       const std::vector<fo3character::Control>& axes,Fo3RgbaTexture& image,
                       const std::atomic<bool>& cancelled){
    Fo3NpcActorQ230 source;
    if(!LoadFo3CharacterHead(key.appearance,key.female,race,axes,source)||
       cancelled.load(std::memory_order_acquire))return false;
    Q230ActorVisual actor;
    if(!Q230BuildNpcActor(source,actor,true))return false;

    // Keep the projector NIF's original UVs. Paint the portrait into the
    // oval screen's authored chart, leaving its original mesh and animation.
    image.width=image.height=key.screen?1024:256;
    const int width=image.width,height=image.height;
    const int left=std::clamp(int(key.uv[0]*width),0,width-1);
    const int top=std::clamp(int(key.uv[1]*height),0,height-1);
    const int right=std::clamp(int(std::ceil(key.uv[2]*width)),left+1,width);
    const int bottom=std::clamp(int(std::ceil(key.uv[3]*height)),top+1,height);
    const int chartWidth=right-left,chartHeight=bottom-top;
    float minX=INFINITY,maxX=-INFINITY,minZ=INFINITY,maxZ=-INFINITY;
    for(const auto& part:actor.parts)for(const auto& v:part.positionsGame){
        minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);
        minZ=std::min(minZ,v.z);maxZ=std::max(maxZ,v.z);
    }
    if(!(maxX>minX&&maxZ>minZ))return false;
    const float scale=.86f*std::min(chartWidth/(maxX-minX),chartHeight/(maxZ-minZ));
    const float centerX=(minX+maxX)*.5f,centerZ=(minZ+maxZ)*.5f;
    image.rgba.assign(size_t(width)*height*4,0);
    for(size_t i=3;i<image.rgba.size();i+=4)image.rgba[i]=255;
    std::vector<float> depth(size_t(chartWidth)*chartHeight,-INFINITY);
    bool headRendered=false;

    for(bool translucent:{false,true})for(const auto& part:actor.parts){
        if(cancelled.load(std::memory_order_acquire))return false;
        const auto& mesh=part.mesh;
        const size_t count=part.positionsGame.size();
        if(mesh.alphaBlend!=translucent||mesh.texcoords.size()!=count*2||!count)continue;
        const auto generated=actor.generatedTextures.find(mesh.diffuseTexturePath);
        Fo3RgbaTexture decoded;
        const Fo3RgbaTexture* texture=nullptr;
        if(generated!=actor.generatedTextures.end())texture=&generated->second;
        else if(LoadFalloutTextureRgba(mesh.diffuseTexturePath,decoded))texture=&decoded;
        if(!texture||texture->width<=0||texture->height<=0||texture->rgba.size()!=size_t(texture->width)*texture->height*4)continue;
        auto screen=[&](uint32_t i){
            const auto& p=part.positionsGame[i];
            return std::array<float,3>{(left+right)*.5f-(p.x-centerX)*scale,
                (top+bottom)*.5f-(p.z-centerZ)*scale,p.y};
        };
        auto edge=[](const auto& a,const auto& b,float x,float y){
            return (b[0]-a[0])*(y-a[1])-(b[1]-a[1])*(x-a[0]);
        };
        const bool vertexColor=mesh.vertexColors.size()==count*4;
        for(size_t i=0;i+2<mesh.indices.size();i+=3){
            const std::array<uint32_t,3> ids{mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]};
            if(*std::max_element(ids.begin(),ids.end())>=count)continue;
            const auto a=screen(ids[0]),b=screen(ids[1]),c=screen(ids[2]);
            const float area=edge(a,b,c[0],c[1]);
            if(!std::isfinite(area)||std::abs(area)<.001f)continue;
            const int x0=std::max(left,int(std::floor(std::min({a[0],b[0],c[0]}))));
            const int y0=std::max(top,int(std::floor(std::min({a[1],b[1],c[1]}))));
            const int x1=std::min(right-1,int(std::ceil(std::max({a[0],b[0],c[0]}))));
            const int y1=std::min(bottom-1,int(std::ceil(std::max({a[1],b[1],c[1]}))));
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                const std::array<float,3> w{edge(b,c,x+.5f,y+.5f)/area,
                    edge(c,a,x+.5f,y+.5f)/area,edge(a,b,x+.5f,y+.5f)/area};
                if(std::min({w[0],w[1],w[2]})<0)continue;
                const float z=a[2]*w[0]+b[2]*w[1]+c[2]*w[2];
                auto& pixelDepth=depth[size_t(y-top)*chartWidth+x-left];
                if(z<=pixelDepth)continue;
                float u=0,v=0,light=0;
                std::array<float,3> tint{};
                for(unsigned k=0;k<3;++k){
                    const size_t vertex=ids[k];
                    u+=mesh.texcoords[vertex*2]*w[k];v+=mesh.texcoords[vertex*2+1]*w[k];
                    const auto& n=part.normalsGame[vertex];
                    light+=(n.y*.75f+n.z*.66f)*w[k];
                    for(unsigned channel=0;channel<3;++channel)
                        tint[channel]+=w[k]*(vertexColor?mesh.vertexColors[vertex*4+channel]:1.f);
                }
                const int sx=std::clamp(int(u*texture->width),0,texture->width-1);
                const int sy=std::clamp(int(v*texture->height),0,texture->height-1);
                const size_t src=(size_t(sy)*texture->width+sx)*4;
                float alpha=texture->rgba[src+3]/255.f*mesh.alpha;
                if(mesh.alphaTest&&alpha<mesh.alphaThreshold)continue;
                if(!translucent)alpha=1;
                if(alpha<.015f)continue;
                const size_t dst=(size_t(y)*width+x)*4;
                const float lit=.60f+.40f*std::max(0.f,light);
                for(unsigned channel=0;channel<3;++channel){
                    const float sample=std::clamp(texture->rgba[src+channel]*lit*tint[channel],0.f,255.f);
                    image.rgba[dst+channel]=uint8_t(sample*alpha+image.rgba[dst+channel]*(1-alpha));
                }
                if(alpha>.5f)pixelDepth=z;
                headRendered|=fo3appearance::SameModel(part.placement.modelPath,source.raceHeadModel);
            }
        }
    }
    return headRendered;
}

static void ReleaseTexture(){
    if(gFo3AppearanceTexture)glDeleteTextures(1,&gFo3AppearanceTexture);
    gFo3AppearanceTexture=0;gFo3AppearanceScreenRef=0;
    textureUv={0,0,1,1};
}

static bool UpdatePreview(const PreviewKey& key,const fo3character::Catalog& catalog){
    if(preparation.Ready()){
        if(pendingGeneration==generation&&pending==key){
            if(preparation.Successful()){
                const auto& image=preparation.Get();
                const GLuint texture=fo3hudassets::UploadTexture(image.width,image.height,image.rgba.data());
                if(texture){
                    ReleaseTexture();
                    gFo3AppearanceTexture=texture;gFo3AppearanceScreenRef=key.screen;
                    textureUv=key.uv;
                }
            }
            completed=key;hasCompleted=true;
        }
        preparation.Reset();
    }
    if(hasCompleted&&completed==key)return true;
    const auto race=catalog.races.find(key.appearance.race);
    if(race==catalog.races.end())return true;
    if(!preparation.Active()){
        pending=key;pendingGeneration=generation;
        auto axes=controls;
        if(!preparation.Start([key,limits=race->second,axes](Fo3RgbaTexture& image,const std::atomic<bool>& cancelled){
            return RenderHead(key,limits,*axes,image,cancelled);
        })){
            completed=key;hasCompleted=true;return true;
        }
    }
    return false;
}
}

bool Fo3AppearanceFocus(){
    return gPlayerSession&&gPlayerSession->player.Snapshot().appearanceMenuOwner;
}
void ShutdownFo3AppearancePreview(){
    fo3chargen::preparation.Reset();
    fo3chargen::ReleaseTexture();
    fo3chargen::playerOwner=nullptr;fo3chargen::controls.reset();
    fo3chargen::hasCompleted=false;fo3chargen::menu={};fo3chargen::panel={};
}
void UpdateFo3AppearanceMenu(double now,float x,float y,bool a,bool b,bool focused,const float* head){
    using namespace fo3chargen;
    if(!gPlayerSession||gPlayerSession->saveBlocked||IsFo3LoadingVisible())return;
    auto& player=gPlayerSession->player;
    const auto owner=player.Snapshot().appearanceMenuOwner;
    if(playerOwner!=&player||generation!=player.ScriptRuntimeGeneration()||cell!=gCurrentCellFormId){
        preparation.RequestCancel();ReleaseTexture();
        playerOwner=&player;generation=player.ScriptRuntimeGeneration();cell=gCurrentCellFormId;
        hasCompleted=false;menu={};panel={};
    }
    if(!owner){
        menu={};panel.visible=false;
        if(preparation.Active())preparation.TryReset();
        if(!gFo3AppearanceScreenRef){ReleaseTexture();hasCompleted=false;}
        return;
    }
    if(!controls){
        std::vector<uint8_t> bytes;
        auto axes=std::make_shared<std::vector<fo3character::Control>>();
        const auto archive=fo3assets::GetBsaArchive(fo3assets::FalloutDataPath("Fallout - Misc.bsa"));
        if(archive&&archive->Read("facegen/si.ctl",bytes,nullptr,fo3assets::BsaPathKind::Exact,1024*1024))
            fo3character::DecodeControls(bytes,*axes);
        controls=std::move(axes);
    }
    PreviewKey key;
    key.appearance=player.AppearanceDraft();key.female=player.PlayerFemale();
    if(const auto* screen=Screen()){key.screen=screen->refFormId;key.uv=screen->geneUv;}
    if(menu.owner!=owner){
        const float yaw=head[3];
        panel={};panel.yaw=yaw;
        panel.anchor={head[0]-std::sin(yaw)*1.10f,
                      head[1]-.04f,
                      head[2]-std::cos(yaw)*1.10f};
    }
    const auto label=[&](const char* id){return fo3menuui::Setting(player,id);};
    const bool ready=UpdatePreview(key,player.Definitions().pipboy.appearance);
    const auto result=menu.Step(player,*controls,label,now,x,y,a,b,focused,ready);
    if(result==Menu::Result::Confirmed){
        panel.visible=false;fo3audio::NamedSound("UIMenuOK");FlushFo3PlayerState();return;
    }
    if(result==Menu::Result::Changed)fo3audio::Scroll();
    panel.visible=true;
    if(menu.confirmation!=Menu::Confirm::None){
        panel.title.clear();
        panel.subtitle=menu.confirmation==Menu::Confirm::Done?
            label("sRSMConfirmDone")+" "+label("sRSMCharacter")+"?":label("sRSMConfirmRandomize");
        panel.choices={label("sYes"),label("sNo")};
        panel.selected=menu.cursor.selected;panel.revision=player.Revision();
    }
}
