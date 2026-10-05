// Physical Pip-Boy activation and session integration. Included after the
// authored body mount; never called from an eye renderer.
void UpdateFo3Pipboy(uint64_t frame, double now, const float *headPose,
                     bool available, float x, float y, bool a, bool b,
                     float grip) {
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
        gExteriorStreamingActiveQ1890 ? gExteriorWorldspaceQ1890 : 0;
    if (now - lastContext > .5) {
      lastContext = now;
      auto previous = gPipMenu.map.location;
      gPipMenu.map.location = {gCurrentCellFormId, world, gameX, gameY,
                               std::atan2(-headPose[8], headPose[10])};
      if (world)
        gPipMenu.map.exterior = gPipMenu.map.location;
      auto radioLocation = gPipMenu.map.location;
      if (!world && gPipMenu.map.exterior.world) {
        radioLocation.world = gPipMenu.map.exterior.world;
        radioLocation.x = gPipMenu.map.exterior.x;
        radioLocation.y = gPipMenu.map.exterior.y;
      }
      auto stations = fo3pipdata::AvailableStations(defs, radioLocation);
      if (stations != gPipMenu.stations) {
        gPipMenu.stations = std::move(stations);
        gPipMenu.RebuildRows(session->player);
        gPipMenu.dirty = true;
      }
      // Exact discovery radius is not present in supplied GMSTs. VR runtime
      // adaptation: discover enabled markers in the occupied authored 4096-unit
      // exterior CELL; never auto-discover the entire world.
      if (world) {
        for (auto &m : defs.markers)
          if (fo3pipdata::MapWorld(defs, m.second.world) ==
                  fo3pipdata::MapWorld(defs, world) &&
              fo3pipdata::Enabled(defs, m.second) &&
              int(std::floor(m.second.x / 4096)) ==
                  int(std::floor(gameX / 4096)) &&
              int(std::floor(m.second.y / 4096)) ==
                  int(std::floor(gameY / 4096)) &&
              !session->player.Snapshot().pipboy.discovered.count(m.first)) {
            session->player.Discover(m.first);
            Q6H_LOGI("PIPBOY MAP discovered=%08X name=%s", m.first,
                    m.second.name.c_str());
            FlushFo3PlayerState();
          }
      }
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
      if (!gPipMenu.MapInteraction())
        gPipMenu.map.centre = gPipMenu.map.player;
      gPipMenu.Pan(x, y, float(std::clamp(now - lastPan, 0.0, .05)));
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
          !fo3pipdata::InRange(defs, tx->second, radioLocation))
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
