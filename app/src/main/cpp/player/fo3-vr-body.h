#pragma once
// Pure, allocation-free retarget math. All points are in avatar render space
// (metres, +Y up, -Z forward). Rotations map authored render vectors to pose.
#include <algorithm>
#include <array>
#include <cmath>
namespace fo3vr {
constexpr float Pi=3.14159265359f;
struct V { float x=0,y=0,z=0; };
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V operator*(V a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Length(V a){return std::sqrt(std::max(0.f,Dot(a,a)));}
inline bool Finite(V a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline V Unit(V a,V fallback={1,0,0}){float n=Length(a);return n>1e-6f?a*(1/n):fallback;}
inline V Project(V a,V n){return a-n*Dot(a,n);}
inline float Wrap(float a){return std::remainder(a,2*Pi);}
using R=std::array<float,9>; // row major
inline R Identity(){return {1,0,0,0,1,0,0,0,1};}
inline V Rotate(const R&r,V v){return {r[0]*v.x+r[1]*v.y+r[2]*v.z,r[3]*v.x+r[4]*v.y+r[5]*v.z,r[6]*v.x+r[7]*v.y+r[8]*v.z};}
inline R Transpose(const R&r){return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]};}
inline R Multiply(const R&a,const R&b){R r{};for(int i=0;i<3;i++)for(int j=0;j<3;j++)for(int k=0;k<3;k++)r[i*3+j]+=a[i*3+k]*b[k*3+j];return r;}
inline R Axis(V a,float angle){a=Unit(a);float c=std::cos(angle),s=std::sin(angle),t=1-c;return {c+a.x*a.x*t,a.x*a.y*t-a.z*s,a.x*a.z*t+a.y*s,a.y*a.x*t+a.z*s,c+a.y*a.y*t,a.y*a.z*t-a.x*s,a.z*a.x*t-a.y*s,a.z*a.y*t+a.x*s,c+a.z*a.z*t};}
inline R Frame(V axis,V normal){V x=Unit(axis),z=Unit(Project(normal,x),Unit(Cross(x,{0,0,1}),{0,1,0})),y=Cross(z,x);return {x.x,y.x,z.x,x.y,y.y,z.y,x.z,y.z,z.z};}
inline R FrameRotation(V a,V n,V b,V m){return Multiply(Frame(b,m),Transpose(Frame(a,n)));}
struct Delta {
 R rotation=Identity(); V translation{},pivot{},axis{1,0,0};float scale=1;
 V Point(V p)const{p=p+axis*(Dot(p-pivot,axis)*(scale-1));return Rotate(rotation,p)+translation;}
};
inline Delta Segment(V rest,V restAxis,V current,const R&r,float scale=1){Delta d;d.rotation=r;d.translation=current-Rotate(r,rest);d.pivot=rest;d.axis=Unit(restAxis);d.scale=scale;return d;}
inline Delta RigidMount(const Delta& bone,V bindCentre){Delta d=bone;d.translation=bone.Point(bindCentre)-Rotate(d.rotation,bindCentre);d.scale=1;return d;}
inline Delta BodyRoot(V authoredEyes,V trackedHead,float torsoYaw){Delta d;d.rotation=Axis({0,1,0},torsoYaw);d.translation=trackedHead-Rotate(d.rotation,authoredEyes);return d;}
// Stable physical shoulder-to-wrist measurement, shared by both arms. The
// configurable adult default is a calibration assumption, not Fallout data.
constexpr float DefaultWristReach=.62f;
inline float WristReach(float measured){return std::isfinite(measured)&&measured>=.40f&&measured<=.80f?measured:DefaultWristReach;}
struct ArmRig {V clavicle{},shoulder{},elbow{},wrist{},palm{};bool left=false,valid=false;float wristReach=DefaultWristReach;};
struct ArmState {V direction{},pole{};float roll=0;bool valid=false;};
struct ArmPose {
 Delta clavicle,upper,upperTwist,fore,foreTwist,hand;
 V shoulder{},elbow{},wrist{},palm{};float stretch=1,roll=0,error=0;
 float armScale=1,normalReach=0,targetDistance=0,flex=0,raisedWeight=0;
 V preferredPole{},pole{};bool clamped=false;
 bool valid=false;
};
// Extract the axial quaternion component, rather than projecting alternating
// palm axes. Wrist flex/deviation stays in the residual hand rotation.
inline float AxialRoll(const R&r,V axis,float previous){
 float w,x,y,z;float t=r[0]+r[4]+r[8];
 if(t>0){float s=2*std::sqrt(t+1);w=s*.25f;x=(r[7]-r[5])/s;y=(r[2]-r[6])/s;z=(r[3]-r[1])/s;}
 else if(r[0]>r[4]&&r[0]>r[8]){float s=2*std::sqrt(std::max(0.f,1+r[0]-r[4]-r[8]));w=(r[7]-r[5])/s;x=s*.25f;y=(r[1]+r[3])/s;z=(r[2]+r[6])/s;}
 else if(r[4]>r[8]){float s=2*std::sqrt(std::max(0.f,1+r[4]-r[0]-r[8]));w=(r[2]-r[6])/s;x=(r[1]+r[3])/s;y=s*.25f;z=(r[5]+r[7])/s;}
 else{float s=2*std::sqrt(std::max(0.f,1+r[8]-r[0]-r[4]));w=(r[3]-r[1])/s;x=(r[2]+r[6])/s;y=(r[5]+r[7])/s;z=s*.25f;}
 float a=Dot({x,y,z},axis);if(w*w+a*a<1e-8f)return previous;
 return previous+Wrap(2*std::atan2(a,w)-previous);
}
// One canonical neck delta is used both for skin and final eye alignment.
inline Delta NeckPose(V neck,float relativeYaw){return Segment(neck,{0,1,0},neck,Axis({0,1,0},relativeYaw));}
inline Delta PosedBodyRoot(V eyes,V hmd,float torsoYaw,const Delta&neck){return BodyRoot(neck.Point(eyes),hmd,torsoYaw);}
struct Q {float x=0,y=0,z=0,w=1;};
inline Q CentreOrientation(Q a,Q b){
 float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w,sign=dot<0?-1.f:1.f;
 Q q{a.x+sign*b.x,a.y+sign*b.y,a.z+sign*b.z,a.w+sign*b.w};
 float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
 if(n<1e-6f||!std::isfinite(n))return {};
 return {q.x/n,q.y/n,q.z/n,q.w/n}; // slerp at t=.5 for unit quaternions
}
inline ArmPose SolveArm(const ArmRig&rig,V target,const R&handRotation,ArmState&state,float dt){
 ArmPose p;if(!rig.valid||!Finite(target))return p;
 for(float f:handRotation)if(!std::isfinite(f))return p;
 const V u=rig.elbow-rig.shoulder,f=rig.wrist-rig.elbow;
 const float ul=Length(u),fl=Length(f);if(ul<.05f||fl<.05f)return p;
 // Palm orientation is known before IK: subtract the rotated authored offset
 // to obtain the anatomical wrist target. Never lengthen the forearm to palm.
 V wristTarget=target-Rotate(handRotation,rig.palm-rig.wrist);
 V clav=rig.shoulder-rig.clavicle,reach=wristTarget-rig.shoulder;
 p.normalReach=WristReach(rig.wristReach);p.armScale=p.normalReach/(ul+fl);
 float excess=std::max(0.f,Length(reach)-p.normalReach);
 // VR shoulder contribution: rotate the authored clavicle, preserving its
 // length. Twelve degrees bounds shoulder displacement (~3.3cm on this rig).
 V clavAxis=Cross(clav,reach);float ca=0;
 if(excess>0){
  V fromClav=wristTarget-rig.clavicle;float c=Length(clav),w=Length(fromClav);
  if(c>1e-5f&&w>1e-5f){
   // Law of cosines: use the minimum anatomical clavicle rotation needed
   // to make normal wrist reach, rather than approximate translation/excess.
   float angle=std::acos(std::clamp(Dot(clav,fromClav)/(c*w),-1.f,1.f));
   float required=std::acos(std::clamp((c*c+w*w-p.normalReach*p.normalReach)/(2*c*w),-1.f,1.f));
   ca=std::clamp(angle-required,0.f,12*Pi/180);
  }
 }
 R cr=Length(clavAxis)>1e-5f?Axis(clavAxis,ca):Identity();
 p.clavicle=Segment(rig.clavicle,clav,rig.clavicle,cr);
 p.shoulder=p.clavicle.Point(rig.shoulder);
 reach=wristTarget-p.shoulder;float d=Length(reach);
 V dir=Unit(reach,state.valid?state.direction:Unit(u));
 // Stable symmetric calibration preserves authored upper/forearm ratio.
 // Emergency reach is separate and reversible; tracking never changes scale.
 p.targetDistance=d;p.stretch=std::clamp(d/p.normalReach,1.f,1.03f);
 float scale=p.armScale*p.stretch,a=ul*scale,b=fl*scale;
 p.clamped=d>a+b;
 d=std::clamp(d,std::fabs(a-b)+1e-5f,a+b-1e-5f);
 p.wrist=p.shoulder+dir*d;
 // Generic raised-arm inference, independent of device/UI state. Wrist
 // height is normalized by the calibrated humerus, so users of different
 // reach use the same smooth regions: half a humerus below the shoulder
 // blends gravity; a quarter humerus above it fully lifts/abducts the elbow.
 float side=rig.left?-1.f:1.f;
 float t=std::clamp((wristTarget.y-p.shoulder.y+a*.5f)/(a*.75f),0.f,1.f);
 p.raisedWeight=t*t*(3-2*t);
 V low=Unit({side,-2.f,2.f*std::max(0.f,-dir.y)});
 // Twenty-degree upward tilt favours abduction over shrugging the elbow
 // above the wrist; this is VR anatomy inference, not an authored offset.
 V lifted=Unit({side,std::tan(20*Pi/180),0});
 V prior=Project(low*(1-p.raisedWeight)+lifted*p.raisedWeight,dir);
 V prev=state.valid?Project(state.pole,dir):prior;
 if(Length(prior)<1e-5f){
  // At exactly the prior direction, even the initial projected pole is zero.
  // Choose a torso-forward tangent, never an axis along the reach direction.
  prior=Length(prev)>1e-5f?prev:Project({0,0,1},dir);
 }
 V pole=Unit(prev,Unit(prior));V desired=Unit(prior,pole);
 float angle=std::atan2(Dot(dir,Cross(pole,desired)),std::clamp(Dot(pole,desired),-1.f,1.f));
 if(state.valid){float step=angle*(1-std::exp(-4.f*std::clamp(dt,0.f,.05f)));step=std::clamp(step,-Pi*dt,Pi*dt);pole=Rotate(Axis(dir,step),pole);}else pole=desired;
 p.preferredPole=desired;p.pole=pole;
 float along=(a*a+d*d-b*b)/(2*d),height=std::sqrt(std::max(0.f,a*a-along*along));
 p.elbow=p.shoulder+dir*along+pole*height;
 p.flex=std::acos(std::clamp((a*a+b*b-d*d)/(2*a*b),-1.f,1.f));
 V normal=Unit(Cross(p.elbow-p.shoulder,p.wrist-p.elbow),Unit(Cross(pole,dir)));
 V restNormal=Unit(Cross(u,f),{0,0,1});
 R ur=FrameRotation(u,restNormal,p.elbow-p.shoulder,normal);
 R fr=FrameRotation(f,restNormal,p.wrist-p.elbow,normal);
 V foreAxis=Unit(p.wrist-p.elbow);
 p.roll=AxialRoll(Multiply(handRotation,Transpose(fr)),foreAxis,state.valid?state.roll:0);
 p.upper=Segment(rig.shoulder,u,p.shoulder,ur,scale);
 // Upper twist shares half the humeral swing's axial component relative to
 // clavicle transport. These are absolute bind->pose deltas, not chained rolls.
 R swing=FrameRotation(u,Rotate(cr,restNormal),p.elbow-p.shoulder,Rotate(cr,restNormal));
 float upperRoll=AxialRoll(Multiply(ur,Transpose(swing)),Unit(p.elbow-p.shoulder),0);
 p.upperTwist=Segment(rig.shoulder,u,p.shoulder,Multiply(Axis(Unit(p.elbow-p.shoulder),upperRoll*.5f),swing),scale);
 p.fore=Segment(rig.elbow,f,p.elbow,Multiply(Axis(foreAxis,p.roll*.5f),fr),scale);
 p.foreTwist=Segment(rig.elbow,f,p.elbow,Multiply(Axis(foreAxis,p.roll),fr),scale);
 p.hand=Segment(rig.wrist,f,p.wrist,handRotation);
 p.palm=p.hand.Point(rig.palm);p.error=Length(p.palm-target);
 p.valid=Finite(p.elbow)&&Finite(p.palm);
 if(p.valid){state.direction=dir;state.pole=pole;state.roll=p.roll;state.valid=true;}
 return p;
}
struct TorsoState {float yaw=0,lastLocomotion=0,outsideTime=0;bool valid=false;};
// Three-point tracking cannot uniquely observe torso yaw. A comfortable neck
// deadzone plus sustained-head follow works even with gesturing/missing hands.
// Hand gestures cannot inject yaw. Snap locomotion transports state once.
inline float SolveTorso(TorsoState&s,V /*head*/,float headYaw,float locomotion,
                       const V /*hands*/[2],const bool /*valid*/[2],float dt){
 if(!s.valid){s.yaw=locomotion;s.lastLocomotion=locomotion;s.valid=true;}
 const float snap=Wrap(locomotion-s.lastLocomotion);s.yaw=Wrap(s.yaw+snap);
 s.lastLocomotion=locomotion;dt=std::clamp(dt,0.f,.05f);
 const float relative=Wrap(headYaw-s.yaw),deadzone=35*Pi/180;
 if(std::fabs(relative)>deadzone){
  s.outsideTime+=dt;
  if(s.outsideTime>=.20f){
   float error=relative-std::copysign(deadzone,relative);
   float step=error*(1-std::exp(-dt/.30f));
   s.yaw=Wrap(s.yaw+std::clamp(step,-Pi*dt,Pi*dt));
  }
 }else s.outsideTime=0;
 return s.yaw;
}
} // namespace fo3vr
