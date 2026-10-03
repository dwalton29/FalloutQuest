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
        const bool container=catalog.containers.count(object.baseFormId)!=0;
        if ((!object.q220LooseObject && !container) || player.IsCollected(object.refFormId) ||
            (!container && !catalog.items.count(object.baseFormId))) continue;
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
            best=distance; target.reference=object.refFormId; target.pickup=!container;
            target.container=container;
            const float panelDistance=std::max(0.0f,distance-.15f);
            target.anchor={ox+dx*panelDistance,oy+dy*panelDistance+.22f,oz+dz*panelDistance};
            target.allowed=!gPlayerSession->saveBlocked &&
                (container ? player.CanLootContainer(target.reference) : player.CanPickup(target.reference));
            target.door={};
        }
    }
    if (!target.reference || HasFo3InteractionOccluder(ox,oy,oz,dx,dy,dz,best,target.reference)) {
        target={}; return false;
    }
    target.distance=best;
    if (target.container) {
        const auto& ref=catalog.references.at(target.reference);
        const auto& container=catalog.containers.at(ref.base);
        std::snprintf(target.prompt.data(),target.prompt.size(),"%s%s",
            ref.locked&&!target.allowed ? "Locked " : "",container.name.c_str());
    } else if (target.pickup) {
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

fo3loot::Cursor gLootCursor;
Fo3LootPanel gLootPanel;
uint64_t gLootPanelRevision=UINT64_MAX;
const Fo3LootPanel& GetFo3LootPanel() {return gLootPanel;}
void UpdateFo3LootSelection(const Fo3InteractionTarget& target,float stickY,double time) {
    if (!target.container || !target.allowed || !gPlayerSession || IsFo3LoadingVisible()) {
        gLootPanel.reference=0;gLootPanel.rows.clear();gLootPanel.title.clear();
        gLootCursor.Target(0,0);return;
    }
    auto& player=gPlayerSession->player;
    const auto revision=player.Revision();
    if (!player.PrepareContainer(target.reference)) {
        gLootPanel.reference=0;gLootPanel.rows.clear();gLootCursor.Target(0,0);return;
    }
    if (revision!=player.Revision()) FlushFo3PlayerState();
    const auto& catalog=player.Definitions();
    if (gLootPanel.reference!=target.reference || gLootPanelRevision!=player.Revision()) {
        gLootPanel.rows.clear();
        gLootPanel.title=catalog.containers.at(catalog.references.at(target.reference).base).name;
        for (const auto& stack:*player.ContainerContents(target.reference)) {
            const auto& item=catalog.items.at(stack.formId);
            if (!item.name.empty()) gLootPanel.rows.push_back({stack.id,item.name,stack.count,
                player.CanTakeContainerStack(target.reference,stack.id)});
        }
        gLootPanelRevision=player.Revision();
    }
    gLootPanel.reference=target.reference;gLootPanel.anchor=target.anchor;
    gLootCursor.Target(target.reference,gLootPanel.rows.size());
    gLootCursor.Scroll(stickY,time,gLootPanel.rows.size());
    gLootPanel.selected=gLootCursor.selected;
}

bool ActivateFo3Interaction(float ox,float oy,float oz,float dx,float dy,float dz) {
    Fo3InteractionTarget target;
    if (!QueryFo3Interaction(ox,oy,oz,dx,dy,dz,target) || !target.allowed) return false;
    if (target.container) {
        // Selection comes from this frame's target; never use a previous container.
        if (gLootPanel.reference!=target.reference || gLootPanel.rows.empty()) return false;
        const auto& row=gLootPanel.rows[gLootPanel.selected];
        if (row.allowed && gPlayerSession->player.TakeContainerStack(target.reference,row.stack)) {
            FlushFo3PlayerState();
            UpdateFo3LootSelection(target,NAN,0);
        }
        return false;
    }
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
