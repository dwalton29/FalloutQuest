// The source event has explicitly delegated to the normal door action.
// Recheck enable state and lock/key access at delivery; never re-enter OnActivate.
bool DeliverFo3ScriptDefaultActivation(uint32_t reference){
    if(!gPlayerSession||Fo3ResidentFrameBlocked())return false;
    auto& player=gPlayerSession->player;
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
