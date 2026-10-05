#pragma once
#include <string_view>
namespace fo3pip {
// Original DATA is MapMenu and identifies itself with pipboymenu, not the
// shared pipboy prefab entity used by the STATS/ITEMS XMLs.
inline bool MenuAssetValid(std::string_view path,std::string_view xml){
 if(path=="menus\\globals.xml")return xml.find("<_pipboy_width> 1024")!=xml.npos;
 if(path=="menus\\main\\map_menu.xml")
  return xml.find("<menu name=\"MapMenu\">")!=xml.npos&&xml.find("&pipboymenu;")!=xml.npos;
 return xml.find("&pipboy;")!=xml.npos;
}
}
