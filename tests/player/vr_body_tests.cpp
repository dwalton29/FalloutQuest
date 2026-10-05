#include "player/fo3-vr-body.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace fo3vr;
static bool Near(V a,V b,float eps=2e-4f){return Length(a-b)<eps;}
static V Mirror(V a){return {-a.x,a.y,a.z};}
// Measured bind origins from the supplied original _male/Skeleton.NIF,
// converted with the production 70 units/metre, -1.55m mesh floor transform.
static ArmRig Rig(bool left){
 float sign=left?-1.f:1.f;
 ArmRig r;r.left=left;r.valid=true;
 r.clavicle={sign*3.01861f/70,105.232f/70-1.55f,.91254f/70};
 r.shoulder={sign*13.7437f/70,102.595f/70-1.55f,2.9865f/70};
 r.elbow={sign*29.0237f/70,94.3773f/70-1.55f,3.68005f/70};
 r.wrist={sign*44.3271f/70,85.2929f/70-1.55f,.226509f/70};
 // Fixture palm offset. Asset test separately exercises measured hand mesh.
 r.palm=r.wrist+V{sign*.04f,0,0};return r;
}
static void Check(const ArmRig&r,const ArmPose&p){
 assert(p.valid&&Finite(p.elbow)&&Finite(p.wrist)&&Finite(p.palm));
 assert(std::fabs(Length(p.elbow-p.shoulder)-Length(r.elbow-r.shoulder)*p.stretch)<2e-4f);
 assert(std::fabs(Length(p.wrist-p.elbow)-Length(r.wrist-r.elbow)*p.stretch)<2e-4f);
 assert(p.stretch>=1&&p.stretch<=1.03001f);
 assert(Near(p.upper.Point(r.elbow),p.elbow));
 assert(Near(p.fore.Point(r.wrist),p.wrist));
 assert(Near(p.foreTwist.Point(r.wrist),p.wrist));
 assert(Near(p.hand.Point(r.wrist),p.wrist));
 assert(Near(p.clavicle.Point(r.shoulder),p.shoulder));
 for(const auto*d:{&p.upper,&p.upperTwist,&p.fore,&p.foreTwist,&p.hand}){
  const auto ortho=Multiply(d->rotation,Transpose(d->rotation));
  for(int i=0;i<9;i++)assert(std::fabs(ortho[i]-Identity()[i])<2e-4f);
 }
}
int main(){
 auto r=Rig(false),l=Rig(true);ArmState state;
 // Relaxed, forward, extended, elbow bent, beside head, Pip-Boy viewing,
 // cross chest, behind shoulder, and near the minimum reach singularity.
 V targets[]={{.2f,-.45f,.04f},{.2f,-.1f,-.32f},{.63f,-.1f,0},
              {.3f,-.22f,-.2f},{.12f,.03f,-.1f},{.1f,-.1f,-.2f},
              {-.1f,-.15f,-.15f},{.23f,-.04f,.15f},r.shoulder};
 for(V target:targets){auto p=SolveArm(r,target,Identity(),state,1.f/72);Check(r,p);
  if(Length(target-r.shoulder)<.4f&&Length(target-r.shoulder)>.1f)assert(p.error<2e-4f);
 }
 // Palm compensation is independent of the wrist pivot and hand rotation.
 for(float angle:{-Pi,-Pi/2,0.f,Pi/2,Pi}){
  R hand=Multiply(Axis({0,0,1},angle),Axis({0,1,0},.3f));
  V target{.22f,-.1f,-.25f};auto p=SolveArm(r,target,hand,state,1.f/72);Check(r,p);
  assert(Near(p.palm,target));assert(Near(p.hand.Point(r.palm),target));
  for(int i=0;i<9;i++)assert(std::fabs(p.hand.rotation[i]-hand[i])<1e-6f);
 }
 // Symmetric inputs produce mirrored elbows, shoulders and equal reach.
 for(V t:targets){ArmState a,b;auto rp=SolveArm(r,t,Identity(),a,.014f);
  auto lp=SolveArm(l,Mirror(t),Identity(),b,.014f);Check(r,rp);Check(l,lp);
  assert(Near(Mirror(rp.elbow),lp.elbow));assert(Near(Mirror(rp.shoulder),lp.shoulder));
  assert(std::fabs(rp.stretch-lp.stretch)<1e-6f);
 }
 // Continuous sweeps through down/up and full extension, including 180 degrees
 // from the authored arm. Poles must remain on the same transported hemisphere.
 state={};V last{};bool have=false;
 for(int i=0;i<1440;i++){
  float t=i*2*Pi/1440;V target=r.shoulder+V{.015f,.48f*std::sin(t),-.48f*std::cos(t)};
  auto p=SolveArm(r,target,Identity(),state,1.f/72);Check(r,p);
  if(have)assert(Length(p.elbow-last)<.025f);
  last=p.elbow;have=true;
 }
 // No cumulative scale state, even after an extreme tracked reach.
 for(int i=0;i<100;i++)SolveArm(r,{2,0,0},Identity(),state,.014f);
 auto normal=SolveArm(r,{.22f,-.1f,-.25f},Identity(),state,.014f);
 assert(normal.stretch==1);assert(Near(normal.shoulder,r.shoulder));
 // Axial quaternion decomposition ignores wrist flex; wrap has no roll jump.
 for(int i=-360;i<=360;i++){
  float angle=i*Pi/180;R rotation=Multiply(Axis({1,0,0},angle),Axis({0,1,0},.4f));
  float roll=AxialRoll(rotation,{1,0,0},angle);assert(std::fabs(roll-angle)<1e-4f);
 }
 // Forearm/twist share roll once and keep the same wrist endpoint. For this
 // fixture orient hand by pure pronation relative to the solved bend plane.
 state={};V wristTarget{.3f,-.15f,-.2f};
 auto base=SolveArm(r,wristTarget+Rotate(Identity(),r.palm-r.wrist),Identity(),state,.014f);
 V axis=Unit(base.wrist-base.elbow);R unrolled=Multiply(Axis(axis,-base.roll),base.foreTwist.rotation);
 R hand=Multiply(Axis(axis,1.2f),unrolled);
 state={};auto rolled=SolveArm(r,wristTarget+Rotate(hand,r.palm-r.wrist),hand,state,.014f);Check(r,rolled);
 assert(std::fabs(rolled.roll-1.2f)<1e-3f);
 R relative=Multiply(rolled.foreTwist.rotation,Transpose(rolled.fore.rotation));
 assert(std::fabs(AxialRoll(relative,Unit(rolled.wrist-rolled.elbow),.6f)-.6f)<1e-3f);
 // Torso does not follow independent head yaw. Snap turning transports history
 // exactly once; coherent physical turns reproduce the same root-relative pose.
 TorsoState torso;V head{0,1.6f,0},hands[2]={{-.2f,1.3f,-.3f},{.2f,1.3f,-.3f}};
 bool valid[2]={true,true};SolveTorso(torso,head,0,0,hands,valid,.014f);
 for(int i=1;i<=100;i++)assert(std::fabs(SolveTorso(torso,head,i*.012f,0,hands,valid,.014f))<1e-5f);
 torso=TorsoState{};SolveTorso(torso,head,0,0,hands,valid,.014f);
 R snap=Axis({0,1,0},Pi/4);V turned[2];for(int i=0;i<2;i++)turned[i]=head+Rotate(snap,hands[i]-head);
 assert(std::fabs(SolveTorso(torso,head,Pi/4,Pi/4,turned,valid,.014f)-Pi/4)<1e-5f);
 torso=TorsoState{};SolveTorso(torso,head,0,0,hands,valid,.014f);
 for(int n=1;n<=100;n++){float yaw=n*.01f;R rot=Axis({0,1,0},yaw);
  for(int i=0;i<2;i++)turned[i]=head+Rotate(rot,hands[i]-head);
  assert(std::fabs(SolveTorso(torso,head,yaw,0,turned,valid,.014f)-yaw)<1e-4f);
 }
 // Invalid inputs never overwrite continuity history or produce an active pose.
 assert(!SolveArm(r,{std::numeric_limits<float>::quiet_NaN(),0,0},Identity(),state,.014f).valid);
 // Production body-root mapping aligns all three eye coordinates and applies
 // locomotion yaw once to bones, controller targets, and authored mounts.
 V eyes{0,.162266f,-.0975711f},hmd{2,1.65f,3};
 for(float yaw:{-Pi,-Pi/2,0.f,Pi/4,Pi}){
  Delta root=BodyRoot(eyes,hmd,yaw);assert(Near(root.Point(eyes),hmd));
  V controller=root.Point(normal.palm);
  V local=Rotate(Transpose(root.rotation),controller-root.translation);
  assert(Near(local,normal.palm));
 }
 std::cout<<"VR body pose, lengths, symmetry, continuity, roll and torso tests passed\n";
}
