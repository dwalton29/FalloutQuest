#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace fo3title {
inline constexpr const char *SaveRoot =
    "/data/user/0/com.falloutquest.app/files";
struct Save {
  std::string path, label;
  int64_t modified = 0;
};
// Keep the original single save discoverable. New games get independent files;
// starting one never renames, truncates or deletes an existing playthrough.
inline std::vector<Save> Saves(const std::string &root) {
  std::vector<Save> result;
  DIR *dir = opendir(root.c_str());
  if (!dir)
    return result;
  while (auto *entry = readdir(dir)) {
    std::string name = entry->d_name;
    if (name != "player-state.fqps" &&
        !(name.rfind("playthrough-", 0) == 0 && name.size() > 5 &&
          name.substr(name.size() - 5) == ".fqps"))
      continue;
    const auto path = root + "/" + name;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
      continue;
    char date[64]{};
    std::tm tm{};
    localtime_r(&st.st_mtime, &tm);
    std::strftime(date, sizeof(date), "%d %b %Y %H:%M", &tm);
    result.push_back(
        {path,
         std::string(name == "player-state.fqps" ? "Current game - "
                                                 : "Game - ") +
             date,
         int64_t(st.st_mtime)});
  }
  closedir(dir);
  std::sort(result.begin(), result.end(), [](const Save &a, const Save &b) {
    return a.modified != b.modified ? a.modified > b.modified : a.path > b.path;
  });
  return result;
}
inline std::string NewPath(const std::string &root, uint64_t stamp) {
  for (unsigned i = 0; i < 10000; ++i) {
    auto p = root + "/playthrough-" + std::to_string(stamp) + "-" +
             std::to_string(i) + ".fqps";
    struct stat s{};
    if (stat(p.c_str(), &s) != 0)
      return p;
  }
  return {};
}
struct Model {
  enum class Page {
    Main,
    Load,
    Settings,
    Audio,
    Controls,
    ConfirmNew,
    ConfirmQuit,
    Notice,
    Credits
  };
  enum class Action { None, Continue, NewGame, Quit, Recenter, Volume };
  Page page = Page::Main;
  size_t selected = 0, creditsPage = 0;
  uint64_t revision = 0;
  std::vector<Save> saves;
  std::vector<std::string> credits;
  std::array<float, 4> volume{1, .3f, 1, 1};
  std::string notice, loadPath;
  bool open = true, neutral = false, aDown = false, bDown = false;
  int axisY = 0, axisX = 0;
  double repeatY = 0, repeatX = 0;
  size_t Count() const {
    switch (page) {
    case Page::Main:
      return 6;
    case Page::Load:
      return saves.size();
    case Page::Settings:
      return 3;
    case Page::Audio:
      return 5;
    case Page::Controls:
    case Page::Notice:
      return 1;
    case Page::Credits:
      return 3;
    default:
      return 2;
    }
  }
  void Go(Page next) {
    page = next;
    selected = 0;
    ++revision;
  }
  void Notice(std::string text) {
    notice = std::move(text);
    Go(Page::Notice);
  }
  void Move(int d) {
    const auto n = Count();
    if (n) {
      selected = (selected + n + d) % n;
      ++revision;
    }
  }
  void Back() {
    Go(page == Page::Audio || page == Page::Controls ? Page::Settings
                                                     : Page::Main);
  }
  Action Adjust(int d) {
    if (page != Page::Audio || selected >= 4)
      return Action::None;
    volume[selected] = std::clamp(volume[selected] + d * .05f, 0.f, 1.f);
    ++revision;
    return Action::Volume;
  }
  Action Confirm() {
    switch (page) {
    case Page::Main:
      switch (selected) {
      case 0:
        if (saves.empty()) {
          Notice("No saved games are available.");
          break;
        }
        loadPath = saves.front().path;
        return Action::Continue;
      case 1:
        Go(Page::ConfirmNew);
        break;
      case 2:
        if (saves.empty())
          Notice("No saved games are available.");
        else
          Go(Page::Load);
        break;
      case 3:
        Go(Page::Settings);
        break;
      case 4:
        if (credits.empty())
          Notice("Install the original Data/Credits.txt to view the credits.");
        else
          Go(Page::Credits);
        break;
      case 5:
        Go(Page::ConfirmQuit);
        selected = 1;
        break;
      }
      break;
    case Page::Load:
      if (selected < saves.size()) {
        loadPath = saves[selected].path;
        return Action::Continue;
      }
      break;
    case Page::Settings:
      if (selected == 0)
        Go(Page::Audio);
      else if (selected == 1)
        Go(Page::Controls);
      else
        return Action::Recenter;
      break;
    case Page::Audio:
      if (selected == 4) {
        volume = {1, .3f, 1, 1};
        ++revision;
        return Action::Volume;
      }
      break;
    case Page::ConfirmNew:
      if (selected == 0)
        return Action::NewGame;
      Go(Page::Main);
      break;
    case Page::ConfirmQuit:
      if (selected == 0)
        return Action::Quit;
      Go(Page::Main);
      break;
    case Page::Credits:
      if (selected == 2)
        Go(Page::Main);
      else {
        if (selected == 0 && creditsPage)
          --creditsPage;
        if (selected == 1 && (creditsPage + 1) * 12 < credits.size())
          ++creditsPage;
        ++revision;
      }
      break;
    default:
      Back();
      break;
    }
    return Action::None;
  }
  // Focus loss and first opening require a real neutral sample. Edges cannot
  // leak into gameplay or confirm twice while a button is held.
  template <class FocusSound>
  Action Input(bool a, bool b, float x, float y, double now, bool focused,
               FocusSound sound) {
    bool ae = a && !aDown, be = b && !bDown;
    aDown = a;
    bDown = b;
    if (!focused) {
      neutral = false;
      axisY = axisX = 0;
      return Action::None;
    }
    if (!neutral) {
      if (!a && !b && std::abs(x) < .25f && std::abs(y) < .25f)
        neutral = true;
      return Action::None;
    }
    auto step = [&](float value, int &old, double &repeat) {
      int d = value > .65f ? 1 : value < -.65f ? -1 : 0;
      if (!d) {
        old = 0;
        return 0;
      }
      if (old != d || now >= repeat) {
        repeat = now + (old != d ? .35 : .12);
        old = d;
        return d;
      }
      return 0;
    };
    if (int d = step(y, axisY, repeatY)) {
      Move(-d);
      sound("UIMenuFocus");
    }
    Action action = Action::None;
    if (int d = step(x, axisX, repeatX)) {
      action = Adjust(d);
      if (action != Action::None)
        sound("UIMenuPrevNext");
    }
    if (be) {
      Back();
      sound("UIMenuCancel");
    } else if (ae) {
      action = Confirm();
      sound("UIMenuOK");
    }
    return action;
  }
};
inline void ReadVolumes(Model &m, const std::string &root) {
  if (FILE *f = std::fopen((root + "/menu-audio.cfg").c_str(), "r")) {
    std::array<float, 4> v{};
    bool ok = std::fscanf(f, "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]) == 4;
    std::fclose(f);
    if (ok && std::all_of(v.begin(), v.end(), [](float n) {
          return std::isfinite(n) && n >= 0 && n <= 1;
        }))
      m.volume = v;
  }
}
inline bool WriteVolumes(const Model &m, const std::string &root) {
  auto path = root + "/menu-audio.cfg", tmp = path + ".tmp";
  FILE *f = std::fopen(tmp.c_str(), "w");
  if (!f)
    return false;
  bool ok = std::fprintf(f, "%.3f %.3f %.3f %.3f\n", m.volume[0], m.volume[1],
                         m.volume[2], m.volume[3]) > 0;
  if (std::fclose(f) != 0)
    ok = false;
  if (ok)
    ok = std::rename(tmp.c_str(), path.c_str()) == 0;
  if (!ok)
    std::remove(tmp.c_str());
  return ok;
}
} // namespace fo3title
