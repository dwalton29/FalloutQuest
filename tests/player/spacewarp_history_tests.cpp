#include "rendering/spacewarp/fo3-frame-history.h"
#include <cassert>
#include "rendering/spacewarp/fo3-motion-vectors.h"
#include "rendering/spacewarp/fo3-motion-capture.h"
#include "rendering/spacewarp/fo3-mode.h"
#include <iostream>
using namespace fqspacewarp;
Frame Input(uint64_t serial){Frame f;f.serial=serial;f.displayTime=int64_t(serial)*13888889;f.scene=10;f.world=0x3c;f.sceneReady=f.focused=true;return f;}
void Submit(History& h,const Frame& f){assert(h.Begin(f));h.EyeRendered(0);h.EyeRendered(1);assert(h.Finish(true));}
int main(){
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
