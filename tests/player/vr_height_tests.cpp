#include "player/fo3-vr-height.h"
#include "player/fo3-player-scale.h"
#include "player/fo3-sneak.h"
#include <cassert>
#include <fstream>
#include <limits>
#include <unistd.h>
int main(){
    using namespace fo3vrheight;
    SupportAnchor feet;feet.Follow(0,0,0);
    feet.Follow(.3f,0,.4f);assert(feet.x==0&&feet.z==0);
    feet.Follow(1,0,.4f);assert(feet.x>.6f&&feet.x<.7f);
    feet.ready=false;feet.Follow(-2,3,0);assert(feet.x==-2&&feet.z==3);
    Settings settings;Tracking tracking;
    tracking.Observe(0,true,true,-1.62f,settings);assert(tracking.Calibrate(settings));
    assert(std::abs(settings.standing-1.62f)<1e-5f);
    auto point=fo3scale::Local({0,tracking.floor+tracking.Shift(-1.55f,settings),0},.4f,-1.55f);
    assert(std::abs(point[1]+1.55f)<1e-5f); // Real floor stays at the authored floor at child scale.
    assert(std::abs(tracking.PhysicalHeight(-1.4f)-.22f)<1e-5f);
    settings.adjustment=.12f;
    assert(std::abs(tracking.Shift(-1.55f,settings)-.19f)<1e-5f);
    tracking.Recenter(.3f);tracking.Observe(-.3f,true,true,-1.92f,settings);
    assert(std::abs(tracking.PhysicalHeight(tracking.head)-1.62f)<1e-5f); // Recenter is not a crouch.
    tracking.Observe(-.3f,false,true,-1.92f,settings);assert(!tracking.Calibrate(settings));
    Settings firstUse;Tracking startup;
    startup.Observe(.5f,true,true,-1.6f,firstUse);assert(!startup.AutoCalibrate(firstUse,.02f));
    for(int i=0;i<30;++i){startup.Observe(0,true,true,-1.6f,firstUse);startup.AutoCalibrate(firstUse,.02f);}
    assert(firstUse.standing==1.6f); // A transient first pose cannot set standing height.
    assert(std::abs(CapsuleHeight(.3f,.26f,1)-.522f)<1e-5f);
    assert(std::abs(CapsuleHeight(2.1f,.26f,1)-2.1f)<1e-5f);
    fo3sneak::Stance stance;stance.Calibrate(1.62f);
    stance.Step(2.1f,true,true,false,.02f);stance.Step(1.62f,true,true,false,.02f);
    assert(stance.standing==1.62f&&!stance.sneaking); // Stretching cannot corrupt standing height.
    stance.Step(1.1f,true,true,false,.02f);stance.Step(1.62f,true,true,false,.02f);
    assert(!stance.sneaking); // Brief jitter does not enter sneak.
    for(int i=0;i<8;++i)stance.Step(.2f,true,true,false,.02f);
    assert(stance.physical&&stance.sneaking&&stance.offset==0); // Full physical lowering is retained.
    for(int i=0;i<8;++i)stance.Step(1.62f,true,true,false,.02f);
    assert(!stance.sneaking);
    stance.Step(1.62f,true,false,true,.02f);stance.Step(1.62f,true,true,true,.02f);
    assert(!stance.manual); // A stick held through a menu never toggles on close.
    stance.Step(1.62f,true,true,false,.02f);stance.Step(1.62f,true,true,true,.02f);
    assert(stance.manual);
    for(int i=0;i<30;++i)stance.Step(1.62f,true,true,true,.02f);
    stance.Step(.2f,true,false,false,.02f);assert(stance.offset==0); // Even modal UI cannot double-lower.
    stance.Calibrate(1.62f);
    for(int i=0;i<8;++i)stance.Step(.2f,true,true,false,.02f,false);
    assert(!stance.sneaking); // Physical crouch can be disabled independently of stick crouch.
    stance.Calibrate(1.6f);
    for(int i=0;i<40;++i)stance.Step(1.6f,true,true,true,.02f,true,.28f,.3f);
    assert(std::abs(1.6f+.3f+stance.offset-1.9f*.65f)<1e-5f);
    const std::string path="/tmp/fq-vr-height-"+std::to_string(getpid())+".cfg";
    assert(settings.Save(path));Settings restored;assert(restored.Load(path));
    assert(restored.adjustment==settings.adjustment&&restored.standing==settings.standing);
    {std::ofstream f(path);f<<"FQVR 1 nan 1.6 .28 1\n";}
    assert(!restored.Load(path)&&restored.adjustment==settings.adjustment);
    unlink(path.c_str());assert(!settings.Save("/not-a-directory/vr-settings.cfg"));
}
