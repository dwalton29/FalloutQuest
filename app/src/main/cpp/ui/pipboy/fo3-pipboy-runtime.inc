// Physical Pip-Boy activation and session integration. Included after the
// authored body mount; never called from an eye renderer.
bool gMapTakingDamage=false;
float gMapPreviousDamage=-1;
const void* gMapDamageOwner=nullptr;
bool RequestFo3MapTravel(uint32_t marker,double now){
  auto* session=GetFo3PlayerSession();if(!session)return false;
  auto& player=session->player;const auto& d=player.Definitions().pipboy;
  auto reject=[&](const char* key){const auto text=player.Definitions().strings.find(key);gPipMenu.actionError=text==player.Definitions().strings.end()?fo3pipdata::TravelDenial(key):text->second;gPipMenu.dirty=true;return false;};
  if(session->saveBlocked||!gSceneReady||IsFo3LoadingVisible()||gSceneLoad.active||
     HasPendingFo3CellTransitionRequestQ74()||Fo3ResidentFrameBlocked())return reject("sNoFastTravelCell");
  if(!player.CanFastTravel(marker,gCurrentCellFormId,gExteriorWorldspaceQ1890,gPipMenu.actionError))return false;
  if(gMapTakingDamage)return reject("sFastTravelNoTravelHealthDamage");
  if(!IsFo3PlayerGrounded())return reject("sNoFastTravelInAir");
  const float nearDistance=gExteriorWorldspaceQ1890?d.hostileExteriorDistance:d.hostileInteriorDistance;
  for(const auto& actor:gQ230NpcActors){
    if(actor.runtime.offScene||!actor.runtime.Alive()||player.ActorHealth(actor.source.refFormId)<=0)continue;
    if(actor.runtime.activity==fo3npc::Activity::Combat&&actor.runtime.combatTarget==fo3player::PlayerRef)return reject("sNoFastTravelCombat");
    auto ai=fo3npc::AI(d,actor.source.baseFormId);float scriptedAI=0;
    if(player.Snapshot().actorAIValues.count((uint64_t(actor.source.refFormId)<<32)|0u)&&player.ActorAIValue(actor.source.refFormId,0,scriptedAI))ai.aggression=uint8_t(scriptedAI);
    if(player.Snapshot().actorAIValues.count((uint64_t(actor.source.refFormId)<<32)|57u)&&player.ActorAIValue(actor.source.refFormId,57,scriptedAI))ai.assistance=uint8_t(scriptedAI);
    const auto saved=player.Snapshot().actors.find(actor.source.refFormId);
    const bool hostile=(saved!=player.Snapshot().actors.end()&&saved->second.hostile==fo3player::PlayerRef)||
      fo3npc::Acquires(ai,fo3npc::Relationship(player.Definitions(),actor.source.baseFormId,fo3player::PlayerBase,{},{},&player.Snapshot().factionRelations));
    const auto& pos=actor.runtime.position;
    const float distance=Q211Length({pos[0]-gQ210Head[0],pos[1]-gQ210Head[1],pos[2]-gQ210Head[2]})*FO3_UNITS_PER_METRE;
    if(hostile&&distance<nearDistance)return reject("sNoFastTravelHostileActorsNear");
  }
  (void)now;
  const auto& destination=d.markers.at(marker);
  const auto origin=gPipMenu.map.location.world?gPipMenu.map.location:gPipMenu.map.exterior;
  // VR fallback for the unavailable original long-distance path-length service:
  // authored map coordinates + walk speed/time scale, using direct distance.
  const auto from=fo3pipdata::WorldPoint(d,origin.world,origin.x,origin.y);
  const auto to=fo3pipdata::WorldPoint(d,destination.world,destination.x,destination.y);
  const double hours=std::hypot(double(to.x)-from.x,double(to.y)-from.y)/d.travelWalkSpeed*player.Definitions().gameTimeScale/3600.;
  if(!QueueFo3MapMarkerTransitionQ74(marker,destination.x,destination.y,destination.z,
      destination.rx,destination.ry,destination.rz,hours))return reject("sNoFastTravelCell");
  fo3audio::NamedSound("UIPipBoyAccessDown");
  gPipActivation.Reset();gPipMenu.inPage=false;gPipMenu.dirty=true;
  // Input keeps its A latch across loading; confirming cannot activate a world
  // object on arrival. No clock/save mutation occurs before scene publication.
  Q6H_LOGI("FAST TRAVEL QUEUED marker=%08X name=%s hours=%.3f",marker,destination.name.c_str(),hours);
  return true;
}
void UpdateFo3Pipboy(uint64_t frame, double now, const float *headPose,
                     bool available, float x, float y, bool a, bool b,
                     float grip,float zoomTrigger) {
  if (!gPipMenu.BeginFrame(frame))
    return;
  const auto started = fqopaque::Clock::now();
  fo3hudrenderer::CachedStateGuard guard;
  if (gSceneReady && !fo3pipui::State().attempted)
    fo3pipui::Ensure();
  auto *session = GetFo3PlayerSession();
  fo3pip::View view;
  if (gPipSolved) {
    Vec3 c{gPipSurface.center.x / FO3_UNITS_PER_METRE,
           FLOOR_Y + gPipSurface.center.z / FO3_UNITS_PER_METRE,
           SCENE_FORWARD - gPipSurface.center.y / FO3_UNITS_PER_METRE};
    Vec3 n{gPipSurface.normal.x, gPipSurface.normal.z, -gPipSurface.normal.y};
    const auto centre = Q211TransformPoint(gPipWorld.data(), c);
    const auto normal = Q218TransformVector(gPipWorld.data(), n);
    float inverse[16];
    if (Q2016InvertAffine(gQ210PlayerRoot, inverse)) {
      auto body = Q211TransformPoint(inverse, centre);
      view = fo3pip::Measure(
          available && gQ210LeftHandValid && session && fo3pipui::State().ready,
          {centre.x, centre.y, centre.z}, {normal.x, normal.y, normal.z},
          {headPose[12], headPose[13], headPose[14]},
          {-headPose[8], -headPose[9], -headPose[10]}, {body.x, body.y, body.z},
          {gPipShoulder.x, gPipShoulder.y, gPipShoulder.z},
          {gPipHand.x, gPipHand.y, gPipHand.z}, gPipSolved);
    }
  }
  const auto old = gPipActivation.phase;
  gPipActivation.Step(view, now);
  if ((gQ211TrackingSerial % 180u) == 1u) {
    const auto eye = Q211TransformPoint(
        gQ210PlayerRoot,
        RuntimePoint(gVrNeckPose.Point(VrPoint(gVrEyeAnchor))));
    Q6H_LOGI("VR BODY: HMD=(%.3f %.3f %.3f) posedEye=(%.3f %.3f %.3f) "
             "eyeError=%.5f torsoDeg=%.1f neckDeg=%.1f pipMount=%d "
             "pipAvailable=%d pipValid=%d uiReady=%d sessionReady=%d "
             "distance=%.3f facingDeg=%.1f coneDeg=%.1f raised=%d phase=%d",
             gQ210Head[0], gQ210Head[1], gQ210Head[2], eye.x, eye.y, eye.z,
             fo3vr::Length(VrPoint(eye) -
                           fo3vr::V{gQ210Head[0], gQ210Head[1], gQ210Head[2]}),
             gQ213TorsoYaw * 57.29578f,
             fo3vr::Wrap(gQ210Head[3] - gQ213TorsoYaw) * 57.29578f, gPipSolved,
             available, view.valid, fo3pipui::State().ready, session != nullptr,
             view.distance,
             std::acos(std::clamp(view.facing, -1.f, 1.f)) * 57.29578f,
             std::acos(std::clamp(view.cone, -1.f, 1.f)) * 57.29578f,
             view.raised, int(gPipActivation.phase));
    Q6H_LOGI(
        "VR BODY TRACKING: source=%d graceAgeMs=%.0f gripValid=%d "
        "gripTracked=%d aimValid=%d aimTracked=%d headValid=%d headTracked=%d",
        int(gVrPipTracking.source), gVrPipTracking.graceAge * 1000,
        gVrLeftGripTracking.valid, gVrLeftGripTracking.tracked,
        gVrLeftAimTracking.valid, gVrLeftAimTracking.tracked,
        gVrHeadTracking.valid, gVrHeadTracking.tracked);
    for (int i = 0; i < 2; i++) {
      const auto &p = gVrSolvedArms[i];
      const auto shoulder =
          Q211TransformPoint(gQ210PlayerRoot, RuntimePoint(p.shoulder));
      const auto elbow =
          Q211TransformPoint(gQ210PlayerRoot, RuntimePoint(p.elbow));
      const auto palm =
          Q211TransformPoint(gQ210PlayerRoot, RuntimePoint(p.palm));
      const float *target = i == 0 ? gQ210LeftHand : gQ210RightHand;
      const bool valid = i == 0 ? gQ210LeftHandValid : gQ210RightHandValid;
      Q6H_LOGI(
          "VR BODY ELBOW: side=%d handRelative=(%.3f %.3f %.3f) "
          "elbowRelative=(%.3f %.3f %.3f) elbowBelowPalm=%.3f bendDeg=%.1f "
          "raisedWeight=%.3f preferred=(%.3f %.3f %.3f) pole=(%.3f %.3f %.3f)",
          i, p.palm.x - p.shoulder.x, p.palm.y - p.shoulder.y,
          p.palm.z - p.shoulder.z, p.elbow.x - p.shoulder.x,
          p.elbow.y - p.shoulder.y, p.elbow.z - p.shoulder.z,
          p.palm.y - p.elbow.y, p.flex * 57.29578f, p.raisedWeight,
          p.preferredPole.x, p.preferredPole.y, p.preferredPole.z, p.pole.x,
          p.pole.y, p.pole.z);
      Q6H_LOGI(
          "VR BODY ARM: side=%d valid=%d shoulder=(%.3f %.3f %.3f) "
          "target=(%.3f %.3f %.3f) palm=(%.3f %.3f %.3f) error=%.4f "
          "elbow=(%.3f %.3f %.3f) armScale=%.4f stretch=%.4f "
          "wristDistance=%.3f palmDistance=%.3f upper=%.3f fore=%.3f "
          "normalWristReach=%.3f normalPalmReach=%.3f clamped=%d rollDeg=%.1f",
          i, valid && p.valid, shoulder.x, shoulder.y, shoulder.z, target[0],
          target[1], target[2], palm.x, palm.y, palm.z, p.error, elbow.x,
          elbow.y, elbow.z, p.armScale, p.stretch, p.targetDistance,
          fo3vr::Length(VrPoint(shoulder) -
                        fo3vr::V{target[0], target[1], target[2]}),
          fo3vr::Length(p.elbow - p.shoulder), fo3vr::Length(p.wrist - p.elbow),
          p.normalReach,
          p.normalReach + fo3vr::Length(gVrArms[i].palm - gVrArms[i].wrist),
          p.clamped, p.roll * 57.29578f);
    }
  }
  if (old != gPipActivation.phase) {
    Q6H_LOGI("PIPBOY VIEW: state=%d->%d distance=%.3f facingDeg=%.1f "
             "viewDeg=%.1f raised=%d debounceMs=150",
             int(old), int(gPipActivation.phase), view.distance,
             std::acos(std::clamp(view.facing, -1.f, 1.f)) * 57.2958f,
             std::acos(std::clamp(view.cone, -1.f, 1.f)) * 57.2958f,
             view.raised);
    if (old == fo3pip::Phase::Active || Fo3PipboyFocus()) {
      gPipMenu.dirty = true;
      fo3audio::NamedSound(Fo3PipboyFocus() ? "UIPipBoyAccessUp"
                                            : "UIPipBoyAccessDown");
    }
  }
  if (session) {
    const float damage=session->player.Snapshot().healthDamage;
    gMapTakingDamage=gMapDamageOwner==session&&gMapPreviousDamage>=0&&damage>gMapPreviousDamage;
    gMapDamageOwner=session;gMapPreviousDamage=damage;
    static double lastContext = 0, lastPan = 0;
    static uint32_t lastRadio = UINT32_MAX, lastNote = 0, lastMapCell = 0;
    static uint64_t lastMapGeneration = UINT64_MAX,
                    lastRadioRevision = UINT64_MAX;
    const float gameX = (gExteriorStreamingActiveQ1890 ? gExteriorOriginXQ1890
                                                       : gSceneCenterXQ1730) +
                        headPose[12] * FO3_UNITS_PER_METRE;
    const float gameY = (gExteriorStreamingActiveQ1890 ? gExteriorOriginYQ1890
                                                       : gSceneCenterYQ1730) +
                        (SCENE_FORWARD - headPose[14]) * FO3_UNITS_PER_METRE;
    auto &defs = session->player.Definitions().pipboy;
    const uint32_t world =
        gExteriorWorldspaceQ1890;
    if (now - lastContext > .5) {
      lastContext = now;
      auto previous = gPipMenu.map.location;
      gPipMenu.map.location = {gCurrentCellFormId, world, gameX, gameY,
                               std::atan2(-headPose[8], headPose[10])};
      if(world){
        gPipMenu.map.exterior=gPipMenu.map.location;
        if(gSceneReady&&!IsFo3LoadingVisible()&&!session->saveBlocked)session->player.TrackExteriorLocation(gPipMenu.map.location);
      }else{
        const auto entrance=fo3pipdata::ExteriorEntrance(defs,gCurrentCellFormId,gameX,gameY);
        const auto saved=session->player.Snapshot().pipboy.lastExterior;
        gPipMenu.map.exterior=entrance.world?entrance:saved;
      }
      auto radioLocation = gPipMenu.map.location;
      if (!world && gPipMenu.map.exterior.world) {
        radioLocation.world = gPipMenu.map.exterior.world;
        radioLocation.x = gPipMenu.map.exterior.x;
        radioLocation.y = gPipMenu.map.exterior.y;
      }
      auto stations = fo3pipdata::AvailableStations(defs, radioLocation,[&](uint32_t id){bool disabled=true;std::string error;return session->player.ReferenceDisabled(id,disabled,error)&&!disabled;});
      if (stations != gPipMenu.stations) {
        gPipMenu.stations = std::move(stations);
        gPipMenu.RebuildRows(session->player);
        gPipMenu.dirty = true;
      }
      if(world&&gSceneReady&&!IsFo3LoadingVisible()&&gVrHeadTracking.valid&&!session->saveBlocked){
        const float gameZ=gSceneFloorZQ1730+(headPose[13]-FLOOR_Y)*FO3_UNITS_PER_METRE;
        const auto count=session->player.TrackLocations(world,gameX,gameY,gameZ);
        if(count){Q6H_LOGI("LOCATIONS DISCOVERED count=%zu authoredXP=%u",count,defs.discoveryXP);FlushFo3PlayerState();}
      }
      if(Fo3PipboyFocus()&&gPipMenu.tab==fo3pip::Tab::Data&&gPipMenu.page<2)gPipMenu.dirty=true;
      if (Fo3PipboyFocus() && gPipMenu.tab == fo3pip::Tab::Data &&
          gPipMenu.page < 2 &&
          (previous.cell != gCurrentCellFormId ||
           std::fabs(previous.x - gameX) > 1 ||
           std::fabs(previous.y - gameY) > 1 ||
           std::fabs(previous.facing - gPipMenu.map.location.facing) > .01))
        gPipMenu.dirty = true;
    }
    if (Fo3PipboyFocus() && gPipMenu.tab == fo3pip::Tab::Data &&
        gPipMenu.page < 2) {
      const auto exterior =
          world ? gPipMenu.map.location : gPipMenu.map.exterior;
      uint32_t mapWorld = fo3pipdata::MapWorld(defs, exterior.world);
      if (mapWorld != gPipMenu.map.mapWorld) {
        gPipMenu.map.mapWorld = mapWorld;
        if(mapWorld){const auto& w=defs.worlds.at(mapWorld);
          Q6H_LOGI("WORLD MAP PROJECTION world=%08X image=%s usable=%dx%d NW=(%d,%d) SE=(%d,%d) playerGame=(%.2f,%.2f)",mapWorld,w.image.c_str(),w.width,w.height,int(w.nwX),int(w.nwY),int(w.seX),int(w.seY),exterior.x,exterior.y);
        }
        gPipMenu.dirty = true;
      }
      if (gPipMenu.page == 0) {
        if (lastMapCell != gCurrentCellFormId ||
            lastMapGeneration != fo3pipdata::LocalMapGeometryRevision() ||
            !gPipMenu.map.local) {
          gPipMenu.map.local = fo3pipdata::CaptureLocalMap(
              gCurrentCellFormId, fo3pipdata::LocalMapGeometryRevision(),
              headPose[13]);
          lastMapCell = gCurrentCellFormId;
          lastMapGeneration = fo3pipdata::LocalMapGeometryRevision();
          gPipMenu.dirty = true;
          Q6H_LOGI("PIPBOY LOCAL MAP cell=%08X generation=%llu pixels=%zu",
                  lastMapCell, (unsigned long long)lastMapGeneration,
                  gPipMenu.map.local->rgba.size());
        }
        gPipMenu.map.localOrigin = {
            gExteriorStreamingActiveQ1890 ? gExteriorOriginXQ1890
                                          : gSceneCenterXQ1730,
            gExteriorStreamingActiveQ1890 ? gExteriorOriginYQ1890
                                          : gSceneCenterYQ1730};
        gPipMenu.map.sceneForward = SCENE_FORWARD;
        gPipMenu.localX = headPose[12];
        gPipMenu.localY = -headPose[14];
        if (gPipMenu.map.local)
          gPipMenu.map.player =
              gPipMenu.map.local->Project(gPipMenu.localX, gPipMenu.localY);
      } else if (mapWorld) {
        gPipMenu.map.player = fo3pipdata::MapProject(defs, exterior.world,
                                                     exterior.x, exterior.y);
      }
      const bool playerKnown=gPipMenu.page==0?bool(gPipMenu.map.local):mapWorld!=0;
      if(playerKnown!=gPipMenu.map.playerKnown){gPipMenu.map.playerKnown=playerKnown;gPipMenu.dirty=true;}
      if (!gPipMenu.MapInteraction())
        gPipMenu.map.centre = gPipMenu.map.player;
      const float seconds=float(std::clamp(now-lastPan,0.0,.05));
      if(zoomTrigger>.65f)gPipMenu.Zoom(y,seconds,defs);
      else gPipMenu.Pan(x,y,seconds);
      gPipMenu.FocusMapMarker(session->player);
    }
    lastPan = now;
    gPipMenu.Refresh(session->player);
    gPipInput.Step(
        Fo3PipboyFocus(), x, y, a, b, grip, now, gPipMenu.MapInteraction(),
        [&](fo3pip::Action action) {
          bool mutation = gPipMenu.Invoke(action, session->player);
          if (mutation) {
            FlushFo3PlayerState();
            SynchronizeFo3PipboyEquipment();
          }
          Q6H_LOGI("PIPBOY ACTION tab=%d page=%d action=%d detail=%d error=%s",
                  int(gPipMenu.tab), gPipMenu.page, int(action),
                  gPipMenu.inPage, gPipMenu.actionError.c_str());
          fo3audio::NamedSound(action == fo3pip::Action::NextTab ||
                                       action == fo3pip::Action::PreviousTab
                                   ? "UIPipBoyMode"
                               : action == fo3pip::Action::Accept
                                   ? "UIPipBoySelect"
                               : action == fo3pip::Action::Back ||
                                       action == fo3pip::Action::NextPage ||
                                       action == fo3pip::Action::PreviousPage
                                   ? "UIPipBoyTab"
                                   : "UIPipBoyHighlight");
        });
    if(gPipMenu.travelRequest){const auto marker=gPipMenu.travelRequest;gPipMenu.travelRequest=0;RequestFo3MapTravel(marker,now);}
    auto tuned = session->player.Snapshot().pipboy.tunedRadio;
    if (tuned) {
      auto radioLocation = gPipMenu.map.location;
      if (!world && gPipMenu.map.exterior.world) {
        radioLocation.world = gPipMenu.map.exterior.world;
        radioLocation.x = gPipMenu.map.exterior.x;
        radioLocation.y = gPipMenu.map.exterior.y;
      }
      auto tx = defs.transmitters.find(tuned);
      if (tx == defs.transmitters.end() ||
          !fo3pipdata::InRange(defs, tx->second, radioLocation,[&](uint32_t id){bool disabled=true;std::string error;return session->player.ReferenceDisabled(id,disabled,error)&&!disabled;}))
        tuned = 0;
    }
    if (lastRadio != tuned) {
      lastRadio = tuned;
      fo3audio::Radio(tuned, defs, session->player.Snapshot().pipboy);
      lastRadioRevision = session->player.Revision();
    }
    if (tuned && lastRadioRevision != session->player.Revision()) {
      lastRadioRevision = session->player.Revision();
      fo3audio::RadioState(session->player.Snapshot().pipboy);
    }
    if (lastNote && gPipMenu.playingNote == lastNote &&
        !fo3audio::PlayingNote()) {
      gPipMenu.playingNote = 0;
      gPipMenu.dirty = true;
    }
    if (lastNote != gPipMenu.playingNote) {
      lastNote = gPipMenu.playingNote;
      fo3audio::Note(lastNote, fo3pipdata::NoteAudio(defs, lastNote));
    }
  }
  gPipUpdateUs = fqopaque::Micros(started);
}
