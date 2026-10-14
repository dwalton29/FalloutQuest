#include "fo3-resident-texture-upload.h"
#include "fo3-resident-gpu-stage.h"
#include "fo3-visual-exchange.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace fo3scene;
static Fo3RgbaTexture image(){Fo3RgbaTexture a;a.width=a.height=2;a.rgba.resize(16,127);return a;}
static Fo3RgbaCubeTexture cube(){
    Fo3RgbaCubeTexture a;a.width=a.height=2;a.mipLevels=2;
    for(auto& f:a.rgbaLevels){f.resize(2);f[0].resize(16);f[1].resize(4);}return a;
}
static void destroy(uint32_t id)noexcept{glDeleteTextures(1,&id);}
struct Object{uint32_t geometry=0;std::shared_ptr<const ResidentTextureOwnership> textures;};
static int disposed=0;
static void dispose(std::vector<Object>& objects)noexcept{
    for(auto& o:objects)if(o.geometry)++disposed;
    objects.clear();
}
static void decodeTests(){
    std::atomic<bool> cancelled{false};std::string error;ResidentTextures out;
    int images=0,cubes=0;
    auto decodeImage=[&](const auto&,auto& a){++images;a=image();return true;};
    auto decodeCube=[&](const auto&,auto& a){++cubes;a=cube();return true;};
    std::vector<ResidentTextureRequest> requests={{"Data/Textures/A.dds"},{"\\textures\\a.dds"},{"a.dds",true},{""}};
    assert(PrepareResidentTextures(requests,out,decodeImage,decodeCube,cancelled,136,error));
    assert(images==1&&cubes==1&&out.bytes==136&&out.images.size()==1&&out.cubes.size()==1);
    auto unchanged=[&]{assert(out.bytes==136&&out.images.size()==1&&out.cubes.size()==1);};
    assert(!PrepareResidentTextures(requests,out,decodeImage,decodeCube,cancelled,135,error));unchanged();
    assert(!PrepareResidentTextures(requests,out,[](const auto&,auto&){return false;},decodeCube,cancelled,1024,error));unchanged();
    assert(!PrepareResidentTextures(requests,out,[](const auto&,auto& a){a=image();a.rgba.pop_back();return true;},decodeCube,cancelled,1024,error));unchanged();
    for(int bad=0;bad<5;++bad){
        auto malformed=[&](const auto&,auto& a){a=cube();switch(bad){
            case 0:a.rgbaLevels[5].clear();break;case 1:a.rgbaLevels[4][1].pop_back();break;
            case 2:a.mipLevels=3;break;case 3:a.height=1;break;case 4:a.width=0;break;}return true;};
        assert(!PrepareResidentTextures(requests,out,decodeImage,malformed,cancelled,1024,error));unchanged();
    }
    cancelled=true;
    assert(!PrepareResidentTextures(requests,out,decodeImage,decodeCube,cancelled,1024,error));unchanged();
    cancelled=false;
    auto cancelImage=[&](const auto&,auto& a){a=image();cancelled=true;return true;};
    assert(!PrepareResidentTextures({{"a"}},out,cancelImage,decodeCube,cancelled,1024,error));unchanged();
    cancelled=false;
    bool threw=false;
    try {PrepareResidentTextures(requests,out,[](const std::string&,Fo3RgbaTexture&)->bool{throw std::bad_alloc();},decodeCube,cancelled,1024,error);}
    catch(const std::bad_alloc&){threw=true;}assert(threw);unchanged();
    size_t bytes=9;assert(!ResidentRgbaBytes(-1,1,bytes)&&bytes==9);
    assert(!ResidentRgbaBytes(0,1,bytes));
    assert(PrepareResidentTextures({},out,decodeImage,decodeCube,cancelled,0,error)&&out.bytes==0);
}
static void uploadTests(){
    mock::reset();int imageOps=0,cubeOps=0;GLuint id=0;
    {
        ResidentTextureOwnership owner(destroy);owner.Reserve(2);
        assert(UploadResidentImage(image(),owner,id,nullptr));const auto first=id;
        assert(mock::uploads.size()==1&&mock::mipmaps==1);imageOps=mock::operation;
        assert(mock::parameters.at({GL_TEXTURE_2D,GL_TEXTURE_WRAP_S})==GL_REPEAT);
        assert(mock::parameters.at({GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER})==GL_LINEAR_MIPMAP_LINEAR);
        assert(!owner.Adopt(first)&&!owner.Adopt(0));
        mock::reset();assert(UploadResidentCube(cube(),owner,id));cubeOps=mock::operation;
        assert(mock::uploads.size()==12&&mock::mipmaps==0);
        for(int face=0;face<6;++face)for(int mip=0;mip<2;++mip)
            assert(mock::uploads[face*2+mip]==std::make_tuple(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,mip,2>>mip,2>>mip));
        assert(mock::parameters.at({GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAX_LEVEL})==1);
        assert(mock::parameters.at({GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R})==GL_CLAMP_TO_EDGE);
        assert(owner.Size()==2&&mock::deleted[first]==0);
        const auto full=id;assert(!owner.Adopt(full+100));
    }
    for(bool isCube:{false,true})for(int failure=0;failure<(isCube?cubeOps:imageOps);++failure){
        mock::reset();mock::failAt=failure;
        const auto before=mock::next;
        {ResidentTextureOwnership owner(destroy);owner.Reserve(1);
         const bool ok=isCube?UploadResidentCube(cube(),owner,id):UploadResidentImage(image(),owner,id,nullptr);
         assert(!ok&&id==0&&owner.Size()==0);}
        assert(mock::next==before+1&&mock::deleted[mock::next]==1);
    }
    mock::reset();mock::zero=true;
    {ResidentTextureOwnership owner(destroy);owner.Reserve(1);assert(!UploadResidentImage(image(),owner,id,nullptr)&&id==0);}
    mock::reset();
    {ResidentTextureOwnership full(destroy);assert(!UploadResidentImage(image(),full,id,nullptr)&&id==0);assert(mock::deleted[mock::next]==1);}
    for(const auto& entry:mock::deleted)assert(entry.first!=0&&entry.second==1);
}
static void stageTests(){
    mock::reset();disposed=0;GLuint owned=0;
    {
        ResidentGpuStage<Object> stage(2,dispose);
        assert(!stage.ReadyObjects());
        auto lease=std::make_shared<ResidentTextureOwnership>(destroy);lease->Reserve(1);
        assert(UploadResidentImage(image(),*lease,owned,nullptr));
        assert(stage.Advance([&](size_t i,Object& o){assert(i==0);o.geometry=1;o.textures=lease;return true;})==GpuStageResult::Pending);
        lease.reset();assert(!mock::deleted[owned]);
        assert(stage.Advance([](size_t i,Object& o){assert(i==1);o.geometry=2;throw std::runtime_error("partial upload");return true;})==GpuStageResult::Failed);
        assert(disposed==2&&mock::deleted[owned]==1&&!stage.ReadyObjects());
        stage.Fail();assert(disposed==2);
    }
    {ResidentGpuStage<Object> empty(0,dispose);assert(empty.ReadyObjects()&&empty.ReadyObjects()->empty());
     assert(empty.Advance([](size_t,Object&){assert(false);return false;})==GpuStageResult::Ready);}
    {ResidentGpuStage<Object> stage(1,dispose);assert(stage.Advance([](size_t,Object& o){o.geometry=4;return false;})==GpuStageResult::Failed);}
    // Transfer into the production reversible exchange. Incoming textures stay
    // alive through commit, then rollback/cancel releases them exactly once.
    for(bool rollback:{false,true}) {
        const int before=disposed;
        std::vector<Object> live(1);live[0].geometry=10;
        VisualPublication publication;
        std::unique_ptr<VisualExchange<Object>> plan;
        GLuint texture=0;
        {
            ResidentGpuStage<Object> stage(2,dispose);
            auto lease=std::make_shared<ResidentTextureOwnership>(destroy);lease->Reserve(1);
            assert(UploadResidentImage(image(),*lease,texture,nullptr));
            for(size_t i=0;i<2;++i)stage.Advance([&](size_t slot,Object& o){o.geometry=20+slot;o.textures=lease;return true;});
            plan=VisualExchange<Object>::Prepare(live,publication,{0},*stage.ReadyObjects(),dispose);
            assert(plan&&stage.ReadyObjects()->empty());
        }
        assert(!mock::deleted[texture]);
        assert(plan->Commit(live,publication));assert(live.size()==3);
        if(rollback){assert(plan->Rollback(live,publication));plan.reset();assert(live.size()==1&&mock::deleted[texture]==1);}
        else {plan.reset();assert(!mock::deleted[texture]);}
        dispose(live);assert(disposed==before+3&&mock::deleted[texture]==1);
    }
    {
        // Abandon a completed prepared exchange before commit.
        std::vector<Object> live;VisualPublication publication;
        ResidentGpuStage<Object> stage(1,dispose);
        auto lease=std::make_shared<ResidentTextureOwnership>(destroy);lease->Reserve(1);
        GLuint texture=0;assert(UploadResidentImage(image(),*lease,texture,nullptr));
        stage.Advance([&](size_t,Object& o){o.geometry=30;o.textures=lease;return true;});
        lease.reset();
        auto plan=VisualExchange<Object>::Prepare(live,publication,{},*stage.ReadyObjects(),dispose);
        assert(plan&&!mock::deleted[texture]);plan.reset();assert(mock::deleted[texture]==1&&live.empty());
    }
    {ResidentGpuStage<Object> cancelled(1,dispose);cancelled.Fail();assert(!cancelled.ReadyObjects());}
    for(const auto& entry:mock::deleted)assert(entry.second==1);
}
int main(){decodeTests();uploadTests();stageTests();std::cout<<"resident CPU textures, GL failures, staging and rollback passed\n";}
