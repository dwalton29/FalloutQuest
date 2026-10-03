#include "../../app/src/main/cpp/ui/loading/fo3-loading-catalog.h"
#include "../../app/src/main/cpp/ui/loading/fo3-loading-menu.h"
#include "../../app/src/main/cpp/ui/loading/fo3-loading-state.h"
#include <fstream>
#include <cassert>
#include <set>
#include <iostream>
using namespace fo3loadingpose;
static bool near(float a,float b){return std::fabs(a-b)<0.0001f;}
static std::vector<uint8_t> field(const char* type,const std::string& value) {
    std::vector<uint8_t> b(type,type+4);size_t n=value.size()+1;b.push_back(n&255);b.push_back(n>>8);
    b.insert(b.end(),value.begin(),value.end());b.push_back(0);return b;
}
int main(int argc,char**argv) {
    fo3loadingmenu::Definition menu;
    fo3loadingmenu::ReadIni("[Other]\niSystemColorMainMenuRed=12\n[Interface]\niSystemColorMainMenuRed=200 ; note\niSystemColorMainMenuGreen=999\niSystemColorMainMenuBlue=12junk\n",menu);
    assert(menu.red==200 && menu.green==255 && menu.blue==165);
    fo3loadingmenu::ReadXml("<menufade>0.9</menufade><nif name=\"loading_nif\"><filename>Interface\\Custom.NIF</filename></nif><nif name=\"loading_pinwheel\"><filename>Interface\\Wheel.NIF</filename><animation>AuthoredIdle</animation></nif>",menu);
    assert(menu.overlayPath=="Interface\\Custom.NIF" && menu.compassPath=="Interface\\Wheel.NIF" && menu.compassAnimation=="AuthoredIdle" && near(menu.fadeSeconds,0.9f));
    fo3loadingmenu::ReadXml("<menufade>nan</menufade><nif name=\"loading_nif\"><filename>bad</filename>",menu);
    assert(near(menu.fadeSeconds,0.9f) && menu.overlayPath=="Interface\\Custom.NIF");
    BeginFo3Loading(1,2);
    for(unsigned i=0;i<FO3_LOADING_MIN_PRESENT_FRAMES+5;++i)MarkFo3LoadingFramePresented(false);
    assert(ShouldDelayFo3TransitionConsume());
    MarkFo3LoadingFramePresented(true);assert(!ShouldDelayFo3TransitionConsume());CancelFo3Loading();
    if(argc==3) {
        auto read=[](const char* path){std::ifstream f(path);assert(f.good());return std::string(std::istreambuf_iterator<char>(f),{});};
        menu=fo3loadingmenu::Definition{};fo3loadingmenu::ReadXml(read(argv[1]),menu);fo3loadingmenu::ReadIni(read(argv[2]),menu);
        assert(menu.overlayPath=="Interface\\Loading\\LoadingAnim01.NIF" && menu.compassPath=="Interface\\Circular Loading\\loading01.nif");
        assert(menu.compassAnimation=="Idle" && menu.red==199 && menu.green==255 && menu.blue==165 && near(menu.fadeSeconds,0.75f));
        std::cout<<"Original XML/INI loading definition verified\n";
    }
    Matrix a=Anchor(1,1.6f,3,0,0,0,1);
    assert(near(a.m[12],1)&&near(a.m[13],1.6f)&&near(a.m[14],0.5f));
    Matrix right=Anchor(1,1.6f,3,0,std::sqrt(0.5f),0,std::sqrt(0.5f));
    assert(near(right.m[12],-1.5f)&&near(right.m[14],3));
    Matrix exhibit=Multiply(a,Placement(ModelX,ModelY,PanelDistance-ModelDistance,0));
    assert(near(3-exhibit.m[14],ModelDistance));
    assert(near(exhibit.m[12],1+ModelX) && near(exhibit.m[13],1.6f+ModelY));
    assert(ModelX<0 && ModelY<0 && ModelDistance<2.1f);
    // Same tracking-space panel produces opposite horizontal offsets for eyes
    // 64mm apart. This guards against the old zero-disparity overlay regression.
    Matrix leftEye=Placement(0.032f,0,0,0),rightEye=Placement(-0.032f,0,0,0);
    Matrix l=Multiply(leftEye,a),r=Multiply(rightEye,a);
    assert(near(l.m[12]-r.m[12],0.064f));
    assert(near(l.m[14],r.m[14]));
    assert(near(RotationRadiansPerSecond*30,6));
    auto payload=field("EDID","Test");auto icon=field("ICON","Interface\\Loading\\test.dds");
    payload.insert(payload.end(),icon.begin(),icon.end());
    fo3loading::ParseLoadingRecord(42,payload);assert(fo3loading::gScreens.size()==1);
    assert(fo3loading::gScreens[0].iconPath=="Interface\\Loading\\test.dds");
    auto model=field("MODL","Weapons\\Pistol.NIF");fo3loading::ParseModelRecord(model);
    fo3loading::ParseModelRecord(model);assert(fo3loading::gModels.size()==1);
    fo3loading::ParseModelRecord(field("MODL","Clutter\\BodyParts\\BodyPart01.NIF"));
    assert(fo3loading::gModels.size()==1);
    model.back()=0;model[4]=255;fo3loading::ParseModelRecord(model);assert(fo3loading::gModels.size()==1);
    fo3loading::gScreens={ {1,"generic","","a",{}},{2,"world","","b",{9}},
                          {3,"cell","","c",{8}},{4,"generic2","","d",{}} };
    assert(fo3loading::Select(8,9)->formId==3);assert(fo3loading::Select(7,9)->formId==2);
    std::set<uint32_t> chosen;
    for(uint64_t i=0;i<100;++i)chosen.insert(fo3loading::Select(7,6,Mix(i))->formId);
    assert(chosen==std::set<uint32_t>({1,4}));
    std::vector<uint8_t> inflated;assert(!fo3loading::InflateRecord({1,2,3},fo3loading::COMPRESSED_RECORD,inflated));
    if(argc==2) {
        fo3loading::gScreens.clear();fo3loading::gModels.clear();fo3loading::gScreensPrepared=false;
        fo3loading::Prepare(argv[1]);assert(fo3loading::gScreens.size()==150);assert(!fo3loading::gModels.empty());
        std::cout<<"Original ESM: "<<fo3loading::gScreens.size()<<" pictures, "<<fo3loading::gModels.size()<<" exhibits\n";
    }
    std::cout<<"Loading presentation tests passed\n";
}
