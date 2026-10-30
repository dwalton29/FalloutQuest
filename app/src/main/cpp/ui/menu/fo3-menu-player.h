#pragma once
#include "fo3-barter-controls.h"
#include "fo3-menu-renderer.h"
#include "pipboy/fo3-quest-route.h"
#include "ui/pipboy/fo3-pipboy-state.h"
namespace fo3menuui {
struct Document {
  fo3menu::Layout layout;
  Surface surface;
  bool attempted = false, ready = false, warnedIncomplete = false;
  uint64_t revision = UINT64_MAX;
  size_t selection = SIZE_MAX;
  std::string noticeKey;
  bool Ensure(const char *path, float w = 1024, float h = 768) {
    if (attempted)
      return ready;
    attempted = true;
    layout.width = w;
    layout.height = h;
    Measure(layout);
    ready = layout.Load(path, Read);
    if (!ready)
      __android_log_print(ANDROID_LOG_ERROR, "FalloutQuest",
                          "AUTHORED MENU failed path=%s reason=%s", path,
                          layout.Error().c_str());
    return ready;
  }
  void Build() {
    const bool complete = surface.Build(layout);
    if ((!complete || !layout.Unsupported().empty()) && !warnedIncomplete) {
      warnedIncomplete = true;
      __android_log_print(ANDROID_LOG_WARN, "FalloutQuest",
                          "AUTHORED MENU incomplete assets=%d unsupported=%zu",
                          !complete, layout.Unsupported().size());
    }
  }
  void Begin(const fo3player::Player &p) {
    layout.Reset();
    Strings(layout, p);
    layout.Set(layout.Globals(), "_Has360Controller", 1.);
    // VR readability: retain the authored backdrop geometry and black art,
    // increasing its opacity for a world-space panel over bright terrain.
    if (layout.width > 1024)
      layout.Set(layout.Globals(), "_background_fill_alpha", 160.);
  }
};
inline Document &PipDocument(int tab) {
  static Document docs[3];
  return docs[tab];
}
inline Document &DialogueDocument() {
  static Document d;
  return d;
}
inline Document &BarterDocument() {
  static Document d;
  return d;
}
inline std::string Number(double n) {
  char text[48];
  std::snprintf(text, sizeof(text), "%.0f", n);
  return text;
}
inline std::string Decimal(double n) {
  char text[48];
  std::snprintf(text, sizeof(text), "%.1f", n);
  std::string value = text;
  if (value.size() > 2 && value.substr(value.size() - 2) == ".0")
    value.resize(value.size() - 2);
  return value;
}
inline std::string Label(const fo3player::Player &p, const std::string &key) {
  return Setting(p, key);
}
inline Document &QuantityDocument() {
  static Document d;
  return d;
}
inline Document &MessageDocument() {
  static Document d;
  return d;
}
inline void BindMessage(Document &doc, const fo3player::Player &p,
                        const std::string &title, const std::string &text,
                        const std::vector<std::string> &choices,
                        size_t selected) {
  doc.Begin(p);
  auto &l = doc.layout;
  l.Set("MM_Title", "string", title);
  l.Set("MM_Title", "visible", double(!title.empty()));
  l.Set("MM_MessageText", "string", text);
  const auto min = l.N(l.Root(), "_MinMenuWidth"),
             max = l.N(l.Root(), "_MaxMenuWidth"),
             buffer = l.N(l.Root(), "_horbuf");
  float width = l.measure(2, text, max - buffer * 2).w + buffer * 2;
  width = std::max(width, l.measure(6, title, max - buffer * 2).w + buffer * 2);
  for (const auto &choice : choices)
    width =
        std::max(width, l.measure(2, choice, max - buffer * 2).w + buffer * 2);
  l.Set("MM_MainRect", "width", std::clamp(width, min, max));
  l.Set("MM_ButtonList", "width", std::clamp(width, min, max) - buffer * 2);
  std::vector<Row> rows;
  for (const auto &choice : choices)
    rows.push_back({choice, "", false, false});
  auto ids = List(l, "MM_ButtonList", "MM_ButtonTemplate", rows, selected);
  float height = 0;
  for (auto row : ids)
    height += l.N(row, "height");
  l.Set("MM_ButtonList", "height", std::min(height, 420.f));
  ListScrollFix(l, l.Find("MM_ButtonList"), selected);
  doc.Build();
}
inline uint64_t TextKey(const std::string &text,
                        uint64_t h = 1469598103934665603ull) {
  for (unsigned char c : text)
    h = (h ^ c) * 1099511628211ull;
  return (h ^ 255u) * 1099511628211ull;
}
inline uint64_t PanelKey(const fo3dialogue::Panel &panel) {
  auto key = TextKey(panel.subtitle, TextKey(panel.title));
  for (const auto &choice : panel.choices)
    key = TextKey(choice, key);
  return key ^ panel.revision;
}
inline void Message(const float *mvp, const fo3dialogue::Panel &panel,
                    const fo3player::Player &p) {
  auto &doc = MessageDocument();
  if (!doc.Ensure("menus\\message_menu.xml", 1280, 1024))
    return;
  const auto key = PanelKey(panel);
  if (doc.revision != key || doc.selection != panel.selected) {
    doc.layout.width = 1280;
    doc.layout.height = 1024;
    BindMessage(doc, p, panel.title, panel.subtitle, panel.choices,
                panel.selected);
    doc.revision = key;
    doc.selection = panel.selected;
  }
  World(doc.surface, doc.layout, mvp);
}
inline void Notice(const float *mvp, const std::string &text,
                   const fo3player::Player &p, bool world,
                   const std::vector<std::string> &choices = {}) {
  if (text.empty())
    return;
  auto &doc = MessageDocument();
  if (!doc.Ensure("menus\\message_menu.xml"))
    return;
  const float width = world ? 1280 : 1024, height = world ? 1024 : 768;
  std::string key = text;
  for (const auto &choice : choices)
    key += '\n' + choice;
  if (doc.revision != UINT64_MAX || doc.noticeKey != key ||
      doc.layout.width != width || doc.layout.height != height) {
    doc.layout.width = width;
    doc.layout.height = height;
    BindMessage(doc, p, "", text, choices, choices.empty() ? SIZE_MAX : 0);
    doc.revision = UINT64_MAX;
    doc.noticeKey = key;
  }
  if (world)
    World(doc.surface, doc.layout, mvp);
  else
    doc.surface.Render(mvp);
}
inline void Card(fo3menu::Layout &l, int parent, const char *name,
                 const std::string &value, bool visible = true) {
  auto id = l.Find(name, parent);
  l.Set(id, "_Value", value);
  l.Set(id, "visible", double(visible));
}
inline void ItemCards(fo3menu::Layout &l, int parent,
                      const fo3player::Player &p,
                      const fo3player::Stack *stack) {
  for (const auto *name :
       {"WeightInfo", "ValueInfo", "ConditionInfo", "DPSInfo",
        "DamageResistInfo", "AmmoInfo", "EffectsInfo"})
    l.Set(l.Find(name, parent), "visible", 0.);
  if (!stack)
    return;
  auto found = p.Definitions().items.find(stack->formId);
  if (found == p.Definitions().items.end())
    return;
  const auto &item = found->second;
  Card(l, parent, "WeightInfo", Decimal(item.weight));
  Card(l, parent, "ValueInfo", Number(item.value), item.valueKnown);
  Card(l, parent, "ConditionInfo", "", item.maxCondition > 0);
  l.Set(l.Find("ConditionInfo", parent), "user5", stack->condition);
  auto armour = p.Definitions().weapons.armourDR.find(item.formId);
  if (armour != p.Definitions().weapons.armourDR.end())
    Card(l, parent, "DamageResistInfo", Number(armour->second));
  if (item.kind == fo3player::ItemKind::Weapon)
    Card(l, parent, "DPSInfo", Number(p.WeaponDamage(*stack)));
  if (item.weapon.Firearm()) {
    const auto ammo = p.Definitions().items.find(item.weapon.ammo);
    if (ammo != p.Definitions().items.end()) {
      l.Set(l.Find("AmmoInfo", parent), "_Title",
            ammo->second.shortName.empty() ? ammo->second.name
                                           : ammo->second.shortName);
      Card(l, parent, "AmmoInfo",
           Number(stack->loadedRounds) + "/" +
               Number(p.AmmoReserve(item.weapon.ammo)));
    }
  }
}
inline std::vector<Row> InventoryRows(const fo3player::Player &p,
                                      const std::vector<uint64_t> &ids) {
  std::vector<Row> rows;
  for (auto id : ids) {
    const auto *s = fo3pip::Menu::Stack(p, id);
    if (!s)
      continue;
    const auto &item = p.Definitions().items.at(s->formId);
    rows.push_back(
        {item.name +
             (s->count > 1 ? " (" + std::to_string(s->count) + ")" : ""),
         "", s->equipped, false});
  }
  return rows;
}
inline std::string TimeDate(const fo3player::Player &p) {
  const auto &s = p.Snapshot();
  const int hour = int(s.gameHour), minute = int((s.gameHour - hour) * 60);
  char text[64];
  // Original date formatter 009A70F2..009A7105, MapMenu 0094F698..0094F6AE.
  std::snprintf(text, sizeof(text), "%02d.%02d.%02d, %d:%02d",
                int(s.gameMonth + 1), int(s.gameDay), int(s.gameYear % 100),
                hour, minute);
  return text;
}
inline const char *MapIcon(uint8_t type) {
  static const char *names[]{
      "undiscovered",     "city",   "settlement", "encampment",
      "natural_landmark", "cave",   "factory",    "monument",
      "military",         "office", "ruins_town", "ruins_urban",
      "ruins_sewer",      "metro",  "vault"};
  return names[std::min(size_t(type), size_t(14))];
}
inline void Map(fo3menu::Layout &l, fo3pip::Menu &menu,
                const fo3player::Player &p, GLuint localTexture) {
  auto &map = menu.map;
  const auto &d = p.Definitions().pipboy;
  const bool world = menu.page == 1;
  const std::string prefix = world ? "MM_WorldMap" : "MM_LocalMap";
  int parent = l.Find(prefix + "_ParentImage");
  auto viewport = l.Bounds(l.Find(prefix + "_ClipWindow"));
  float width = 256, height = 256;
  std::string image;
  if (world && d.worlds.count(map.mapWorld)) {
    const auto &def = d.worlds.at(map.mapWorld);
    image = def.image;
    width = def.width;
    height = def.height;
  } else if (!world && map.local && localTexture) {
    image = "__local_map";
    Shared().dynamicImages[image] = localTexture;
    const auto aspect = (map.local->maxX - map.local->minX) /
                        (map.local->maxY - map.local->minY);
    height = width / std::max(.01f, aspect);
  }
  l.Set(parent, "visible", double(!image.empty()));
  l.Set(parent, "filename", image);
  l.Set(parent, "filewidth", width);
  l.Set(parent, "fileheight", height);
  // VR map zoom/pan policy supplies native values. The viewport, marker art,
  // selected enlargement, marker offsets and clamping come from original XML.
  const float scale = viewport.w * map.zoom / std::max(1.f, width);
  l.Set(parent, "_Magnification", scale);
  l.Set(parent, "x", viewport.w * .5f - map.centre.x * width * scale);
  l.Set(parent, "y", viewport.h * .5f - map.centre.y * height * scale);
  l.Set(parent, "user0", 30.);
  l.Set(parent, "user1", 30.);
  l.Set(parent, "user2", 28.);
  auto marker = [&](const std::string &type, fo3pipdata::Point point,
                    const std::string &path, bool selected = false) {
    if (type != "LocalMapQuestMarkerTemplate") {
      const float x = viewport.w * .5f +
                      (point.x - map.centre.x) * width * scale,
                  y = viewport.h * .5f +
                      (point.y - map.centre.y) * height * scale;
      if (x < -64 || y < -64 || x > viewport.w + 64 || y > viewport.h + 64)
        return -1;
    }
    int node = l.Instantiate(type, parent);
    l.Set(node, "_x", point.x);
    l.Set(node, "_y", point.y);
    l.Set(node, "filename", path);
    l.Set(node, "_selected", double(selected));
    return node;
  };
  if (world) {
    for (const auto &entry : d.markers) {
      const auto &m = entry.second;
      if (fo3pipdata::MapWorld(d, m.world) != map.mapWorld ||
          !p.MapMarkerEnabled(entry.first) ||
          !fo3pipdata::MarkerVisible(m, entry.first, p.Snapshot().pipboy))
        continue;
      auto point = fo3pipdata::MapProject(d, m.world, m.x, m.y);
      auto path =
          std::string("Interface\\Icons\\World Map\\icon_map_") +
          MapIcon(fo3pipdata::MarkerKnown(m, entry.first, p.Snapshot().pipboy)
                      ? m.type
                      : 0) +
          ".dds";
      marker("MapMarkerTemplate", point, path,
             menu.selectedMarker == entry.first);
    }
    if (map.mapWorld && d.worlds.count(map.mapWorld) &&
        p.Snapshot().pipboy.waypoint.world == map.mapWorld) {
      const auto &w = p.Snapshot().pipboy.waypoint;
      marker("WaypointTemplate",
             d.worlds.at(map.mapWorld).Project(w.point.x, w.point.y),
             "Interface\\Worldmap\\waypoint_node.dds");
    }
    for (auto reference : fo3pipdata::NavigationTargets(p)) {
      fo3pipdata::Location target;
      if (!p.ScriptReferenceLocation(reference, target))
        continue;
      if (!target.world)
        target =
            fo3pipdata::ExteriorEntrance(d, target.cell, target.x, target.y);
      if (fo3pipdata::MapWorld(d, target.world) == map.mapWorld)
        marker("WorldMapQuestMarkerTemplate",
               fo3pipdata::MapProject(d, target.world, target.x, target.y),
               "Interface\\HUD\\glow_hud_compass_objective_marker.dds");
    }
  } else if (map.local) {
    auto project = [&](float x, float y) {
      return map.local->Project((x - map.localOrigin.x) / 70.f,
                                (y - map.localOrigin.y) / 70.f -
                                    map.sceneForward);
    };
    auto doors = d.doors.find(map.location.cell);
    if (doors != d.doors.end())
      for (const auto &door : doors->second) {
        if (fo3pipdata::Enabled(d, door))
          marker("MapMarkerTemplate", project(door.x, door.y),
                 "Interface\\Icons\\Local Map\\icon_local_door.dds");
      }
    static thread_local fo3questroute::Navigator route;
    std::unordered_map<uint32_t, bool> flags;
    for (auto reference : fo3pipdata::NavigationTargets(p, &flags)) {
      auto target = route.Toward(p, map.location, reference, flags[reference]);
      if (target.valid)
        marker("LocalMapQuestMarkerTemplate",
               project(target.point.x, target.point.y),
               "Interface\\HUD\\glow_hud_compass_objective_marker.dds");
    }
  }
  // Player arrow uses the existing canonical coordinate/facing measurement.
  int cursor = l.Find(prefix + "Cursor");
  l.Set(cursor, "visible", double(map.playerKnown));
  auto pos = l.Bounds(parent);
  auto clip = l.Bounds(l.Find(prefix + "_ClipWindow"));
  l.Set(cursor, "x", pos.x - clip.x + map.player.x * pos.w - 14);
  l.Set(cursor, "y", pos.y - clip.y + map.player.y * pos.h - 14);
  l.Set(cursor, "width", 28.);
  l.Set(cursor, "height", 28.);
  l.Set(cursor, "_rotation", map.location.facing);
  if (menu.selectedMarker && d.markers.count(menu.selectedMarker))
    l.Set("MM_Headline_LocationInfo", "string",
          d.markers.at(menu.selectedMarker).name);
}
inline bool Pipboy(fo3pip::Menu &menu, const fo3player::Player &p,
                   const float *mvp, GLuint localTexture = 0) {
  const char *paths[]{"menus\\main\\stats_menu.xml",
                      "menus\\main\\inventory_menu.xml",
                      "menus\\main\\map_menu.xml"};
  auto &doc = PipDocument(int(menu.tab));
  if (!doc.Ensure(paths[int(menu.tab)]))
    return false;
  const bool animatedRadio = menu.tab == fo3pip::Tab::Data && menu.page == 4;
  uint64_t radioKey = fo3pip::PresentationRevision(p) ^
                      uint64_t(menu.selected << 8) ^
                      uint64_t(menu.radioSignal) ^ 0x524144494full;
  for (auto id : menu.rows)
    radioKey = (radioKey ^ id) * 1099511628211ull;
  if (animatedRadio && doc.revision == radioKey) {
    doc.surface.Animate(menu.radioPhase);
    doc.surface.Render(mvp);
    Notice(mvp, menu.actionError, p, false);
    return true;
  }
  doc.Begin(p);
  auto &l = doc.layout;
  const auto &state = p.Snapshot();
  const auto &d = p.Definitions().pipboy;
  if (menu.tab == fo3pip::Tab::Items) {
    l.Set("IM_Headline_PlayerWGInfo", "_Value",
          Decimal(p.InventoryWeight()) + "/" + Number(p.CarryCapacity()));
    l.Set("IM_Headline_PlayerHPInfo", "_Value",
          Number(p.Health()) + "/" + Number(p.MaxHealth()));
    l.Set("IM_Headline_PlayerDRInfo", "_Value",
          Number(p.DamageResistance(fo3player::PlayerRef)));
    l.Set("IM_Headline_PlayerCapsInfo", "_Value", Number(p.ItemCount(0xf)));
    l.Set(l.Root(), "_EquippableItem", 0.);
    auto rows = InventoryRows(p, menu.rows);
    auto ids = List(l, "IM_InventoryList", "IM_InventoryListTemplate", rows,
                    menu.selected);
    for (size_t i = 0; i < ids.size(); ++i) {
      auto marker = l.Find("IM_Template_ItemMarker", ids[i]);
      l.Set(marker, "visible", double(rows[i].equipped));
    }
    const fo3player::Stack *stack =
        menu.selected < menu.rows.size()
            ? fo3pip::Menu::Stack(p, menu.rows[menu.selected])
            : nullptr;
    ItemCards(l, l.Find("IM_ItemInfoRect"), p, stack);
    if (stack) {
      const auto &item = p.Definitions().items.at(stack->formId);
      l.Set("IM_ItemIcon", "visible", double(!item.icon.empty()));
      l.Set("IM_ItemIcon", "filename", item.icon);
      l.Set("IM_EquipButton", "string",
            Label(p, item.kind == fo3player::ItemKind::Aid ||
                             item.kind == fo3player::ItemKind::Ingredient
                         ? "sInventoryUse"
                     : stack->equipped ? "sInventoryUnequip"
                                       : "sInventoryEquip"));
      l.Set("IM_EquipButton", "target",
            double(fo3pip::Equippable(item) ||
                   item.kind == fo3player::ItemKind::Aid ||
                   item.kind == fo3player::ItemKind::Ingredient));
      l.Set(l.Root(), "_EquippableItem",
            double(fo3pip::Equippable(item) ||
                   item.kind == fo3player::ItemKind::Aid ||
                   item.kind == fo3player::ItemKind::Ingredient));
    }
    Tabs(l, "IM_Tabline",
         {Label(p, "sInventoryWeapons"), Label(p, "sInventoryApparel"),
          Label(p, "sInventoryAid"), Label(p, "sInventoryMisc"),
          Label(p, "sInventoryAmmoTab")},
         menu.page);
    // Repair/drop/hotkey controls are only shown when the gameplay action is
    // connected. The existing VR A action still binds equip/use and B backs
    // out.
    for (const char *name :
         {"IM_RepairButton", "IM_DropButton", "IM_HotkeyButton"})
      l.Set(name, "visible", 0.);
  } else if (menu.tab == fo3pip::Tab::Stats) {
    l.Set(l.Root(), "user0", double(menu.page));
    l.Set("stats_status_container", "user0", double(menu.statusMode));
    l.Set(l.Root(), "user1", state.pipboy.radiation);
    l.Set(l.Root(), "_rads", Number(state.pipboy.radiation));
    double maxRads = 0;
    for (const auto &stage : d.radiationStages)
      maxRads = std::max(maxRads, double(stage.second.threshold));
    l.Set(l.Root(), "user2", maxRads);

    l.Set(l.Root(), "user5", Number(p.Health()) + "/" + Number(p.MaxHealth()));
    l.Set(l.Root(), "user6",
          Number(p.ActionPoints()) + "/" + Number(p.MaxActionPoints()));
    l.Set(l.Root(), "user7",
          Number(state.experience) + "/" +
              Number(p.ExperienceProgress().nextThreshold));
    l.Set(l.Root(), "user8", double(state.level));
    std::vector<Row> rows;
    std::string list;
    std::string description, icon;
    if (menu.page == 1 || menu.page == 2) {
      bool special = menu.page == 1;
      list = special ? "stats_special_container" : "stats_skills_container";
      for (size_t index = 0; index < (special ? 7u : 13u); ++index) {
        size_t av = !special && index == 12 ? 13 : index;
        const auto &source = special ? d.special[av] : d.skills[av];
        rows.push_back({source.name,
                        Number(special ? state.special[av] : state.skills[av]),
                        false, false});
        if (index == menu.selected) {
          description = source.description;
          icon = source.icon;
        }
      }
    } else if (menu.page == 3) {
      list = "stats_perks_container";
      for (size_t i = 0; i < menu.rows.size(); ++i) {
        auto id = uint32_t(menu.rows[i]);
        auto found = d.perks.find(id);
        if (found == d.perks.end())
          continue;
        const auto &perk = found->second;
        rows.push_back(
            {perk.name, Number(state.pipboy.perks.at(id)), false, false});
        if (i == menu.selected) {
          description = perk.description;
          icon = perk.icon;
        }
      }
    } else if (menu.page == 4) {
      list = "stats_general_container";
      rows.push_back({"Locations Discovered",
                      Number(state.pipboy.discovered.size()), false, false});
    }
    if (!list.empty())
      List(l, list, "stats_list_template", rows, menu.selected);
    l.Set("stats_description", "string", description);
    l.Set("stats_icon", "filename", icon);
    l.Set("stats_icon", "visible", double(!icon.empty()));
    l.Set("stats_description_scrollbar", "_current_value",
          double(menu.textScroll));
    // Source EXE binds Interface\\Stats\\%s.dds to these six tiles. The
    // current player has no per-limb damage AVs; omit their numeric meters.
    if (menu.page == 0) {
      const char *tiles[]{"head",     "torso",   "leftarm",
                          "rightarm", "leftleg", "rightleg"};
      const char *files[]{"head",      "torso",    "left_arm",
                          "right_arm", "left_leg", "right_leg"};
      for (int i = 0; i < 6; ++i) {
        std::string name = std::string("stats_player_") + tiles[i];
        l.Set(name, "filename",
              std::string("Interface\\Stats\\") + files[i] + ".dds");
        l.Set(name + "_pct", "visible", 0.);
        l.Set(name + "_crippled", "visible", 0.);
      }
      l.Set("stats_player_face", "visible", 0.);
    }
    for (const auto *name : {"stats_stimpak_button", "stats_healing_mode"})
      l.Set(name, "visible", 0.);
  } else {
    l.Set("MM_Headline_TimeDateInfo", "string", TimeDate(p));
    l.Set("MM_Headline_LocationInfo", "string",
          p.CellFullName(menu.map.location.cell));
    Tabs(l, "MM_Tabline",
         {Label(p, "sLocalMapTabText"), Label(p, "sWorldMapTabText"),
          Label(p, "sQuestsTabText"), Label(p, "sNotesTabText"),
          Label(p, "sCommsTabText")},
         menu.page);
    if (menu.page < 2)
      Map(l, menu, p, localTexture);
    std::vector<Row> rows;
    std::string list = menu.page == 2   ? "MM_QuestsList"
                       : menu.page == 3 ? "MM_NotesList"
                                        : "MM_RadioStationList";
    for (auto value : menu.rows) {
      auto id = uint32_t(value);
      if (menu.page == 2 && d.quests.count(id))
        rows.push_back({d.quests.at(id).name, "", false,
                        state.pipboy.selectedQuest == id});
      else if (menu.page == 3 && d.notes.count(id))
        rows.push_back(
            {d.notes.at(id).name, "", false, menu.playingNote == id});
      else if (menu.page == 4 && d.transmitters.count(id))
        rows.push_back({d.stations.at(d.transmitters.at(id).base).name, "",
                        false, state.pipboy.tunedRadio == id});
    }
    if (menu.page >= 2) {
      auto ids = List(l, list, "MM_ListMarkerTemplate", rows, menu.selected);
      for (size_t i = 0; i < ids.size(); ++i) {
        if (menu.page == 2 &&
            state.pipboy.quests.at(uint32_t(menu.rows[i])).status !=
                fo3pipdata::Completion::Active)
          l.Set(ids[i], "_TextAlpha", 128.);
        l.Set(ids[i], "_selected", double(rows[i].marked));
        l.Set(ids[i], "_ShowEmptyMarker", 1.);
      }
    }
    std::string text, image;
    int type = 0;
    if (menu.selected < menu.rows.size()) {
      auto id = uint32_t(menu.rows[menu.selected]);
      if (menu.page == 2 && d.quests.count(id) &&
          state.pipboy.quests.count(id)) {
        std::vector<Row> objectives;
        std::vector<bool> finished;
        for (auto index : d.quests.at(id).objectiveOrder) {
          auto o = state.pipboy.quests.at(id).objectives.find(index);
          if (o == state.pipboy.quests.at(id).objectives.end() ||
              !o->second.displayed)
            continue;
          const bool done = o->second.status != fo3pipdata::Completion::Active;
          objectives.push_back(
              {d.quests.at(id).objectives.at(index).text, "", false, done});
          finished.push_back(done);
        }
        auto ids = List(l, "MM_QuestObjectivesList", "MM_ListMarkerTemplate",
                        objectives, SIZE_MAX);
        for (size_t i = 0; i < ids.size(); ++i) {
          l.Set(ids[i], "_TextAlpha", finished[i] ? 128. : 255.);
          l.Set(ids[i], "_ShowEmptyMarker", 1.);
          l.Set(ids[i], "_selected", double(finished[i]));
        }
        type = 4;
      } else if (menu.page == 3 && d.notes.count(id)) {
        const auto &note = d.notes.at(id);
        if (note.type == 1) {
          text = note.text;
          type = 1;
        } else if (note.type == 2) {
          image = note.image;
          type = 2;
        } else
          type = 3;
      }
    }
    l.Set("MM_DataText", "string", text);
    l.Set("MM_DataImage", "filename", image);
    l.Set("MM_TextScrollbar", "_current_value", double(menu.textScroll));
    l.Set("MM_DataRect", "visible", double(type != 0));
    l.Set("MM_DataRect", "_ItemType", double(type));
    if (menu.page == 4) {
      l.Set("MM_Waveform", "filename", "Interface\\Radio Wave\\wave.dds");
      l.Set("MM_Waveform", "visible", double(menu.radioSignal));
    }
  }
  doc.Build();
  doc.revision = animatedRadio ? radioKey : UINT64_MAX;
  if (animatedRadio)
    doc.surface.Animate(menu.radioPhase);
  doc.surface.Render(mvp);
  if (menu.travelPrompt && d.markers.count(menu.travelMarker))
    Notice(mvp,
           Label(p, "sTravelQuestion") + "\n" +
               d.markers.at(menu.travelMarker).name + "?",
           p, false, {Label(p, "sYes"), Label(p, "sNo")});
  else
    Notice(mvp, menu.actionError, p, false);
  return true;
}
inline void Conversation(const float *mvp, const fo3dialogue::Panel &panel,
                         const fo3player::Player &p) {
  auto &doc = DialogueDocument();
  if (!doc.Ensure("menus\\dialog\\dialog_menu.xml", 1280, 1024))
    return;
  const auto key = PanelKey(panel);
  if (doc.revision != key || doc.selection != panel.selected) {
    doc.Begin(p);
    auto &l = doc.layout;
    l.Set(l.Root(), "_DialogVisible", 1.);
    l.Set(l.Root(), "_ShowingText", double(panel.choices.empty()));
    l.Set("DM_SpeakerNameLabel", "string", panel.title);
    l.Set("DM_SpeakerText", "string", panel.subtitle);
    std::vector<Row> rows;
    for (const auto &choice : panel.choices)
      rows.push_back({choice, "", false, false});
    List(l, "DM_TopicList", "DM_TopicTemplate", rows, panel.selected);
    float total = 0;
    int list = l.Find("DM_TopicList");
    for (auto c : l.Nodes()[list].children)
      if (l.Nodes()[c].dynamic)
        total += l.N(c, "height");
    l.Set(list, "height",
          std::max(l.N(l.Root(), "_MinListHeight"),
                   std::min(total, l.height * .4f)));
    ListScrollFix(l, list, panel.selected);
    doc.Build();
    doc.revision = key;
    doc.selection = panel.selected;
  }
  World(doc.surface, doc.layout, mvp);
}
inline void Barter(const float *mvp, const fo3player::Player &p,
                   const fo3player::BarterSession &session, bool buying,
                   size_t selected, uint64_t revision,
                   const std::string &status,
                   const fo3barter::Controls &controls = {}) {
  auto &doc = BarterDocument();
  if (!doc.Ensure("menus\\barter_menu.xml", 1280, 1024))
    return;
  // Session counters restart on reopening. Include canonical snapshot content
  // so another merchant (or a restocked inventory) never reuses the previous
  // list.
  uint64_t key = TextKey(status) ^ revision ^ session.actor ^ uint64_t(buying) ^
                 uint64_t(selected << 1);
  key ^= uint64_t(controls.filters[0] + 1) << 40;
  key ^= uint64_t(controls.filters[1] + 1) << 44;
  key = TextKey(Number(session.skill) + ":" + Number(session.discount), key);
  for (const auto *inventory : {&session.player, &session.merchant})
    for (const auto &stack : *inventory)
      key = TextKey(Number(stack.id) + ":" + Number(stack.formId) + ":" +
                        Number(stack.count) + ":" +
                        fo3menu::Value(stack.condition).text + ":" +
                        Number(stack.equipped),
                    key);
  for (const auto &choice : session.selections)
    key = TextKey(Number(choice.stack) + ":" + Number(choice.count) + ":" +
                      Number(choice.buying),
                  key);
  if (doc.revision != key) {
    doc.Begin(p);
    auto &l = doc.layout;
    l.Set(l.Root(), "_IsContainerListSelected", double(buying));
    const auto reference = p.Definitions().references.find(session.actor);
    std::string merchant;
    if (reference != p.Definitions().references.end()) {
      auto npc =
          p.Definitions().pipboy.dialogueActors.find(reference->second.base);
      if (npc != p.Definitions().pipboy.dialogueActors.end())
        merchant = npc->second.name;
    }
    l.Set("BM_ContainerTitle", "string", merchant);
    const char *categories[]{"sInventoryItems",   "sInventoryWeapons",
                             "sInventoryApparel", "sInventoryAid",
                             "sInventoryMisc",    "sInventoryAmmoTab"};
    l.Set("BM_ItemsTitle", "string",
          Label(p, categories[controls.filters[0] + 1]));
    l.Set("BM_ContainerTitle", "string",
          controls.filters[1] < 0
              ? merchant
              : Label(p, categories[controls.filters[1] + 1]));
    l.Set("BM_Items_TitleButton", "filename", "general_button_lt.dds");
    l.Set("BM_Container_TitleButton", "filename", "general_button_rt.dds");
    const fo3player::Stack *focused = nullptr;
    for (bool side : {false, true}) {
      const auto &inventory = side ? session.merchant : session.player;
      std::vector<Row> rows;
      std::vector<const fo3player::Stack *> stacks;
      int64_t caps = 0;
      for (const auto &stack : inventory) {
        if (stack.formId == 0xf)
          caps += stack.count;
        float price = 0;
        if (!fo3barter::InCategory(p.Definitions().items.at(stack.formId),
                                   controls.filters[side]) ||
            !p.BarterItemAllowed(session.actor, stack) ||
            !p.BarterPrice(stack, side, session.discount, price))
          continue;
        int32_t pending = 0;
        for (const auto &choice : session.selections)
          if (choice.buying == side && choice.stack == stack.id)
            pending = choice.count;
        const auto &item = p.Definitions().items.at(stack.formId);
        rows.push_back(
            {item.name +
                 (stack.count > 1 ? " (" + Number(stack.count) + ")" : ""),
             Decimal(price), stack.equipped, pending > 0});
        stacks.push_back(&stack);
      }
      List(l, side ? "BM_Container_InventoryList" : "BM_Items_InventoryList",
           "BM_list_template", rows, side == buying ? selected : SIZE_MAX);
      l.Set(side ? "BM_Container_CapsLabel" : "BM_Items_CapsLabel", "string",
            Number(caps));
      if (side == buying && selected < stacks.size())
        focused = stacks[selected];
    }
    int64_t net = 0;
    std::string reason;
    p.BarterBalance(session, net, reason);
    l.Set("BM_CapsFlow", "_Value", Number(std::abs(net)));
    l.Set(l.Root(), "_ArrowDirection", net < 0 ? 2. : 1.);
    l.Set("BM_CapsFlow", "visible", double(net != 0));
    l.Set("BM_ButtonX", "_box_visible", double(!session.selections.empty()));
    ItemCards(l, l.Find("BM_ItemData"), p, focused);
    if (focused) {
      float price = 0;
      if (p.BarterPrice(*focused, buying, session.discount, price))
        Card(l, l.Find("BM_ItemData"), "ValueInfo", Decimal(price));
    }

    if (focused) {
      const auto &item = p.Definitions().items.at(focused->formId);
      l.Set("BM_ItemIcon", "filename", item.icon);
      l.Set("BM_ItemIcon", "visible", double(!item.icon.empty()));
    }
    // Transaction feedback uses the original amount field. Original A/B footer
    // remains in place and the canonical confirmation/control path is
    // unchanged.
    if (!status.empty())
      l.Set("BM_TransactionAmount", "string", status);
    doc.Build();
    doc.revision = key;
  }
  World(doc.surface, doc.layout, mvp);
  if (controls.Editing()) {
    auto &q = QuantityDocument();
    if (q.Ensure("menus\\quantity_menu.xml", 1280, 1024)) {
      const auto key = uint64_t(controls.quantityStack) ^
                       (uint64_t(controls.quantity) << 32);
      if (q.revision != key) {
        q.Begin(p);
        q.layout.Set("QM_AmountMeter", "user0", double(controls.maximum));
        q.layout.Set("QM_AmountMeter", "user2", double(controls.quantity));
        q.Build();
        q.revision = key;
      }
      World(q.surface, q.layout, mvp);
    }
  } else
    Notice(mvp, status, p, true);
}
inline void Shutdown() {
  for (int i = 0; i < 3; ++i) {
    auto &d = PipDocument(i);
    d.surface.Shutdown();
    d = {};
  }
  for (auto *d : {&DialogueDocument(), &BarterDocument(), &MessageDocument(),
                  &QuantityDocument()}) {
    d->surface.Shutdown();
    *d = {};
  }
  Shared().Shutdown();
}

} // namespace fo3menuui
