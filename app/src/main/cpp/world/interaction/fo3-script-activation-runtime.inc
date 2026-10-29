// Original reference OnActivate bridge. Pending world work is not Executed.
void FlushFo3PlayerState(void);
bool PublishFo3DialogueReferences(fo3player::Player&,fo3player::ScriptTransactionPlan&,std::string&);
fo3player::ScriptEventResult DispatchFo3ScriptedActivation(
        uint32_t reference,uint32_t activator,std::string& error) {
    using Result=fo3player::ScriptEventResult;
    error.clear();
    if(!gPlayerSession||gPlayerSession->saveBlocked||Fo3ResidentFrameBlocked()){
        error="script activation scene or save unavailable";return Result::Unsupported;
    }
    auto& player=gPlayerSession->player;
    if(!player.HasReferenceEvent(reference,"OnActivate"))return Result::NoHandler;
    auto plan=player.PrepareReferenceEventTransaction(reference,"OnActivate",activator,error,
        fo3player::ScriptProgramCapture::EnteredPrograms);
    if(!plan)return Result::Unsupported;
    if(!plan->HasReferenceChanges()){
        if(player.CommitScriptTransaction(*plan))return Result::Executed;
        error="script event transaction changed before publication";return Result::Unsupported;
    }
    // The same logical publication path as dialogue/OnLoad handles changes
    // wholly outside the current scene (e.g. customers in a destination CELL).
    if(PublishFo3DialogueReferences(player,*plan,error))return Result::Executed;
    const auto ticket=BeginFo3ResidentPreparedWorkflow({},std::move(plan));
    if(!ticket){
        if(error.empty())error="resident scripted activation could not reserve a safe scene";
        return Result::Unsupported;
    }
    gFo3ResidentGameplayTicket=ticket;
    // Defer disk persistence until the owned exchange has settled and released.
    FlushFo3PlayerState();
    return Result::Pending;
}
