#include "fo3-footsteps.h"
#include "fo3-audio-catalog.h"
#include "data/fo3-esm-reader.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <memory>
#include <cstring>
namespace fo3audio {
bool SafePath(const std::string &path) {
  if (path.empty() || path.front() == '/' ||
      path.find(':') != std::string::npos ||
      path.find('\n') != std::string::npos ||
      path.find('\r') != std::string::npos)
    return false;
  size_t at = 0;
  while (at < path.size()) {
    auto end = path.find('/', at);
    auto part = path.substr(at, end - at);
    if (part == ".." || part == "." || part.empty())
      return false;
    if (end == std::string::npos)
      break;
    at = end + 1;
  }
  return true;
}
std::string Catalog::MusicForCell(uint32_t id) const {
  auto cell = cells.find(id);
  if (cell == cells.end())
    return {};
  uint32_t musicId = cell->second.music;
  // Only the exterior exploration fallback is enabled in this milestone.
  // Interior music without an explicit XCMO is left silent rather than guessed.
  if (!cell->second.hasMusic && !(cell->second.flags & 1)) {
    auto name = names.find("DefaultExplore");
    if (name != names.end())
      musicId = name->second;
  }
  auto m = music.find(musicId);
  return m == music.end() ? std::string{} : m->second;
}
uint32_t Catalog::AmbientForCell(uint32_t id) const {
  auto c = cells.find(id);
  if (c == cells.end())
    return 0;
  auto s = spaces.find(c->second.space);
  return s == spaces.end() ? 0 : s->second;
}
bool IsLoadingScreenEffect(const std::string& path) {
  constexpr char prefix[]="sound/fx/ui/loadscreen/";
  if(path.size()<sizeof(prefix)-1)return false;
  for(size_t i=0;i<sizeof(prefix)-1;++i){
    const char c=path[i]=='\\'?'/':char(std::tolower(static_cast<unsigned char>(path[i])));
    if(c!=prefix[i])return false;
  }
  return true;
}
float Sound::GainAt(float distance) const {
  if(distance<=nearDistance||farDistance<=nearDistance)return gain;
  return gain*std::clamp((farDistance-distance)/(farDistance-nearDistance),0.f,1.f);
}
bool LoadCatalog(const std::string &path, Catalog &out, std::string &error) {
  struct CloseFile {
    void operator()(FILE *f) const { fclose(f); }
  };
  std::unique_ptr<FILE, CloseFile> f(fopen(path.c_str(), "rb"));
  if (!f) {
    error = "audio ESM unavailable";
    return false;
  }
  const auto size = fo3esm::FileSize(f.get());
  if (size < 24) {
    error = "audio ESM truncated";
    return false;
  }
  Catalog catalog;
  struct Children {uint64_t start,end;uint32_t world,cell;};
  std::vector<Children> children;
  std::function<bool(uint64_t,uint64_t,unsigned,uint32_t,uint32_t,bool)> walk = [&](uint64_t at,
                                                               uint64_t end,
                                                               unsigned depth,uint32_t world,uint32_t owner,bool refs) {
    if (depth > 32)
      return false;
    while (at < end) {
      if (end - at < 24 || fseeko(f.get(), at, SEEK_SET))
        return false;
      uint8_t h[24];
      if (!fo3esm::ReadExact(f.get(), h, 24))
        return false;
      auto type = fo3esm::FourCC(h);
      auto n = fo3esm::ReadU32(h + 4), flags = fo3esm::ReadU32(h + 8),
           id = fo3esm::ReadU32(h + 12);
      if (type == "GRUP") {
        if (n < 24 || n > end - at)return false;
        const auto kind=fo3esm::ReadU32(h+12),label=fo3esm::ReadU32(h+8);
        if(!refs && kind==6)children.push_back({at+24,at+n,world,label});
        else if(!walk(at+24,at+n,depth+1,kind==1?label:world,owner,refs))
          return false;
        at += n;
        continue;
      }
      if (n > end - at - 24)
        return false;
      const bool selected = refs ? type=="REFR" : type=="REGN" || type=="WTHR" ||
          type == "SOUN" || type == "MUSC" || type == "ASPC" ||
          type == "CELL" || type == "DOOR" || type == "CONT" ||
          type == "WEAP" || type == "ARMO" || type == "AMMO" ||
          type == "MISC" || type == "ALCH" || type == "INGR" ||
          type == "BOOK" || type == "NOTE" || type == "KEYM";
      if (selected && !(flags & 0x20)) {
        std::vector<uint8_t> bytes;
        if (!fo3esm::ReadPayloadCurrent(f.get(), n, flags, bytes))
          return false;
        if(refs){
          uint32_t base=0;bool positioned=false;
          SoundPlacement source{id,0,0,0,0,(flags&0x800)==0};
          fo3esm::WalkSubrecords(bytes,[&](const char* tag,const uint8_t* p,uint32_t len){
            if(!memcmp(tag,"NAME",4)&&len==4)base=fo3esm::ReadU32(p);
            if(!memcmp(tag,"DATA",4)&&len==24){
              source.x=fo3esm::ReadF32(p);source.y=fo3esm::ReadF32(p+4);source.z=fo3esm::ReadF32(p+8);
              positioned=std::isfinite(source.x)&&std::isfinite(source.y)&&std::isfinite(source.z);
            }
          });
          if(positioned&&catalog.sounds.count(base)){
            source.sound=base;catalog.placements[world?world:owner].push_back(source);
          }
          at+=24+n;continue;
        }
        size_t pos = 0;
        uint32_t extended = 0;
        while (pos < bytes.size()) {
          if (bytes.size() - pos < 6)
            return false;
          const auto tag = fo3esm::FourCC(bytes.data() + pos);
          uint32_t length = fo3esm::ReadU16(bytes.data() + pos + 4);
          pos += 6;
          if (tag == "XXXX") {
            if (extended || length != 4 || bytes.size() - pos < 4)
              return false;
            extended = fo3esm::ReadU32(bytes.data() + pos);
            pos += 4;
            if (!extended)
              return false;
            continue;
          }
          length = extended ? extended : length;
          extended = 0;
          if (length > bytes.size() - pos)
            return false;
          pos += length;
        }
        if (extended)
          return false;
        SoundRegion region;bool soundSection=false;
        Sound sound;
        Object object;
        Cell cell;
        std::string name, filename;
        uint32_t ambient = 0;
        fo3esm::WalkSubrecords(bytes, [&](const char *tag, const uint8_t *p,
                                          uint32_t len) {
          std::string t(tag, 4);
          if(type=="REGN"){
            if(t=="WNAM"&&len==4)region.world=fo3esm::ReadU32(p);
            if(t=="RPLD"&&len%8==0){
              region.areas.emplace_back();
              for(uint32_t i=0;i<len;i+=8)region.areas.back().push_back({fo3esm::ReadF32(p+i),fo3esm::ReadF32(p+i+4)});
            }
            if(t=="RDAT"&&len>=6){
              soundSection=fo3esm::ReadU32(p)==7;
              if(soundSection){region.overrideLower=(p[4]&1)!=0;region.priority=p[5];}
            }
            if(t=="RDSD"&&soundSection&&len%12==0)
              for(uint32_t i=0;i<len;i+=12)region.sounds.push_back({fo3esm::ReadU32(p+i),fo3esm::ReadU32(p+i+4),fo3esm::ReadU32(p+i+8)});
          }
          if(type=="ASPC"&&t=="RDAT"&&len==4)catalog.spaceRegions[id]=fo3esm::ReadU32(p);
          if(type=="WTHR"&&t=="DATA"&&len>=12)catalog.weatherMasks[id]=p[11]&15;
          if(type=="WTHR"&&t=="SNAM"&&len==8&&fo3esm::ReadU32(p+4)==2)
            catalog.weatherLoops[id].push_back(fo3esm::ReadU32(p));
          if (t == "EDID")
            name = fo3esm::ZString(p, len);
          if (t == "FNAM" && (type == "SOUN" || type == "MUSC"))
            filename = fo3esm::ZString(p, len);
          if (type == "SOUN" && (t == "SNDX" || t == "SNDD") && len >= 12) {
            sound.nearDistance=5.f*p[0];
            sound.farDistance=100.f*p[1];
            sound.endTime=p[10];sound.startTime=p[11];
            sound.positional=(fo3esm::ReadU32(p+4)&0x40)==0;
            sound.loop = (fo3esm::ReadU32(p + 4) & 16) != 0;
            int16_t attenuation = static_cast<int16_t>(fo3esm::ReadU16(p + 8));
            sound.gain =
                std::clamp(std::pow(10.f, -attenuation / 2000.f), 0.f, 1.f);
          }
          if (len == 4) {
            auto form = fo3esm::ReadU32(p);
            if (t == "YNAM")
              object.pickup = form;
            if (t == "SNAM") {
              object.open = form;
              if (type == "ASPC")
                ambient = form;
            }
            if (t == "QNAM" || (type == "DOOR" && t == "ANAM"))
              object.close = form;
            if (t == "XCMO") {
              cell.music = form;
              cell.hasMusic = true;
            }
            if (t == "XCAS")
              cell.space = form;
          }
          if (type == "CELL" && t == "DATA" && len)
            cell.flags = p[0];
        });
        std::replace(filename.begin(), filename.end(), '\\', '/');
        if (type == "SOUN") {
          if (SafePath(filename))
            sound.path = "sound/" + filename;
          catalog.sounds[id] = sound;
          catalog.names[name] = id;
        } else if (type == "MUSC") {
          catalog.music[id] =
              SafePath(filename) ? "music/" + filename : std::string{};
          catalog.names[name] = id;
        } else if (type == "CELL")
          catalog.cells[id] = cell;
        else if (type == "ASPC")
          catalog.spaces[id] = ambient;
        else if(type=="REGN"){if(!region.sounds.empty())catalog.regions[id]=std::move(region);}
        else if(type!="WTHR")
          catalog.objects[id] = object;
      }
      at += 24 + n;
    }
    return true;
  };
  if (!walk(0,size,0,0,0,false)||!std::all_of(children.begin(),children.end(),[&](const Children& c){return walk(c.start,c.end,1,c.world,c.cell,true);})) {
    error = "invalid audio ESM records";
    return false;
  }
  out = std::move(catalog);
  error.clear();
  return true;
}
} // namespace fo3audio

