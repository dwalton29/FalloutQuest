#pragma once
#include "data/fo3-bsa-archive.h"
#include <map>
#include <string>
namespace fo3menu {
// Some original UI filenames are atlas aliases, not complete DDS paths. When
// the user's atlas index is empty, resolve only unique original Interface DDS
// basenames. Never select an arbitrary duplicate or synthesize replacement art.
class TexturePaths {
  std::map<std::string, std::string> paths_;

public:
  void Add(const std::string &source) {
    auto path = fo3assets::NormalizeArchivePath(source);
    auto relative = path.rfind("textures\\", 0) == 0 ? path.substr(9) : path;
    if (relative.rfind("interface\\", 0) != 0 || relative.size() < 4 ||
        relative.substr(relative.size() - 4) != ".dds")
      return;
    path = "textures\\" + relative;
    auto name = relative.substr(relative.find_last_of('\\') + 1);
    auto old = paths_.find(name);
    if (old == paths_.end())
      paths_[name] = path;
    else if (old->second != path)
      old->second.clear();
  }
  std::string Resolve(const std::string &source) const {
    auto name = fo3assets::NormalizeArchivePath(source);
    if (name.find('\\') != name.npos)
      return {};
    auto found = paths_.find(name);
    return found == paths_.end() ? "" : found->second;
  }
};
} // namespace fo3menu
