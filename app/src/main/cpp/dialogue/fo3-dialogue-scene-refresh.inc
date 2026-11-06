// Full-scene fallback for script results whose resident resources do not yet
// support an in-place exchange. The loading barrier owns all scene resources;
// the original transaction and its side effects remain indivisible.
bool PublishFo3DialogueSceneRefresh(fo3player::Player& player,
    fo3player::ScriptTransactionPlan& plan,std::string& error){
    const auto* candidate=plan.CandidateState();
    if(!candidate||!plan.HasReferenceChanges()||candidate->playerMove.pending||
       !CanDeferFo3DialogueResult()||gExteriorWorldspaceQ1890||!player.CanCommitScriptTransaction(plan))return false;
    const auto snapshot=plan.CandidateReferences();
    if(!player.CommitScriptTransaction(plan))return false;
    if(!QueueFo3SceneRefreshQ74(gCurrentCellFormId,gSceneCenterXQ1730,gSceneCenterYQ1730,gSceneFloorZQ1730)){
        const bool restored=player.RollbackScriptTransaction(plan);
        gFo3ReferenceState=std::make_shared<const fo3world::ReferenceStateSnapshot>(player.CaptureReferenceState());
        gFo3ScenePublication.Publish(gFo3ReferenceState);
        error=restored?"script scene refresh queue unavailable":"script scene refresh rollback failed";return false;
    }
    gFo3ReferenceState=snapshot;gFo3ScenePublication.Publish(snapshot);error.clear();return true;
}
