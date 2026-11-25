// The source event has explicitly delegated to the normal door action.
// Recheck enable state and lock/key access at delivery; never re-enter OnActivate.
bool DeliverFo3ScriptDefaultActivation(uint32_t reference,uint32_t activator){
    if(!gPlayerSession||Fo3ResidentFrameBlocked())return false;
    auto& player=gPlayerSession->player;
    // A switch is a legal action reference for its authored activation children.
    // It has no NPC runtime/health and must not be rejected as a dead actor.
    if(activator!=fo3player::PlayerRef&&!player.Definitions().actorPlacements.count(activator)&&
       player.IsActivationParent(reference,activator)){
        if(!player.CanScriptActivateDoor(reference,activator))return false;
        if(!Q2400FindSwingDoorObject(reference))return player.ScriptDoorState(reference,player.ScriptOpenState(reference)==3);
        const bool wasOpen=Q2400DoorOpen(reference);
        if(!Q2400ToggleInteriorDoor(reference,gQ210Head[0],gQ210Head[2]))return false;
        const auto placed=player.Definitions().references.find(reference);
        if(placed!=player.Definitions().references.end()){
            if(wasOpen)fo3audio::Close(placed->second.base);else fo3audio::Open(placed->second.base);
        }
        FlushFo3PlayerState();return true;
    }
    if(activator!=fo3player::PlayerRef){
        const auto* actor=DialogueActor(activator);
        if(!actor||actor->runtime.offScene||!player.CanScriptActivateDoor(reference,activator)||
           !Q2400FindSwingDoorObject(reference))return false;
        if(Q2400DoorOpen(reference))return true;
        const auto& origin=actor->runtime.position;
        if(!Q2400ToggleInteriorDoor(reference,origin[0],origin[2]))return false;
        const auto door=player.Definitions().references.find(reference);
        if(door!=player.Definitions().references.end())fo3audio::Open(door->second.base);
        FlushFo3PlayerState();return true;
    }
    const auto object=player.Definitions().references.find(reference);
    if(object!=player.Definitions().references.end()&&player.Definitions().pipboy.furniture.count(object->second.base)){
        bool disabled=true;std::string error;
        return player.ReferenceDisabled(reference,disabled,error)&&!disabled&&BeginFo3PlayerSit(reference);
    }
    if(player.CanScriptActivateContainer(reference)){
        if(!player.OpenScriptContainer(reference))return false;
        FlushFo3PlayerState();return true;
    }
    if(!player.CanScriptActivateDoor(reference))return false;
    if(player.GetLock(reference).locked&&!player.UnlockByKey(reference))return false;
    Fo3DoorTeleport arrival;
    if(ResolveFo3DoorTeleportQ1700(reference,&arrival)&&arrival.valid)
        return QueueFo3DoorTransitionQ1700(reference,arrival.destinationDoorRefFormId,
            arrival.x,arrival.y,arrival.z,arrival.rx,arrival.ry,arrival.rz);
    if(!Q2400ToggleInteriorDoor(reference,gQ210Head[0],gQ210Head[2]))return false;
    if(!gPlayerSession->player.ScriptDoorState(reference,Q2400DoorOpen(reference)))return false;
    FlushFo3PlayerState();return true;
}
