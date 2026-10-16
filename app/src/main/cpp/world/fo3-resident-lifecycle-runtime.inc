bool QuiesceFo3ResidentForSceneChange() noexcept {
    if(gFo3ResidentExchangeExecuting||gFo3ResidentExchangeRecoveryFailed||gFo3ResidentTeardownActive)return false;
    Fo3ResidentWorkflowAdapter adapter;
    if(!gFo3ResidentWorkflow.Interrupt(adapter))return false;
    if(!gFo3ResidentLane.Blocked())return true;
    // Also handle a lane whose logical factory never became an owner.
    try{return CancelFo3ResidentPreparation(gFo3ResidentLane.CurrentTicket());}
    catch(...){return false;}
}
bool ShutdownFo3ResidentWorkflow() noexcept {
    using Phase=fo3scene::ResidentWorkflowPhase;
    if(gFo3ResidentTeardownActive||gFo3ResidentExchangeExecuting||
        gFo3ResidentWorkflow.State()==Phase::Executing||gFo3ResidentWorkflow.State()==Phase::Finalizing)return false;
    Fo3ResidentWorkflowAdapter adapter;
    (void)gFo3ResidentWorkflow.Interrupt(adapter);
    // Never save a settled exchange that could not prove/finish finalization.
    // Preparations have not changed canonical state and may be joined below.
    if(gFo3ResidentWorkflow.State()==Phase::Settled||
        gFo3ResidentWorkflow.State()==Phase::RecoveryFailed||gFo3ResidentExchangeRecoveryFailed){
        gFo3ResidentExchangeRecoveryFailed=true;gSceneReady=false;
        if(gPlayerSession)gPlayerSession->saveBlocked=true;
    }
    try{ResetFo3ResidentPreparationLane();return true;}
    catch(...){
        gFo3ResidentTeardownActive=true;gFo3ResidentExchangeRecoveryFailed=true;gSceneReady=false;
        if(gPlayerSession)gPlayerSession->saveBlocked=true;
        return false; // Caller must keep this GL context/caches alive.
    }
}