namespace fo3audio {
uint32_t Catalog::Footstep(uint32_t material,unsigned action) const {
  if(action>=FootstepActions.size())return 0;
  if((material&64u)&&action<4)action=(action==0||action==3)?6:7;
  unsigned surface=FootstepMaterials[material&31u];
  for(unsigned attempt=0;attempt<3 && surface<10;++attempt) {
    const auto named=names.find(std::string("FST")+FootstepSurfaces[surface]+FootstepActions[action]);
    if(named!=names.end() && sounds.count(named->second))return named->second;
    surface=FootstepFallback[surface];
  }
  const auto fallback=names.find("FSTConcSolidWalkR");
  return fallback==names.end()?0:fallback->second;
}
} // namespace fo3audio

namespace fo3audio {
static bool Inside(const std::vector<std::array<float,2>>& polygon,float x,float y){
  bool inside=false;
  for(size_t i=0;i<polygon.size();++i){
    const auto& a=polygon[i];const auto& b=polygon[i?i-1:polygon.size()-1];
    if((a[1]>y)!=(b[1]>y)&&x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])inside=!inside;
  }
  return inside;
}
std::vector<AmbientVoice> Catalog::Environment(const Listener& at) const {
  std::vector<AmbientVoice> voices;
  const auto cell=cells.find(at.cell);
  if(cell==cells.end())return voices;
  const bool exterior=!(cell->second.flags&1);
  auto add=[&](uint64_t key,uint32_t id,float distance,float pan,uint32_t chance){
    const auto found=sounds.find(id);
    if(found==sounds.end()||found->second.path.empty())return;
    const auto& s=found->second;
    const float start=s.startTime*.09375f,end=s.endTime*.09375f;
    if(start!=end&&(start<end?(at.hour<start||at.hour>end):(at.hour<start&&at.hour>end)))return;
    const float gain=s.GainAt(distance);
    if(gain>0)voices.push_back({key,id,gain,s.positional?pan:0,s.loop,chance});
  };
  const auto bed=AmbientForCell(at.cell);
  if(bed){
    add((uint64_t(2)<<32)|bed,bed,0,0,0);
    if(!voices.empty())voices.back().loop=true;
  }
  std::vector<const SoundRegion*> active;
  auto interior=spaceRegions.find(cell->second.space);
  if(interior!=spaceRegions.end()){
    auto r=regions.find(interior->second);if(r!=regions.end())active.push_back(&r->second);
  }else if(exterior&&at.world){
    for(const auto& pair:regions){
      const auto& r=pair.second;
      if(r.world==at.world&&std::any_of(r.areas.begin(),r.areas.end(),[&](const auto& area){return Inside(area,at.x,at.y);}))
        active.push_back(&r);
    }
  }
  unsigned minimum=0;
  for(auto r:active)if(r->overrideLower)minimum=std::max(minimum,unsigned(r->priority));
  auto weather=weatherMasks.find(at.weather);
  for(auto r:active)if(r->priority>=minimum)for(const auto& entry:r->sounds){
    if(exterior&&entry.weatherMask&&weather!=weatherMasks.end()&&weather->second&&!(entry.weatherMask&weather->second))continue;
    if(std::none_of(voices.begin(),voices.end(),[&](const auto& v){return v.key==entry.form;}))
      add(entry.form,entry.form,0,0,entry.chance);
  }
  auto windy=weatherLoops.find(at.weather);
  if(exterior&&windy!=weatherLoops.end())for(auto id:windy->second)
    if(std::none_of(voices.begin(),voices.end(),[&](const auto& v){return v.key==id;}))add(id,id,0,0,0);
  const auto sources=placements.find(exterior?at.world:at.cell);
  if(sources!=placements.end())for(const auto& source:sources->second){
    if(!(at.states?at.states->Enabled(source.reference,source.enabled):source.enabled))continue;
    const auto& s=sounds.at(source.sound);
    const float dx=source.x-at.x,dy=source.y-at.y,dz=source.z-at.z;
    const float horizontal=std::hypot(dx,dy),distance=std::hypot(horizontal,dz);
    if(s.farDistance>0&&distance>=s.farDistance)continue;
    if(exterior&&s.farDistance==0&&distance>4096)continue;
    const float pan=horizontal?(dx*std::cos(at.yaw)+dy*std::sin(at.yaw))/horizontal:0;
    add((uint64_t(1)<<32)|source.reference,source.sound,distance,pan,0);
  }
  return voices;
}
}
