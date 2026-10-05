#include "ui/interaction/fo3-item-notifications.h"

void NotifyFo3ItemTransfer(uint32_t base,int32_t count) {
    const auto& catalog=gPlayerSession->player.Definitions();
    const auto item=catalog.items.find(base);
    if(item==catalog.items.end()) return;
    auto setting=[&](const char* key,const char* fallback) {
        const auto it=catalog.strings.find(key);
        return it==catalog.strings.end() ? std::string(fallback) : it->second;
    };
    fo3notify::Notifications().Enqueue(fo3notify::ItemText(item->second.name,count,
        setting("sAddItemtoInventory","added"),setting("sPlural","(s)")));
    fo3audio::Pickup(base);
    FlushFo3PlayerState();
    Q6H_LOGI("ITEM ADDED HUD: base=%08X count=%d queued=%zu",base,count,fo3notify::Notifications().Size());
}

// Render-thread transaction shared by A-button and physical shoulder release.
// CanPickup/Pickup remain the only inventory/ownership/count/condition policy.
bool CollectFo3WorldReference(uint32_t reference) {
    if(!gSceneReady || IsFo3LoadingVisible() || !gPlayerSession || gPlayerSession->saveBlocked) return false;
    auto& player=gPlayerSession->player;
    if(!player.CanPickup(reference)) return false;
    const auto ref=player.Definitions().references.at(reference);
    if(player.Definitions().items.at(ref.base).weapon.Firearm())return false;
    if(!player.Pickup(reference)) return false;
    for(auto& hand:gQ220Grab) if(hand.refFormId==reference) hand={};
    gQ223DynamicBodies.erase(reference);
    SetFo3CollectedCollisionRefs(player.Snapshot().collected);
    NotifyFo3ItemTransfer(ref.base,ref.count);
    Q6H_LOGI("ITEM PICKUP: reference=%08X base=%08X count=%d condition=%.3f weight=%.2f stacks=%zu",reference,ref.base,ref.count,ref.condition,
        player.InventoryWeight(),player.Snapshot().inventory.size());
    return true;
}

// Included after loose-object transforms: all operations run on the render thread.
bool QueryFo3Interaction(float ox, float oy, float oz, float dx, float dy,
                         float dz, Fo3InteractionTarget &target) {
    target = {};
    if (!gSceneReady || IsFo3LoadingVisible() || Fo3DialogueFocus() || Fo3PipboyFocus() || !gPlayerSession) return false;
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
        // Inventory eligibility comes from the item catalog, independently
        // of whether the physics runtime supports this loose-object type.
        if (player.IsCollected(object.refFormId) ||
            (!container && !catalog.items.count(object.baseFormId))) continue;
        if(!container&&catalog.items.at(object.baseFormId).weapon.Firearm())continue;
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
    for(auto& actor:gQ230NpcActors) {
        if(player.ActorHealth(actor.source.refFormId)<=0||actor.source.fullName.empty())continue;
        std::array<float,3> lo,hi;float distance=0;
        if(Q230LiveBounds(actor,lo,hi)&&fo3interaction::Box({ox,oy,oz},{dx,dy,dz},lo,hi,fo3npc::DialoguePolicy::TalkRange,distance)&&distance<best&&DialogueCanStart(actor)) {
            best=distance;target={};target.reference=actor.source.refFormId;target.talk=true;target.allowed=true;
            std::snprintf(target.prompt.data(),target.prompt.size(),"Talk %s",actor.source.fullName.c_str());
        }
    }
    if (!target.reference || HasFo3InteractionOccluder(ox,oy,oz,dx,dy,dz,best,target.reference)) {
        target={}; return false;
    }
    target.distance=best;
    if(target.talk)return true;
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
        const char* action = target.allowed ? "Take " : "Unavailable ";
        if (!target.allowed && ref->second.owner && ref->second.owner != fo3player::PlayerBase)
            action = "Owned ";
        else if (!target.allowed && item.script) action = "Scripted ";
        std::snprintf(target.prompt.data(),target.prompt.size(),"%s%s",action,item.name.c_str());
    } else {
        if (!GetFo3DoorPromptQ1840(target.reference,target.prompt.data(),target.prompt.size())) {
            std::snprintf(target.prompt.data(),target.prompt.size(),"%s Door",
                target.door.localSwing && target.door.open ? "Close" : "Open");
        }
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
    const bool valid=target.container && target.allowed && gPlayerSession && !IsFo3LoadingVisible();
    const auto previousReference=gLootPanel.reference;
    if (previousReference && (!valid || previousReference!=target.reference) && gPlayerSession) {
        const auto ref=gPlayerSession->player.Definitions().references.find(previousReference);
        if (ref!=gPlayerSession->player.Definitions().references.end()) fo3audio::Close(ref->second.base);
    }
    if (!valid) {
        gLootPanel.reference=0;gLootPanel.rows.clear();gLootPanel.title.clear();
        gLootCursor.Target(0,0);return;
    }
    auto& player=gPlayerSession->player;
    const auto revision=player.Revision();
    if (!player.PrepareContainer(target.reference)) {
        if (previousReference==target.reference) fo3audio::Close(player.Definitions().references.at(target.reference).base);
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
    if (previousReference!=target.reference) fo3audio::Open(catalog.references.at(target.reference).base);
    gLootPanel.reference=target.reference;gLootPanel.anchor=target.anchor;
    gLootCursor.Target(target.reference,gLootPanel.rows.size());
    const auto previousSelection=gLootCursor.selected;
    gLootCursor.Scroll(stickY,time,gLootPanel.rows.size());
    if (previousSelection!=gLootCursor.selected) fo3audio::Scroll();
    gLootPanel.selected=gLootCursor.selected;
}

bool ActivateFo3Interaction(float ox,float oy,float oz,float dx,float dy,float dz) {
    Fo3InteractionTarget target;
    if (!QueryFo3Interaction(ox,oy,oz,dx,dy,dz,target) || !target.allowed) return false;
    if(target.talk){StartFo3Dialogue(target.reference);return false;}
    if (target.container) {
        // Selection comes from this frame's target; never use a previous container.
        if (gLootPanel.reference!=target.reference || gLootPanel.rows.empty()) return false;
        const auto& row=gLootPanel.rows[gLootPanel.selected];
        uint32_t base=0;
        int32_t count=0;
        for (const auto& stack:*gPlayerSession->player.ContainerContents(target.reference))
            if (stack.id==row.stack) {base=stack.formId;count=stack.count;}
        if (row.allowed && gPlayerSession->player.TakeContainerStack(target.reference,row.stack)) {
            NotifyFo3ItemTransfer(base,count);
            UpdateFo3LootSelection(target,NAN,0);
        }
        return false;
    }
    if (!target.pickup) {
        const bool localSwing=target.door.localSwing;
        const bool wasOpen=target.door.open;
        const bool activated=ActivateDoorInternalQ1700(ox,oy,oz,dx,dy,dz);
        if (activated) {
            const auto ref=gPlayerSession->player.Definitions().references.find(target.reference);
            if (ref!=gPlayerSession->player.Definitions().references.end()) {
                if(localSwing && wasOpen) fo3audio::Close(ref->second.base);
                else fo3audio::Open(ref->second.base);
            }
        }
        // Only XTEL doors reset the VR origin. A local swing activation is
        // successful but remains in this CELL.
        return activated && !localSwing;
    }
    CollectFo3WorldReference(target.reference);
    return false;
}
