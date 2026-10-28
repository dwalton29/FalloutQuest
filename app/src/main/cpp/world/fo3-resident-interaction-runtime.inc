// Included after UI, weapon, loose-body and actor declarations. Read-only
// render-thread probes: never poll effects, close a menu, release a hand or
// lazily instantiate a body/door/actor while deciding admission.
fo3scene::ResidentInteractionActivity CaptureFo3ResidentInteractionActivity() noexcept {
    fo3scene::ResidentInteractionActivity activity;
    activity.ready=gSceneReady&&!gSceneLoad.active&&!IsFo3LoadingVisible()&&
        gExteriorWorldspaceQ1890==0&&gPlayerSession&&!gPlayerSession->saveBlocked;
    activity.dialogue=Fo3DialogueFocus();activity.barter=Fo3BarterFocus();
    activity.pipboy=Fo3PipboyFocus();activity.message=Fo3ScriptMessageFocus();
    activity.lockpick=Fo3LockpickFocus()||Fo3TerminalFocus();activity.lockpickLatch=Fo3LockpickInputBlocked();
    activity.loot=GetFo3LootPanel().reference!=0;
    activity.weaponHandling=weaponruntime::held||weaponruntime::support||
        weaponruntime::magazine||weaponruntime::charging;
    activity.shoulderTransfer=gShoulderGesture.reference!=0||gShoulderGesture.armed;
    return activity;
}
fo3scene::ResidentInteractionHazard Fo3ResidentInteractionHazard(uint32_t reference) noexcept {
    using Hazard=fo3scene::ResidentInteractionHazard;
    if(!reference||!gPlayerSession)return Hazard::UnknownReference;
    const auto& player=gPlayerSession->player;
    const auto& catalog=player.Definitions();
    const auto ref=catalog.references.find(reference);
    if(ref==catalog.references.end()||!ref->second.valid||!ref->second.base||
        (ref->second.flags&0x20))return Hazard::UnknownReference;
    if(reference==fo3player::PlayerRef||catalog.actorPlacements.count(reference)||
        catalog.actorInventories.count(ref->second.base)||player.Snapshot().actors.count(reference))return Hazard::Actor;
    for(const auto& actor:gQ230NpcActors)
        if(actor.source.refFormId==reference)return Hazard::Actor;
    for(const auto& hand:gQ220Grab)
        if(hand.refFormId==reference)return Hazard::HeldObject;
    if(player.IsCollected(reference))return Hazard::CollectedObject;
    if(gQ223DynamicBodies.count(reference)||catalog.items.count(ref->second.base))return Hazard::LooseObject;
    if(gQ2400InteriorDoors.count(reference))return Hazard::SwingDoor;
    if(catalog.pipboy.furniture.count(ref->second.base))return Hazard::Furniture;
    for(const auto& object:gObjects)if(object.refFormId==reference) {
        if(object.q220LooseObject)return Hazard::LooseObject;
        if(object.q2400SwingDoor)return Hazard::SwingDoor;
        if(object.baseRecordType=="FURN")return Hazard::Furniture;
        if(object.baseRecordType=="NPC_"||object.baseRecordType=="CREA")return Hazard::Actor;
    }
    return Hazard::None;
}
