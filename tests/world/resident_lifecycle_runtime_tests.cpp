#include "world/fo3-resident-workflow.h"
#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using Phase=fo3scene::ResidentWorkflowPhase;
bool gFo3ResidentTeardownActive=false,gFo3ResidentExchangeExecuting=false;
bool gFo3ResidentExchangeRecoveryFailed=false,gSceneReady=true,contextCurrent=true;
struct Session{bool saveBlocked=false;} session;
Session* gPlayerSession=&session;
std::vector<int> order;
std::array<int,6> deletes{};
std::array<bool,6> owned{};
bool joinFails=false;
void step(int n){assert(contextCurrent&&gFo3ResidentTeardownActive);order.push_back(n);}
struct Lane{
    bool blocked=false;uint64_t ticket=0;
    bool Blocked()const{return blocked;}
    uint64_t CurrentTicket()const{return ticket;}
    void Reset(){step(1);if(joinFails)throw std::runtime_error("join failed");blocked=false;ticket=0;}
} gFo3ResidentLane;
struct Fo3ResidentWorkflowAdapter{};
struct Workflow{
    Phase phase=Phase::Idle;
    bool interruptReady=true;int interrupts=0,finalizations=0;
    Phase State()const{return phase;}
    bool Interrupt(Fo3ResidentWorkflowAdapter&)noexcept{
        ++interrupts;
        if(phase==Phase::Idle)return true;
        if(phase==Phase::Executing||phase==Phase::Finalizing||phase==Phase::RecoveryFailed)return false;
        if(!interruptReady)return false;
        if(phase==Phase::Settled)++finalizations;
        phase=Phase::Idle;gFo3ResidentLane.blocked=false;return true;
    }
    void ResetAfterDrain(){step(8);phase=Phase::Idle;}
} gFo3ResidentWorkflow;
int rawCancels=0;bool rawCancelReady=true,rawCancelThrows=false;
bool CancelFo3ResidentPreparation(uint64_t ticket){
    ++rawCancels;assert(ticket==gFo3ResidentLane.ticket&&ticket);
    if(rawCancelThrows)throw std::bad_alloc();
    if(!rawCancelReady)return false;
    gFo3ResidentLane.blocked=false;return true;
}
void dispose(int index,int stage){step(stage);if(owned[index]){owned[index]=false;++deletes[index];}}
void DiscardFo3ResidentGpuStage(){dispose(0,2);}
void DiscardFo3ResidentVisualPlan(){dispose(1,3);}
void DiscardFo3ResidentLightPlan(){dispose(2,4);}
void DiscardFo3ResidentDoorInteractionPlan(){dispose(3,5);}
void DiscardFo3ResidentReferencePublicationPlan(){dispose(4,6);}
struct Admission{void reset(){dispose(5,7);}} gFo3ResidentInteractionAdmission;
const int sentinel=1;
const int* gFo3ResidentPreparedPacket=&sentinel;
uint64_t gFo3ResidentPreparedCollision=99;
#include "world/fo3-resident-reset-runtime.inc"
#include "world/fo3-resident-lifecycle-runtime.inc"
void fixture(Phase phase){
    gFo3ResidentTeardownActive=false;gFo3ResidentExchangeExecuting=false;gFo3ResidentExchangeRecoveryFailed=false;
    gSceneReady=true;contextCurrent=true;session={};gPlayerSession=&session;order.clear();deletes={};owned.fill(true);
    joinFails=false;gFo3ResidentWorkflow={};gFo3ResidentWorkflow.phase=phase;
    gFo3ResidentLane={phase!=Phase::Idle,10};
    rawCancels=0;rawCancelReady=true;rawCancelThrows=false;
    gFo3ResidentPreparedPacket=&sentinel;gFo3ResidentPreparedCollision=99;
}
void cleared(){
    assert(gFo3ResidentWorkflow.State()==Phase::Idle&&!gFo3ResidentLane.Blocked());
    assert(!gFo3ResidentTeardownActive&&!gFo3ResidentPreparedPacket&&!gFo3ResidentPreparedCollision);
    assert(order==std::vector<int>({1,2,3,4,5,6,7,8}));
    for(auto count:deletes)assert(count==1);
}
int main(){
    // Native scene-change path never calls the joining reset; a pending
    // cancellation/finalizer retains every resource and the caller's request.
    for(auto phase:{Phase::Draining,Phase::Cpu,Phase::Gpu,Phase::Ready,Phase::Settled}){
        fixture(phase);gFo3ResidentWorkflow.interruptReady=false;
        for(int i=0;i<3;++i)assert(!QuiesceFo3ResidentForSceneChange());
        assert(order.empty()&&rawCancels==0&&!session.saveBlocked);
        assert(gFo3ResidentWorkflow.State()==phase);
        gFo3ResidentWorkflow.interruptReady=true;
        assert(QuiesceFo3ResidentForSceneChange()&&order.empty()&&rawCancels==0);
        assert(gFo3ResidentWorkflow.finalizations==(phase==Phase::Settled?1:0));
    }
    fixture(Phase::Idle);gFo3ResidentLane.blocked=true;rawCancelReady=false;
    assert(!QuiesceFo3ResidentForSceneChange()&&rawCancels==1&&order.empty());
    rawCancelThrows=true;assert(!QuiesceFo3ResidentForSceneChange()&&order.empty());
    rawCancelThrows=false;rawCancelReady=true;assert(QuiesceFo3ResidentForSceneChange()&&order.empty());
    for(int hazard=0;hazard<3;++hazard){
        fixture(Phase::Ready);
        if(hazard==0)gFo3ResidentExchangeExecuting=true;
        if(hazard==1)gFo3ResidentExchangeRecoveryFailed=true;
        if(hazard==2)gFo3ResidentTeardownActive=true;
        assert(!QuiesceFo3ResidentForSceneChange()&&gFo3ResidentWorkflow.interrupts==0&&order.empty());
    }
    // Explicit shutdown can join unfinished private work, in the old context,
    // before canonical owner destruction and without framebuffer dependence.
    for(auto phase:{Phase::Idle,Phase::Draining,Phase::Cpu,Phase::Gpu,Phase::Ready,Phase::Cancelling}){
        fixture(phase);gFo3ResidentWorkflow.interruptReady=false;
        assert(ShutdownFo3ResidentWorkflow());cleared();
        assert(!session.saveBlocked&&!gFo3ResidentExchangeRecoveryFailed);
        order.clear();assert(ShutdownFo3ResidentWorkflow());
        for(auto count:deletes)assert(count==1); // Idempotent framebuffer fallback.
    }
    fixture(Phase::Settled);assert(ShutdownFo3ResidentWorkflow());cleared();
    assert(gFo3ResidentWorkflow.finalizations==1&&!session.saveBlocked);
    for(auto phase:{Phase::Settled,Phase::RecoveryFailed}){
        fixture(phase);gFo3ResidentWorkflow.interruptReady=false;
        assert(ShutdownFo3ResidentWorkflow());cleared();
        assert(session.saveBlocked&&gFo3ResidentExchangeRecoveryFailed&&!gSceneReady);
        assert(!QuiesceFo3ResidentForSceneChange()); // Cleanup is not recovery.
        assert(ShutdownFo3ResidentWorkflow()&&session.saveBlocked&&gFo3ResidentExchangeRecoveryFailed);
    }
    for(auto phase:{Phase::Executing,Phase::Finalizing}){
        fixture(phase);assert(!ShutdownFo3ResidentWorkflow());
        assert(order.empty()&&gFo3ResidentWorkflow.interrupts==0);
    }
    fixture(Phase::Cpu);gFo3ResidentWorkflow.interruptReady=false;joinFails=true;
    assert(!ShutdownFo3ResidentWorkflow());
    assert(order==std::vector<int>{1}&&gFo3ResidentTeardownActive&&session.saveBlocked&&gFo3ResidentExchangeRecoveryFailed);
    for(auto count:deletes)assert(count==0); // Caller must retain GL context.
    fixture(Phase::RecoveryFailed);gPlayerSession=nullptr;
    assert(ShutdownFo3ResidentWorkflow()&&gFo3ResidentExchangeRecoveryFailed);
    std::cout<<"Native resident lifecycle/reset bridge tests passed\n";
}
