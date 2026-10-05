#pragma once
// Included after canonical Text/Quad/Draw primitives. Authored definitions and
// mutable state live in their own modules; this file only presents them.
namespace fo3pipui {
inline void Icon(const std::string &path, float x, float y, float size,
                 float angle = 0) {
  GLuint art = Art(path);
  fo3hudassets::TaiSprite sprite;
  bool atlas = false;
  if (!art) {
    auto slash = path.find_last_of("/\\");
    auto alias = path.substr(slash == std::string::npos ? 0 : slash + 1);
    static std::unordered_map<std::string, fo3hudassets::TaiSprite> cached;
    auto found = cached.find(alias);
    if (found == cached.end()) {
      fo3hudassets::TaiSprite s;
      fo3hudassets::ParseButtonTai(s, alias.c_str());
      found = cached.emplace(alias, s).first;
    }
    sprite = found->second;
    if (sprite.w > 0) {
      art = Art(sprite.atlasPath);
      atlas = true;
    }
  }
  if (!art)
    return;
  float u0 = atlas ? sprite.u : 0, v0 = atlas ? sprite.v : 0,
        u1 = atlas ? sprite.u + sprite.w : 1,
        v1 = atlas ? sprite.v + sprite.h : 1;
  auto &vertices = State().vertices;
  size_t first = vertices.size();
  Quad(x - size / 2, y - size / 2, size, size, u0, v0, u1, v1);
  if (angle != 0)
    for (size_t i = first; i < vertices.size(); ++i) {
      auto &v = vertices[i];
      float px = v.x - x, py = v.y - y;
      v.x = x + px * std::cos(angle) - py * std::sin(angle);
      v.y = y + px * std::sin(angle) + py * std::cos(angle);
    }
  Draw(art);
}
inline std::vector<std::string> Wrap(std::string text, float width) {
  // NOTE text is CP1252, the same single-byte authored FNT encoding.
  std::vector<std::string> lines;
  size_t at = 0;
  while (at < text.size()) {
    if (text[at] == '\r') {
      ++at;
      continue;
    }
    if (text[at] == '\n') {
      lines.push_back("");
      ++at;
      continue;
    }
    auto end = text.find_first_of("\r\n", at);
    if (end == std::string::npos)
      end = text.size();
    auto part = std::string_view(text).substr(at, end - at);
    size_t n = fo3font::FitText(State().fonts[0], part, width);
    if (!n)
      n = 1;
    if (n < part.size()) {
      auto space = part.substr(0, n).find_last_of(" \t");
      if (space != std::string_view::npos && space)
        n = space;
    }
    lines.emplace_back(part.substr(0, n));
    at += n;
    while (at < end && (text[at] == ' ' || text[at] == '\t'))
      ++at;
    if (at == end) {
      if (at < text.size() && text[at] == '\r')
        ++at;
      if (at < text.size() && text[at] == '\n')
        ++at;
    }
  }
  return lines;
}
inline void Paragraph(fo3pip::Menu &menu, const std::string &text, float x = 90,
                      float y = 185, float width = 810) {
  static std::string previous;
  static std::vector<std::string> lines;
  static float priorWidth = 0;
  if (previous != text || priorWidth != width) {
    previous = text;
    priorWidth = width;
    lines = Wrap(text, width);
  }
  const size_t visible =
      std::max(size_t(1), size_t(std::max(50.f, 625.f - y) / 50.f));
  menu.textScroll = std::min(
      menu.textScroll, lines.size() > visible ? lines.size() - visible : 0);
  for (size_t i = menu.textScroll;
       i < std::min(lines.size(), menu.textScroll + visible); ++i)
    Text(lines[i], x, y + (i - menu.textScroll) * 50, 0, width);
  Draw(State().fontTexture[0]);
}
inline void ExtendedStats(fo3pip::Menu &menu, const fo3player::Player &p) {
  auto &r = State();
  auto &s = p.Snapshot().pipboy;
  auto &d = p.Definitions().pipboy;
  if (menu.page == 3) {
    size_t first = menu.selected >= 7 ? menu.selected - 6 : 0;
    for (size_t i = first; i < std::min(menu.rows.size(), first + 7); ++i) {
      auto id = uint32_t(menu.rows[i]);
      auto &perk = d.perks.at(id);
      Text((i == menu.selected ? "> " : "") + perk.name +
               (s.perks.at(id) > 1 ? " (" + std::to_string(s.perks.at(id)) + ")"
                                   : ""),
           90, 145 + (i - first) * 51, 0, 390);
    }
    Draw(r.fontTexture[0]);
    if (menu.selected < menu.rows.size()) {
      auto &perk = d.perks.at(menu.rows[menu.selected]);
      if (!perk.icon.empty())
        Icon(perk.icon, 730, 260, 200);
      Paragraph(menu, perk.description, 530, 395, 390);
    }
  } else { // General is an engine-supplied list in stats_menu.xml. Only
           // observed counters are presented. English label verified in
           // Fallout3.exe.
    Text("Locations Discovered", 120, 180);
    Text(std::to_string(s.discovered.size()), 680, 180);
    Draw(r.fontTexture[0]);
  }
}
inline const char *MarkerIcon(uint8_t type) {
  static const char *icons[] = {"icon_map_undiscovered.dds",
                                "icon_map_city.dds",
                                "icon_map_settlement.dds",
                                "icon_map_encampment.dds",
                                "icon_map_natural_landmark.dds",
                                "icon_map_cave.dds",
                                "icon_map_factory.dds",
                                "icon_map_monument.dds",
                                "icon_map_military.dds",
                                "icon_map_office.dds",
                                "icon_map_ruins_town.dds",
                                "icon_map_ruins_urban.dds",
                                "icon_map_ruins_sewer.dds",
                                "icon_map_metro.dds",
                                "icon_map_vault.dds"};
  return icons[std::min(size_t(type), size_t(14))];
}
inline void MapPage(fo3pip::Menu &menu, const fo3player::Player &p) {
  auto &r = State();
  auto &d = p.Definitions().pipboy;
  auto &s = p.Snapshot().pipboy;
  auto &map = menu.map;
  constexpr float left = 75, top = 135, w = 870, h = 445;
  float aspect = 1;
  if (menu.page == 0 && map.local)
    aspect = (map.local->maxX - map.local->minX) /
             (map.local->maxY - map.local->minY);
  else if (menu.page == 1 && d.worlds.count(map.mapWorld)) {
    auto &world = d.worlds.at(map.mapWorld);
    aspect = float(world.width) / world.height;
  }
  aspect = std::max(aspect, .01f);
  auto screen = [&](fo3pipdata::Point point) {
    return fo3pipdata::Point{
        left + w * (.5f + (point.x - map.centre.x) * map.zoom),
        top + h * .5f + (point.y - map.centre.y) * w * map.zoom / aspect};
  };
  auto inside = [&](fo3pipdata::Point v) {
    return v.x >= left + 18 && v.x < left + w - 18 && v.y >= top + 18 &&
           v.y < top + h - 18;
  };
  GLuint texture = 0;
  if (menu.page == 1) {
    auto world = d.worlds.find(map.mapWorld);
    if (world != d.worlds.end())
      texture = Art(world->second.image);
  } else if (map.local && !map.local->rgba.empty()) {
    if (r.localMap != map.local) {
      if (r.localTexture)
        glDeleteTextures(1, &r.localTexture);
      r.localTexture =
          fo3hudassets::UploadTexture(256, 256, map.local->rgba.data());
      r.localMap = map.local;
    }
    texture = r.localTexture;
  }
  if (texture) {
    float u0 = map.centre.x - .5f / map.zoom,
          v0 = map.centre.y - .5f * h / w * aspect / map.zoom,
          u1 = map.centre.x + .5f / map.zoom,
          v1 = map.centre.y + .5f * h / w * aspect / map.zoom;
    float clippedU0 = std::max(0.f, u0), clippedV0 = std::max(0.f, v0),
          clippedU1 = std::min(1.f, u1), clippedV1 = std::min(1.f, v1);
    Quad(left + (clippedU0 - u0) * w * map.zoom,
         top + (clippedV0 - v0) * w * map.zoom / aspect,
         (clippedU1 - clippedU0) * w * map.zoom,
         (clippedV1 - clippedV0) * w * map.zoom / aspect, clippedU0, clippedV0,
         clippedU1, clippedV1);
    Draw(texture);
  }
  auto player = screen(map.player);
  if (inside(player))
    Icon("Interface\\Icons\\Misc\\glow_cursor.dds", player.x, player.y, 28,
         map.location.facing);
  menu.selectedMarker = 0;
  if (menu.page == 1 && map.mapWorld) {
    auto &world = d.worlds.at(map.mapWorld);
    float closest = .04f;
    std::string selected;
    for (auto &m : d.markers) {
      if (fo3pipdata::MapWorld(d, m.second.world) != map.mapWorld ||
          !fo3pipdata::Enabled(d, m.second) ||
          (!(m.second.mapFlags & 1) && !s.discovered.count(m.first)))
        continue;
      auto point =
          fo3pipdata::MapProject(d, m.second.world, m.second.x, m.second.y);
      auto pos = screen(point);
      if (inside(pos))
        Icon(std::string("Interface\\Icons\\World Map\\") +
                 MarkerIcon(s.discovered.count(m.first) ? m.second.type : 0),
             pos.x, pos.y, 30);
      float dist = std::hypot(point.x - map.centre.x, point.y - map.centre.y);
      if (dist < closest) {
        closest = dist;
        selected = m.second.name;
        menu.selectedMarker = m.first;
      }
    }
    if (s.waypoint.world == map.mapWorld) {
      auto pos = screen(world.Project(s.waypoint.point.x, s.waypoint.point.y));
      if (inside(pos))
        Icon("Interface\\Worldmap\\waypoint_node.dds", pos.x, pos.y, 30);
    }
    if (!selected.empty())
      Text(selected, 90, 590, 0, 810);
    for (auto target : fo3pipdata::QuestTargets(d, s)) {
      auto &t = d.targets.at(target);
      if (fo3pipdata::MapWorld(d, t.world) != map.mapWorld)
        continue;
      auto pos = screen(fo3pipdata::MapProject(d, t.world, t.x, t.y));
      if (inside(pos))
        Icon("Interface\\HUD\\glow_hud_compass_objective_marker.dds", pos.x,
             pos.y, 30);
    }
  }
  if (menu.page == 0 && map.local) {
    auto doors = d.doors.find(map.location.cell);
    if (doors != d.doors.end())
      for (auto &door : doors->second) {
        if (!fo3pipdata::Enabled(d, door))
          continue;
        auto point = map.local->Project((door.x - map.localOrigin.x) / 70.f,
                                        (door.y - map.localOrigin.y) / 70.f -
                                            map.sceneForward);
        auto pos = screen(point);
        if (inside(pos))
          Icon("Interface\\Icons\\Local Map\\icon_local_door.dds", pos.x, pos.y,
               24);
      }
  }
  if (menu.MapInteraction())
    Icon("Interface\\Shared\\Marker\\glow_square_small.dds", left + w * .5f,
         top + h * .5f, 25);
  Draw(r.fontTexture[0]);
}
inline void DataPage(fo3pip::Menu &menu, const fo3player::Player &p) {
  auto &r = State();
  auto &d = p.Definitions().pipboy;
  auto &s = p.Snapshot().pipboy;
  if (menu.page < 2) {
    MapPage(menu, p);
    return;
  }
  if (menu.page == 3 && menu.inPage && menu.selected < menu.rows.size()) {
    auto id = uint32_t(menu.rows[menu.selected]);
    auto &n = d.notes.at(id);
    Text(n.name, 90, 135, 0, 810);
    Draw(r.fontTexture[0]);
    if (n.type == 1)
      Paragraph(menu, n.text);
    else if (n.type == 2 && !n.image.empty())
      Icon(n.image, 500, 370, 390);
    else
      Text(menu.playingNote == id ? "Stop Audio" : "Play Audio", 90, 210);
    Draw(r.fontTexture[0]);
    return;
  }
  if (menu.page == 2 && menu.inPage && menu.selected < menu.rows.size()) {
    auto id = uint32_t(menu.rows[menu.selected]);
    auto &q = d.quests.at(id);
    auto &state = s.quests.at(id);
    Text(q.name, 90, 135, 0, 810);
    Draw(r.fontTexture[0]);
    std::vector<uint32_t> indices;
    for (auto &o : state.objectives)
      if (o.second.displayed)
        indices.push_back(o.first);
    std::sort(indices.begin(), indices.end());
    std::string text;
    for (auto index : indices) {
      auto &obj = state.objectives.at(index);
      text += (obj.status == fo3pipdata::Completion::Complete ? "[+] "
               : obj.status == fo3pipdata::Completion::Failed ? "[-] "
                                                              : "") +
              q.objectives.at(index).text + "\n\n";
    }
    Paragraph(menu, text);
    return;
  }
  size_t first = menu.selected >= 8 ? menu.selected - 7 : 0;
  for (size_t i = first; i < std::min(menu.rows.size(), first + 8); ++i) {
    auto id = uint32_t(menu.rows[i]);
    std::string name;
    if (menu.page == 2) {
      name = d.quests.at(id).name;
      auto status = s.quests.at(id).status;
      if (status == fo3pipdata::Completion::Complete)
        name = "[+] " + name;
      else if (status == fo3pipdata::Completion::Failed)
        name = "[-] " + name;
      else if (s.selectedQuest == id)
        name = "> " + name;
    }
    if (menu.page == 3)
      name = d.notes.at(id).name;
    if (menu.page == 4) {
      name = d.stations.at(d.transmitters.at(id).base).name;
      if (s.tunedRadio == id)
        name = "[+] " + name;
    }
    if (i == menu.selected) {
      Draw(r.fontTexture[0]);
      Quad(75, 141 + (i - first) * 51, 850, 2);
      Draw(r.white);
    }
    Text(name, 90, 145 + (i - first) * 51, 0, 810);
  }
  Draw(r.fontTexture[0]);
}
} // namespace fo3pipui
