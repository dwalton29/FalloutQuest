#include "rendering/spacewarp/fo3-frame-history.h"
#include <cassert>
#include "rendering/spacewarp/fo3-motion-vectors.h"
#include "rendering/spacewarp/fo3-motion-capture.h"
#include "rendering/spacewarp/fo3-mode.h"
#include <iostream>
#include <limits>
#include <string_view>
using namespace fqspacewarp;
Frame Input(uint64_t serial){Frame f;f.serial=serial;f.displayTime=int64_t(serial)*13888889;f.scene=10;f.world=0x3c;f.sceneReady=f.focused=true;return f;}
void Submit(History& h,const Frame& f){assert(h.Begin(f));h.EyeRendered(0);h.EyeRendered(1);assert(h.Finish(true));}
void CheckRejectionDiagnostics() {
    History history;
    Drawable base;
    base.geometry=99;
    base.palette.resize(1);
    base.deformedPositions={1,2,3};
    const float nan=std::numeric_limits<float>::quiet_NaN();
    auto check=[&](const char* reason,uint64_t id,auto modify) {
        assert(history.Begin(Input(1))&&!history.FirstRejection().reason);
        assert(history.Record(1,base)&&history.Record(1,base));
        auto candidate=base;
        modify(candidate);
        fqmotion::Capture capture;
        capture.Begin(history);
        capture.Main();
        fqmotion::Draw draw;
        draw.identity=id;
        draw.mesh=7;
        assert(!capture.Record(draw,candidate,0x1234));
        assert(capture.reasons&fqmotion::Capacity);
        assert(!history.Record(0,base)); // A later error must preserve the first.
        const auto& r=history.FirstRejection();
        assert(r.reason&&std::string_view(r.reason)==reason);
        assert(r.identity==id&&r.reference==0x1234&&r.mesh==7);
        assert(r.objects==1&&r.usedBytes==base.Bytes()&&r.recordBytes==candidate.Bytes());
    };
    check("missing-identity",0,[](Drawable&){});
    check("missing-geometry",2,[](Drawable& d){d.geometry=0;});
    check("nonfinite-model",2,[&](Drawable& d){d.model[0]=nan;});
    check("nonfinite-palette",2,[&](Drawable& d){d.palette[0][0]=nan;});
    check("nonfinite-position",2,[&](Drawable& d){d.deformedPositions[0]=nan;});
    check("conflicting-geometry",1,[](Drawable& d){++d.geometry;});
    check("conflicting-model",1,[](Drawable& d){++d.model[0];});
    check("conflicting-palette",1,[](Drawable& d){++d.palette[0][0];});
    check("conflicting-positions",1,[](Drawable& d){++d.deformedPositions[0];});
    check("byte-limit",2,[](Drawable& d){d.deformedPositions.resize(History::MaxHistoryBytes/sizeof(float));});

    assert(history.Begin(Input(2)));
    for(size_t id=1;id<=History::MaxDrawables;++id)assert(history.Record(id,base));
    assert(!history.Record(History::MaxDrawables+1,base));
    const auto& r=history.FirstRejection();
    assert(std::string_view(r.reason)=="object-limit"&&r.objects==History::MaxDrawables);
    assert(r.usedBytes==History::MaxDrawables*base.Bytes());

    assert(history.Begin(Input(3)));
    fqmotion::Capture capture;
    capture.Begin(history);
    capture.Main();
    capture.draws.resize(History::MaxDrawables);
    fqmotion::Draw draw;
    draw.identity=2;
    assert(!capture.Record(draw,base));
    assert(std::string_view(history.FirstRejection().reason)=="draw-list-limit");
    history.Reset();
    assert(!history.Record(0,base)&&!history.FirstRejection().reason);
}
void CheckSharedPlacements() {
    History history;
    constexpr uint64_t owner=0x100000007ULL;
    const std::array<MotionIdentity,5> keys{{
        {707,{PlacementKind::Weapon,owner}}, {707,{PlacementKind::Weapon,7}},
        {707,{PlacementKind::SpareMagazine,owner}}, {707,{PlacementKind::EjectedMagazine,owner}},
        {708,{PlacementKind::Weapon,owner}}
    }};
    for(unsigned frame=1;frame<=3;++frame){
        assert(history.Begin(Input(frame)));
        std::array<Coverage,2> coverage{};
        for(unsigned eye=0;eye<2;++eye){
            fqmotion::Capture capture;
            capture.Begin(history);
            capture.Main();
            for(size_t n=0;n<keys.size();++n){
                const bool reverse=(frame+eye)%2==0;
                const size_t i=reverse?keys.size()-1-n:n;
                fqmotion::Draw draw;
                draw.identity=keys[i];
                draw.mesh=(i==4)?7208:7207;
                Drawable shape;
                shape.geometry=keys[i].shape;
                shape.model[12]=float(i*10+frame);
                assert(capture.Record(draw,shape)&&capture.Record(draw,shape));
                if(frame>1){
                    const auto* prior=history.Previous(draw.identity,shape.geometry);
                    assert(prior&&prior->model[12]==shape.model[12]-1);
                }
            }
            assert(capture.draws.size()==keys.size());
            coverage[eye]=capture.Coverage(true);
            history.EyeRendered(eye);
        }
        assert(!history.FirstRejection().reason);
        assert(history.CanSynthesize(coverage)==(frame==3));
        assert(history.Finish(true));
    }
    // Two different transforms for the *same* placement are still an error.
    assert(history.Begin(Input(4)));
    Drawable conflicting;
    conflicting.geometry=keys[0].shape;
    assert(history.Record(keys[0],conflicting));
    ++conflicting.model[12];
    assert(!history.Record(keys[0],conflicting,0,7207,"mesh.nif"));
    assert(std::string_view(history.FirstRejection().reason)=="conflicting-model");
    assert(history.RejectedPlacement().id==owner&&history.RejectedModel()=="mesh.nif");
}
int main(){
 CheckSharedPlacements();
 CheckRejectionDiagnostics();
 Matrix reproject;auto unchanged=fqskin::Identity();unchanged[12]=157.12f;unchanged[13]=-31.88f;assert(ClipReprojection(unchanged,unchanged,reproject)&&reproject==fqskin::Identity());
 Matrix inverse;auto translated=fqskin::Identity();translated[12]=.2f;
 assert(Inverse(translated,inverse));auto world=Transform(inverse,{.2f,0,0,1});assert(std::abs(world[0])<1e-6f);
 std::array<float,3> velocity{};assert(StaticMotion(fqskin::Identity(),fqskin::Identity(),.3f,.4f,.5f,{1,1,1},velocity));assert((velocity==std::array<float,3>{}));
 assert(StaticMotion(inverse,fqskin::Identity(),.5f,.5f,.5f,{1,1,1},velocity));assert(std::abs(velocity[0]-.2f)<1e-6f);
 assert(!StaticMotion(inverse,fqskin::Identity(),.5f,.5f,1,{1,1,1},velocity));Matrix singular{};assert(!Inverse(singular,inverse));assert(!ClipReprojection(singular,fqskin::Identity(),reproject));assert(!ClipReprojection(singular,singular,reproject));assert(ClipReprojection(translated,fqskin::Identity(),reproject)&&std::abs(reproject[12]+.2f)<1e-6f);
 History h;auto frame=Input(1);assert(Eligible(frame));
 for(uint32_t world:{0u,0xa74u,0x3cu}){auto other=frame;other.world=world;assert(Eligible(other));}
 assert(SelectMode("")==Mode::Synthesis&&SelectMode(nullptr)==Mode::Synthesis&&SelectMode("on")==Mode::Synthesis);
 assert(SelectMode("0")==Mode::Off&&SelectMode("validate")==Mode::Validate&&SelectMode("1")==Mode::Validate&&SelectMode("visualize")==Mode::Visualize);
 for(int block=0;block<4;++block){auto f=frame;if(block==0)f.loading=true;if(block==1)f.focused=false;if(block==2)f.modal=true;if(block==3)f.sceneReady=false;assert(!h.Begin(f));}
 Drawable d;d.geometry=99;d.palette.resize(2);d.deformedPositions={1,2,3};
 assert(h.Begin(frame)&&h.Record(1,d)&&h.Record(1,d));h.EyeRendered(0);assert(!h.Finish(true)&&!h.Previous(1,99));
 Submit(h,Input(2));assert(h.SubmittedFrames()==1);Submit(h,Input(3));assert(h.SubmittedFrames()==2);
 assert(h.Begin(Input(4)));h.EyeRendered(0);h.EyeRendered(1);
 std::array<Coverage,2> coverage{};assert(!h.CanSynthesize(coverage));
 for(auto& c:coverage){c.serial=4;c.motion=c.depth=c.staticWorld=c.cutouts=c.hands=c.weapons=true;c.dynamicExpected=5;c.dynamicWritten=5;}
 for(uint64_t id=1;id<=5;++id)assert(h.Record(id,d));
 assert(h.CanSynthesize(coverage));coverage[1].dynamicWritten=4;assert(!h.CanSynthesize(coverage));coverage[1].dynamicWritten=5;
 coverage[0].serial=3;assert(!h.CanSynthesize(coverage));coverage[0].serial=4;
 assert(h.Record(1,d)&&h.Finish(true));assert(h.Previous(1,99)&&!h.Previous(1,100));
 assert(h.Begin(Input(5)));auto changed=d;changed.palette[0][0]=1;assert(h.Record(1,d));assert(!h.Record(1,changed));h.EyeRendered(0);h.EyeRendered(1);assert(!h.Finish(true));
 Submit(h,Input(6));Submit(h,Input(7));auto cut=Input(8);cut.appToWorld[12]=3;assert(h.Begin(cut)&&!h.Ready());h.EyeRendered(0);h.EyeRendered(1);h.Finish(true);
 auto scene=Input(9);scene.scene=11;assert(h.Begin(scene)&&!h.Ready());h.Finish(false);
 Submit(h,Input(10));Submit(h,Input(11));auto rebase=Input(12);rebase.origin[0]=4096;assert(h.Begin(rebase)&&!h.Ready());
 History lost;Submit(lost,Input(1));Submit(lost,Input(2));assert(!lost.Finish(false)&&lost.SubmittedFrames()==0);
 History bounded;assert(bounded.Begin(Input(1)));Drawable large;large.geometry=99;large.deformedPositions.resize(History::MaxHistoryBytes/sizeof(float));assert(!bounded.Record(1,large));
 History captured;fqmotion::Capture capture;fqmotion::Draw draw;draw.identity=5;d={};d.geometry=9;
 for(unsigned serial=1;serial<=2;++serial){assert(captured.Begin(Input(serial)));capture.Begin(captured);assert(!capture.Record(draw,d));capture.Main();assert(capture.Record(draw,d));assert(capture.Record(draw,d)&&capture.draws.size()==1);
   if(serial==1)assert(capture.reasons&fqmotion::MissingHistory);
   else assert(capture.Coverage(true).Complete(serial));
   captured.EyeRendered(0);captured.EyeRendered(1);assert(captured.Finish(true));}
 assert(captured.Begin(Input(3)));capture.Begin(captured);capture.Main();d.deformedPositions={1,2,3};assert(capture.Record(draw,d));assert(capture.reasons&fqmotion::Deformation);assert(!capture.Coverage(true).Complete(3));
 capture.Block(fqmotion::Ui|fqmotion::Water);assert((capture.reasons&fqmotion::Ui)&&(capture.reasons&fqmotion::Water));
 captured.Finish(false);capture.Begin(captured);capture.Main();assert(!capture.history&&!capture.collecting&&!capture.Record(draw,d));
 History transitions;Submit(transitions,Input(1));Submit(transitions,Input(2));auto inside=Input(3);inside.world=0;
 assert(transitions.Begin(inside)&&transitions.Active()&&!transitions.Ready()&&transitions.SubmittedFrames()==0);
 transitions.EyeRendered(0);transitions.EyeRendered(1);assert(transitions.Finish(true));inside.serial=4;inside.displayTime+=10000000;
 assert(transitions.Begin(inside));transitions.EyeRendered(0);transitions.EyeRendered(1);assert(transitions.Finish(true));
 inside.serial=5;inside.displayTime+=10000000;assert(transitions.Begin(inside)&&transitions.Ready());
 std::cout<<"SpaceWarp all-scene defaults, transitions, stereo submission, deformation history, coverage and bounded storage passed\n";
}
