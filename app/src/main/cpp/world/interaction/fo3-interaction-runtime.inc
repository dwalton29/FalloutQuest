// Included after loose-object transforms: all operations run on the render thread.
bool QueryFo3Interaction(float ox, float oy, float oz, float dx, float dy,
                         float dz, Fo3InteractionTarget &target) {
    target = {};
    if (!gSceneReady || IsFo3LoadingVisible() || !gPlayerSession) return false;
    const float length = std::sqrt(dx*dx+dy*dy+dz*dz);
    if (!std::isfinite(length) || length < 1e-5f ||
        !std::isfinite(ox) || !std::isfinite(oy) || !std::isfinite(oz)) return false;
    dx /= length; dy /= length; dz /= length;
    auto& player = gPlayerSession->player;
    const auto& catalog = player.Definitions();
    float best = 3.001f;
    Fo3DoorAimQ1700 door;
    if (QueryDoorInternalQ1700(ox,oy,oz,dx,dy,dz,&door)) {
        best = door.distance;
        target.reference = door.sourceDoorRef;
        target.door = door;
        target.allowed = player.CanOpenDoor(target.reference);
    }
    for (const auto& object : gObjects) {
        if (!object.q220LooseObject || player.IsCollected(object.refFormId) ||
            !catalog.items.count(object.baseFormId)) continue;
        fo3interaction::Point lo{1e30f,1e30f,1e30f}, hi{-1e30f,-1e30f,-1e30f};
        for (int corner=0;corner<8;++corner) {
            const auto p = Q220TransformPoint(object.q220DynamicTransform,
                Vec3{corner&1 ? object.maxX : object.minX,
                     corner&2 ? object.maxY : object.minY,
                     corner&4 ? object.maxZ : object.minZ});
            lo[0]=std::min(lo[0],p.x); hi[0]=std::max(hi[0],p.x);
            lo[1]=std::min(lo[1],p.y); hi[1]=std::max(hi[1],p.y);
            lo[2]=std::min(lo[2],p.z); hi[2]=std::max(hi[2],p.z);
        }
        // Small VR aiming tolerance, separate from authored mesh geometry.
        for (int i=0;i<3;++i) {lo[i]-=0.015f;hi[i]+=0.015f;}
        float distance;
        if (fo3interaction::Box({ox,oy,oz},{dx,dy,dz},lo,hi,3,distance) && distance<best) {
            best=distance; target.reference=object.refFormId; target.pickup=true;
            target.allowed=!gPlayerSession->saveBlocked && player.CanPickup(target.reference);
            target.door={};
        }
    }
    if (!target.reference || HasFo3InteractionOccluder(ox,oy,oz,dx,dy,dz,best,target.reference)) {
        target={}; return false;
    }
    target.distance=best;
    if (target.pickup) {
        const auto& ref=catalog.references.find(target.reference);
        if (ref==catalog.references.end()) {target={};return false;}
        const auto& item=catalog.items.at(ref->second.base);
        if (item.name.empty()) {target={};return false;}
        // Vanilla English action labels are present in the supplied Fallout3.exe.
        std::snprintf(target.prompt.data(),target.prompt.size(),"%s%s",
                      target.allowed ? "Take " : "",item.name.c_str());
    } else {
        GetFo3DoorPromptQ1840(target.reference,target.prompt.data(),target.prompt.size());
        if (!target.allowed) {
            const auto ref=catalog.references.find(target.reference);
            if (ref!=catalog.references.end() && ref->second.locked)
                std::snprintf(target.prompt.data(),target.prompt.size(),"Locked");
        }
    }
    return true;
}

bool ActivateFo3Interaction(float ox,float oy,float oz,float dx,float dy,float dz) {
    Fo3InteractionTarget target;
    if (!QueryFo3Interaction(ox,oy,oz,dx,dy,dz,target) || !target.allowed) return false;
    if (!target.pickup) return ActivateDoorInternalQ1700(ox,oy,oz,dx,dy,dz);
    if (!gPlayerSession->player.Pickup(target.reference)) return false;
    for (auto& hand : gQ220Grab) if (hand.refFormId==target.reference) hand={};
    gQ223DynamicBodies.erase(target.reference);
    SetFo3CollectedCollisionRefs(gPlayerSession->player.Snapshot().collected);
    FlushFo3PlayerState();
    Q6H_LOGI("ITEM PICKUP: reference=%08X weight=%.2f stacks=%zu",target.reference,
        gPlayerSession->player.InventoryWeight(),gPlayerSession->player.Snapshot().inventory.size());
    return false;
}
