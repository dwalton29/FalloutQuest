namespace {
bool gFo3TitleInitialized = false, gFo3TitleMenuDown = false;
void InitializeFo3Title() {
  if (gFo3TitleInitialized)
    return;
  gFo3TitleInitialized = true;
  gFo3Title.saves = fo3title::Saves(fo3title::SaveRoot);
  fo3title::ReadVolumes(gFo3Title, fo3title::SaveRoot);
  fo3audio::Volumes(gFo3Title.volume);
  auto path =
      fo3audio::FindAudioFile(fo3assets::FalloutDataPath(""), "Credits.txt");
  if (!path.empty())
    if (FILE *f = std::fopen(path.c_str(), "rb")) {
      char line[1024];
      while (gFo3Title.credits.size() < 10000 &&
             std::fgets(line, sizeof(line), f)) {
        auto s = fo3menu::Trim(line);
        if (!s.empty())
          gFo3Title.credits.push_back(s);
      }
      std::fclose(f);
    }
  for (const char *s :
       {"UIMenuFocus", "UIMenuOK", "UIMenuCancel", "UIMenuPrevNext", "MUSDeath"})
    fo3audio::PreloadSound(s);
}
void UpdateFo3Title(bool menu, bool a, bool b, float x, float y, double now,
                    bool focused, bool &quit) {
  if (!gFo3Title.open)
    return;
  InitializeFo3Title();
  if (menu && !gFo3TitleMenuDown && focused)
    fo3titleui::Recenter();
  gFo3TitleMenuDown = menu;
  const auto action = gFo3Title.Input(
      a, b, x, y, now, focused, [](const char *s) { fo3audio::NamedSound(s); });
  using A = fo3title::Model::Action;
  if (action == A::Volume) {
    fo3audio::Volumes(gFo3Title.volume);
    if (!fo3title::WriteVolumes(gFo3Title, fo3title::SaveRoot))
      gFo3Title.Notice("Could not save audio settings.");
  } else if (action == A::Recenter)
    fo3titleui::Recenter();
  else if (action == A::Quit) {
    quit = true;
  } else if (action == A::Continue || action == A::NewGame) {
    gFo3StartupNew = action == A::NewGame;
    gFo3StartupError.clear();
    if(gFo3StartupNew){
      std::string source;
      if(!fo3newgame::FindQuest(fo3assets::FalloutDataRoot(),gFo3StartupQuest,source,gFo3StartupError)){
        gFo3Title.Notice(gFo3StartupError);return;
      }
      Q6H_LOGI("NEW GAME CONFIG: source=%s quest=%08X",source.c_str(),gFo3StartupQuest);
    }
    gFo3StartupPath = gFo3StartupNew
                          ? fo3title::NewPath(fo3title::SaveRoot,
                                              uint64_t(std::time(nullptr)))
                          : gFo3Title.loadPath;
    if (gFo3StartupPath.empty()) {
      gFo3Title.Notice("Could not allocate a save file.");
      return;
    }
    gFo3Title.open = false;
    gSceneLoad.bootAttempted = false;
    gFo3Title.neutral = false;
    BeginFo3Loading(0, 0);
  }
}
} // namespace
