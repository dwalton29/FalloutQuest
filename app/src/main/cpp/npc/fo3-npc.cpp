#include "fo3-install-paths.h"
#include "fo3-npc.h"
#include "fo3-bsa-reader.h"
#include "fo3-texture-bsa.h"
#include "fo3-esm-reader.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";



#define Q230_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q230_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

struct GroupFrame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

using Locator = fo3esm::RecordLocation;

struct RawActor {
    uint32_t refFormId = 0u;
    uint32_t baseFormId = 0u;
    uint32_t flags = 0u;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    float scale = 1.0f;
};

struct NpcBase {
    uint32_t formId = 0u;
    uint32_t baseFlags = 0u;
    uint32_t templateId = 0u;
    uint16_t templateFlags = 0u;
    float height = 1.0f;
    uint32_t race = 0u;
    uint32_t hair = 0u;
    uint32_t eyes = 0u;
    std::string editorId;
    std::string fullName;
    std::string skeleton;
    std::vector<uint32_t> headParts;
    std::vector<std::pair<uint32_t,int32_t>> inventory;
    std::vector<float> faceSymmetric;
    std::vector<float> faceAsymmetric;
    std::vector<float> faceTextureSymmetric;
    uint8_t hairColor[4]{0u,0u,0u,0u};
    bool faceGen = false;
};

struct Linked {
    std::string type;
    std::string editorId;
    std::string fullName;
    std::string model;
    std::string model3;
    std::string icon;
    std::vector<std::string> headModels;
    std::vector<std::string> headTextures;
    std::vector<std::string> bodyModels;
    std::vector<std::string> bodyTextures;
    uint32_t bipedMask = 0u;
    std::vector<float> faceSymmetric;
    std::vector<float> faceAsymmetric;
    std::vector<float> faceTextureSymmetric;
};

void FloatArray(const uint8_t* p, uint32_t n, std::vector<float>& out) {
    out.clear();
    if ((n & 3u) != 0u) return;
    out.reserve(n / 4u);
    for (uint32_t at = 0u; at < n; at += 4u)
        out.push_back(fo3esm::ReadF32(p + at));
}

bool InActorCell(const std::vector<GroupFrame>& groups, uint32_t cell) {
    for(auto it=groups.rbegin();it!=groups.rend();++it){
        if(it->label==cell &&
           (it->type==6u || it->type==8u ||
            it->type==9u || it->type==10u)) return true;
    }
    return false;
}

bool ParseActorPlacement(const std::vector<uint8_t>& data,
                         RawActor& out) {
    bool base=false, placement=false;
    fo3esm::WalkSubrecords(data,[&](const char* type,const uint8_t* p,uint32_t n){
        if(std::memcmp(type,"NAME",4u)==0 && n>=4u){
            out.baseFormId=fo3esm::ReadU32(p);
            base=out.baseFormId!=0u;
        } else if(std::memcmp(type,"DATA",4u)==0 && n>=24u){
            placement=true;
            out.x=fo3esm::ReadF32(p+0u); out.y=fo3esm::ReadF32(p+4u); out.z=fo3esm::ReadF32(p+8u);
            out.rx=fo3esm::ReadF32(p+12u); out.ry=fo3esm::ReadF32(p+16u); out.rz=fo3esm::ReadF32(p+20u);
        } else if(std::memcmp(type,"XSCL",4u)==0 && n>=4u){
            out.scale=fo3esm::ReadF32(p);
        }
    });
    return base && placement && std::isfinite(out.x) && std::isfinite(out.y) &&
        std::isfinite(out.z) && std::isfinite(out.rx) && std::isfinite(out.ry) &&
        std::isfinite(out.rz) && std::isfinite(out.scale) && out.scale>0;
}

bool ScanIndexAndActors(
        FILE* f,
        std::vector<RawActor>& actors,
        std::unordered_map<uint32_t,Locator>& locators, uint32_t cell) {
    const int64_t fileSize=fo3esm::FileSize(f);
    if(fileSize<static_cast<int64_t>(fo3esm::HEADER_SIZE)) return false;

    std::vector<GroupFrame> groups;
    uint64_t pos=0u;
    while(pos+fo3esm::HEADER_SIZE<=static_cast<uint64_t>(fileSize)){
        while(!groups.empty() && pos>=groups.back().end) groups.pop_back();
        if(fseeko(f,static_cast<off_t>(pos),SEEK_SET)!=0) break;
        uint8_t h[fo3esm::HEADER_SIZE]{};
        if(!fo3esm::ReadExact(f,h,sizeof(h))) break;
        const uint32_t size=fo3esm::ReadU32(h+4u);

        if(std::memcmp(h,"GRUP",4u)==0){
            if(size<fo3esm::HEADER_SIZE || pos+size>static_cast<uint64_t>(fileSize)) break;
            groups.push_back({pos+size,fo3esm::ReadU32(h+8u),fo3esm::ReadU32(h+12u)});
            pos+=fo3esm::HEADER_SIZE;
            continue;
        }

        const uint32_t flags=fo3esm::ReadU32(h+8u);
        const uint32_t formId=fo3esm::ReadU32(h+12u);
        const uint64_t payloadOffset=pos+fo3esm::HEADER_SIZE;
        const uint64_t end=payloadOffset+size;
        if(end>static_cast<uint64_t>(fileSize)) break;
        const std::string type=fo3esm::FourCC(h);

        // Keep only actor-assembly record classes; this is tiny compared with a
        // full FormID index and lets later linked-record reads seek directly.
        if(type=="NPC_" || type=="RACE" || type=="HAIR" ||
           type=="HDPT" || type=="ARMO" || type=="EYES"){
            locators[formId]={payloadOffset,size,flags,type};
        }

        if(type=="ACHR" && InActorCell(groups,cell)){
            Locator loc{payloadOffset,size,flags,type};
            std::vector<uint8_t> payload;
            if(fo3esm::ReadPayload(f,loc,payload)){
                RawActor a;
                a.refFormId=formId;
                a.flags=flags;
                if(ParseActorPlacement(payload,a))
                    actors.push_back(a);
            }
        }
        pos=end;
    }
    return !actors.empty();
}

bool ParseNpc(FILE* f, const Locator& loc,
              uint32_t formId, NpcBase& out) {
    std::vector<uint8_t> data;
    if(!fo3esm::ReadPayload(f,loc,data)) return false;
    out.formId=formId;
    fo3esm::WalkSubrecords(data,[&](const char* type,const uint8_t* p,uint32_t n){
        if(std::memcmp(type,"EDID",4u)==0 && out.editorId.empty())
            out.editorId=fo3esm::ZString(p,n);
        else if(std::memcmp(type,"FULL",4u)==0 && out.fullName.empty())
            out.fullName=fo3esm::ZString(p,n);
        else if(std::memcmp(type,"MODL",4u)==0 && out.skeleton.empty())
            out.skeleton=fo3esm::ZString(p,n);
        else if(std::memcmp(type,"ACBS",4u)==0 && n>=4u) {
            out.baseFlags=fo3esm::ReadU32(p);
            if(n>=24u) out.templateFlags=fo3esm::ReadU16(p+22u);
        } else if(std::memcmp(type,"TPLT",4u)==0 && n>=4u)
            out.templateId=fo3esm::ReadU32(p);
        else if(std::memcmp(type,"NAM6",4u)==0 && n>=4u)
            out.height=fo3esm::ReadF32(p);
        else if(std::memcmp(type,"RNAM",4u)==0 && n>=4u)
            out.race=fo3esm::ReadU32(p);
        else if(std::memcmp(type,"HNAM",4u)==0 && n>=4u)
            out.hair=fo3esm::ReadU32(p);
        else if(std::memcmp(type,"ENAM",4u)==0 && n>=4u)
            out.eyes=fo3esm::ReadU32(p);
        else if(std::memcmp(type,"PNAM",4u)==0 && n>=4u)
            out.headParts.push_back(fo3esm::ReadU32(p));
        else if(std::memcmp(type,"CNTO",4u)==0 && n>=8u)
            out.inventory.push_back({
                fo3esm::ReadU32(p),static_cast<int32_t>(fo3esm::ReadU32(p+4u))});
        else if(std::memcmp(type,"HCLR",4u)==0 && n>=4u) {
            out.hairColor[0]=p[0]; out.hairColor[1]=p[1];
            out.hairColor[2]=p[2]; out.hairColor[3]=p[3];
        } else if(std::memcmp(type,"FGGS",4u)==0) {
            FloatArray(p,n,out.faceSymmetric);
            out.faceGen=true;
        } else if(std::memcmp(type,"FGGA",4u)==0) {
            FloatArray(p,n,out.faceAsymmetric);
            out.faceGen=true;
        } else if(std::memcmp(type,"FGTS",4u)==0) {
            FloatArray(p,n,out.faceTextureSymmetric);
            out.faceGen=true;
        }
    });
    return !out.editorId.empty() || !out.fullName.empty();
}

// xEdit FO3 ACBS template categories: traits=0, model/animation=6,
// base data=7, inventory=8. Resolve only appearance categories we consume.
bool ResolveNpc(FILE* f, uint32_t id,
        const std::unordered_map<uint32_t,Locator>& locators,
        std::unordered_set<uint32_t>& visiting, NpcBase& npc) {
    if(!visiting.insert(id).second || visiting.size()>32u) return false;
    auto it=locators.find(id);
    if(it==locators.end() || it->second.type!="NPC_" ||
       !ParseNpc(f,it->second,id,npc)) return false;
    if(npc.templateId && (npc.templateFlags & (1u|64u|128u|256u))) {
        NpcBase parent;
        if(!ResolveNpc(f,npc.templateId,locators,visiting,parent)) {
            Q230_LOGW("ACTOR TEMPLATE UNSUPPORTED: actor=%08X template=%08X",id,npc.templateId);
            return false;
        }
        if(npc.templateFlags & 1u) {
            npc.race=parent.race;
            npc.height=parent.height;
            npc.baseFlags=(npc.baseFlags & ~1u)|(parent.baseFlags & 1u);
        }
        if(npc.templateFlags & 64u) {
            npc.skeleton=parent.skeleton; npc.hair=parent.hair; npc.eyes=parent.eyes;
            npc.headParts=parent.headParts; npc.faceSymmetric=parent.faceSymmetric;
            npc.faceAsymmetric=parent.faceAsymmetric;
            npc.faceTextureSymmetric=parent.faceTextureSymmetric;
            npc.faceGen=parent.faceGen;
            std::copy_n(parent.hairColor,4,npc.hairColor);
        }
        if(npc.templateFlags & 128u) npc.fullName=parent.fullName;
        if(npc.templateFlags & 256u) npc.inventory=parent.inventory;
    }
    visiting.erase(id);
    return true;
}

bool ParseLinked(FILE* f, const Locator& loc, Linked& out, bool female) {
    std::vector<uint8_t> data;
    if(!fo3esm::ReadPayload(f,loc,data)) return false;
    out.type=loc.type;

    bool raceHeadData=false;
    bool raceBodyData=false;
    bool raceSelectedHead=false;
    bool raceSelectedBody=false;
    uint32_t raceHeadIndex=0xffffffffu;
    uint32_t raceBodyIndex=0xffffffffu;
    int raceFaceGender=0; // 1 male, 2 female

    fo3esm::WalkSubrecords(data,[&](const char* type,const uint8_t* p,uint32_t n){
        if(std::memcmp(type,"EDID",4u)==0 && out.editorId.empty())
            out.editorId=fo3esm::ZString(p,n);
        else if(std::memcmp(type,"FULL",4u)==0 && out.fullName.empty())
            out.fullName=fo3esm::ZString(p,n);

        if(loc.type=="ARMO" &&
           std::memcmp(type,"BMDT",4u)==0 && n>=4u){
            out.bipedMask=fo3esm::ReadU32(p);
        }

        if(loc.type=="RACE"){
            if(std::memcmp(type,"NAM0",4u)==0){
                raceHeadData=true;
                raceBodyData=false;
                raceSelectedHead=false;
                raceSelectedBody=false;
                raceHeadIndex=0xffffffffu;
                raceBodyIndex=0xffffffffu;
                return;
            }
            if(std::memcmp(type,"NAM1",4u)==0){
                raceHeadData=false;
                raceBodyData=true;
                raceSelectedHead=false;
                raceSelectedBody=false;
                raceHeadIndex=0xffffffffu;
                raceBodyIndex=0xffffffffu;
                return;
            }
            // HNAM/ENAM come after the body model table and before the final
            // male/female FaceGen data markers.
            if((std::memcmp(type,"HNAM",4u)==0 ||
                std::memcmp(type,"ENAM",4u)==0) && raceBodyData){
                raceBodyData=false;
                raceSelectedBody=false;
                raceBodyIndex=0xffffffffu;
            }
            if(std::memcmp(type,"MNAM",4u)==0){
                if(raceHeadData){
                    raceSelectedHead=!female;
                    raceHeadIndex=0xffffffffu;
                } else if(raceBodyData){
                    raceSelectedBody=!female;
                    raceBodyIndex=0xffffffffu;
                } else {
                    raceFaceGender=1;
                }
                return;
            }
            if(std::memcmp(type,"FNAM",4u)==0){
                if(raceHeadData){
                    raceSelectedHead=female;
                    raceHeadIndex=0xffffffffu;
                } else if(raceBodyData){
                    raceSelectedBody=female;
                    raceBodyIndex=0xffffffffu;
                } else {
                    raceFaceGender=2;
                }
                return;
            }
            if(std::memcmp(type,"INDX",4u)==0 && n>=4u){
                if(raceHeadData && raceSelectedHead){
                    raceHeadIndex=fo3esm::ReadU32(p);
                    return;
                }
                if(raceBodyData && raceSelectedBody){
                    raceBodyIndex=fo3esm::ReadU32(p);
                    return;
                }
            }
            if(std::memcmp(type,"FGGS",4u)==0 && raceFaceGender==(female?2:1)){
                FloatArray(p,n,out.faceSymmetric);
                return;
            }
            if(std::memcmp(type,"FGGA",4u)==0 && raceFaceGender==(female?2:1)){
                FloatArray(p,n,out.faceAsymmetric);
                return;
            }
            if(std::memcmp(type,"FGTS",4u)==0 && raceFaceGender==(female?2:1)){
                FloatArray(p,n,out.faceTextureSymmetric);
                return;
            }
        }

        if(std::memcmp(type,"MODL",4u)==0){
            const std::string path=fo3esm::ZString(p,n);
            if(out.model.empty()) out.model=path;
            if(loc.type=="RACE" && raceHeadData && raceSelectedHead &&
               raceHeadIndex<8u && !path.empty()){
                if(out.headModels.size()<8u)
                    out.headModels.resize(8u);
                out.headModels[raceHeadIndex]=path;
            }
            if(loc.type=="RACE" && raceBodyData && raceSelectedBody &&
               raceBodyIndex<4u && !path.empty()){
                if(out.bodyModels.size()<4u)
                    out.bodyModels.resize(4u);
                out.bodyModels[raceBodyIndex]=path;
            }
        } else if(std::memcmp(type,"MOD3",4u)==0 && out.model3.empty()){
            out.model3=fo3esm::ZString(p,n);
        } else if(std::memcmp(type,"ICON",4u)==0){
            const std::string path=fo3esm::ZString(p,n);
            if(out.icon.empty()) out.icon=path;
            if(loc.type=="RACE" && raceHeadData && raceSelectedHead &&
               raceHeadIndex<8u && !path.empty()){
                if(out.headTextures.size()<8u)
                    out.headTextures.resize(8u);
                if(out.headTextures[raceHeadIndex].empty())
                    out.headTextures[raceHeadIndex]=path;
            }
            if(loc.type=="RACE" && raceBodyData && raceSelectedBody &&
               raceBodyIndex<4u && !path.empty()){
                if(out.bodyTextures.size()<4u)
                    out.bodyTextures.resize(4u);
                if(out.bodyTextures[raceBodyIndex].empty())
                    out.bodyTextures[raceBodyIndex]=path;
            }
        }
    });
    return true;
}

} // namespace

bool LoadFo3CellActors(
        uint32_t cellFormId, std::vector<Fo3NpcActorQ230>& outActors,
        const std::string& esmPath) {
    outActors.clear();
    FILE* f=std::fopen(esmPath.empty()?fo3assets::FalloutMasterPath().c_str():esmPath.c_str(),"rb");
    if(!f){
        Q230_LOGW("Q23.0 NPC ESM OPEN FAILED: %s",fo3assets::FalloutMasterPath().c_str());
        return false;
    }

    std::vector<RawActor> raw;
    std::unordered_map<uint32_t,Locator> locators;
    const bool indexed=ScanIndexAndActors(f,raw,locators,cellFormId);
    if(!indexed){
        std::fclose(f);
        Q230_LOGW("Q23.0 NPC INDEX FAILED: targetCell=%08X",cellFormId);
        return false;
    }

    for(const RawActor& placed:raw){
        // Deleted and initially disabled references need game-state evaluation.
        if ((placed.flags & (0x20u | 0x800u)) != 0u) continue;
        const auto npcLoc=locators.find(placed.baseFormId);
        if(npcLoc==locators.end() || npcLoc->second.type!="NPC_") continue;

        NpcBase npc;
        std::unordered_set<uint32_t> visiting;
        if(!ResolveNpc(f,placed.baseFormId,locators,visiting,npc)) continue;

        Fo3NpcActorQ230 actor;
        actor.refFormId=placed.refFormId;
        actor.baseFormId=placed.baseFormId;
        actor.referenceFlags=placed.flags;
        actor.actorBaseFlags=npc.baseFlags;
        actor.female=(npc.baseFlags & 0x00000001u)!=0u;
        actor.raceFormId=npc.race;
        actor.hairFormId=npc.hair;
        actor.eyesFormId=npc.eyes;
        actor.editorId=npc.editorId;
        actor.fullName=npc.fullName;
        actor.skeletonModel=npc.skeleton;
        actor.headPartFormIds=npc.headParts;
        actor.faceGenGeometrySymmetric=npc.faceSymmetric;
        actor.faceGenGeometryAsymmetric=npc.faceAsymmetric;
        actor.faceGenTextureSymmetric=npc.faceTextureSymmetric;
        for(int i=0;i<4;++i) actor.hairColor[i]=npc.hairColor[i];
        actor.hasFaceGenGeometry=npc.faceGen;
        actor.x=placed.x; actor.y=placed.y; actor.z=placed.z;
        actor.rx=placed.rx; actor.ry=placed.ry; actor.rz=placed.rz;
        actor.scale=placed.scale * (std::isfinite(npc.height) && npc.height>0.0f ? npc.height : 1.0f);

        auto resolve=[&](uint32_t id, Linked& linked){
            const auto it=locators.find(id);
            return it!=locators.end() && ParseLinked(f,it->second,linked,actor.female);
        };

        Linked race;
        if(npc.race!=0u && resolve(npc.race,race)){
            actor.raceEditorId=race.editorId;
            actor.raceHeadModel=race.headModels.empty()?std::string{}:race.headModels[0];
            actor.raceHeadModels=race.headModels;
            actor.raceHeadTextures=race.headTextures;
            actor.raceBodyModels=race.bodyModels;
            actor.raceBodyTextures=race.bodyTextures;
            if(race.bodyModels.size()>3u)
                actor.raceBodyTextureModel=race.bodyModels[3u];
            actor.raceFaceGenGeometrySymmetric=race.faceSymmetric;
            actor.raceFaceGenGeometryAsymmetric=race.faceAsymmetric;
            actor.raceFaceGenTextureSymmetric=race.faceTextureSymmetric;
        }

        Linked hair;
        if(npc.hair!=0u && resolve(npc.hair,hair)){
            actor.hairModel=hair.model;
            actor.hairTexturePath=hair.icon;
        }

        Linked eyes;
        if(npc.eyes!=0u && resolve(npc.eyes,eyes))
            actor.eyeTexturePath=eyes.icon;

        for(uint32_t id:npc.headParts){
            Linked part;
            if(resolve(id,part) && !part.model.empty())
                actor.headPartModels.push_back(part.model);
        }

        for(const auto& inv:npc.inventory){
            Fo3NpcVisualItemQ230 item;
            item.formId=inv.first;
            item.count=inv.second;
            const auto it=locators.find(inv.first);
            if(it!=locators.end()){
                Linked linked;
                if(ParseLinked(f,it->second,linked,actor.female)){
                    item.recordType=linked.type;
                    item.editorId=linked.editorId;
                    item.fullName=linked.fullName;
                    if(linked.type=="ARMO"){
                        item.modelPath=
                            actor.female && !linked.model3.empty()
                                ? linked.model3
                                : linked.model;
                        item.bipedMask=linked.bipedMask;
                    }
                }
            }
            actor.inventory.push_back(std::move(item));
        }

        outActors.push_back(std::move(actor));
    }

    std::fclose(f);

    Q230_LOGI("Q23.0 NPC CENSUS: cell=%08X actors=%zu source=Fallout3.esm ACHR->NPC_",
              cellFormId,outActors.size());
    for(const Fo3NpcActorQ230& a:outActors){
        size_t armorModels=0u;
        for(const auto& item:a.inventory)
            if(item.recordType=="ARMO" && !item.modelPath.empty()) ++armorModels;
        Q230_LOGI("Q23.6 NPC: ref=%08X base=%08X EDID=%s FULL=%s female=%d actorFlags=%08X race=%08X raceEDID=%s skeleton=%s raceHeadParts=%zu raceBodyParts=%zu hair=%s hairTex=%s npcHeadParts=%zu eyeTex=%s inventory=%zu armorModels=%zu npcFaceGen=(%zu,%zu,%zu) raceFaceGen=(%zu,%zu,%zu) hairRGB=(%u,%u,%u) pos=(%.1f %.1f %.1f) rot=(%.3f %.3f %.3f)",
                  a.refFormId,a.baseFormId,
                  a.editorId.empty()?"<none>":a.editorId.c_str(),
                  a.fullName.empty()?"<none>":a.fullName.c_str(),
                  a.female?1:0,a.actorBaseFlags,a.raceFormId,
                  a.raceEditorId.empty()?"<none>":a.raceEditorId.c_str(),
                  a.skeletonModel.empty()?"<none>":a.skeletonModel.c_str(),
                  a.raceHeadModels.size(),
                  a.raceBodyModels.size(),
                  a.hairModel.empty()?"<none>":a.hairModel.c_str(),
                  a.hairTexturePath.empty()?"<none>":a.hairTexturePath.c_str(),
                  a.headPartModels.size(),
                  a.eyeTexturePath.empty()?"<none>":a.eyeTexturePath.c_str(),
                  a.inventory.size(),armorModels,
                  a.faceGenGeometrySymmetric.size(),
                  a.faceGenGeometryAsymmetric.size(),
                  a.faceGenTextureSymmetric.size(),
                  a.raceFaceGenGeometrySymmetric.size(),
                  a.raceFaceGenGeometryAsymmetric.size(),
                  a.raceFaceGenTextureSymmetric.size(),
                  static_cast<unsigned>(a.hairColor[0]),
                  static_cast<unsigned>(a.hairColor[1]),
                  static_cast<unsigned>(a.hairColor[2]),
                  a.x,a.y,a.z,a.rx,a.ry,a.rz);
        for(const auto& item:a.inventory){
            if(item.recordType=="ARMO" && !item.modelPath.empty()){
                Q230_LOGI("Q23.6 NPC ARMOR: actor=%s form=%08X EDID=%s FULL=%s count=%d bipedMask=%08X model=%s",
                          a.editorId.c_str(),item.formId,
                          item.editorId.empty()?"<none>":item.editorId.c_str(),
                          item.fullName.empty()?"<none>":item.fullName.c_str(),
                          item.count,item.bipedMask,item.modelPath.c_str());
            }
        }
    }
    return !outActors.empty();
}


bool LoadFo3FaceGenMorphQ233(
        const std::string& nifPath,
        const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric,
        Fo3FaceGenMorphQ233& out) {
    out = {};
    if (nifPath.empty()) return false;

    std::string egmPath=nifPath;
    const size_t dot=egmPath.find_last_of('.');
    if(dot==std::string::npos) return false;
    egmPath.replace(dot,std::string::npos,".egm");

    std::vector<uint8_t> bytes;
    std::string resolved;
    if(!LoadFalloutMeshFile(egmPath,bytes,&resolved)) return false;
    if(bytes.size()<64u || std::memcmp(bytes.data(),"FREGM002",8u)!=0){
        Q230_LOGW("Q23.3 FACEGEN EGM INVALID: nif=%s egm=%s bytes=%zu",
                  nifPath.c_str(),resolved.c_str(),bytes.size());
        return false;
    }

    const uint32_t vertices=fo3esm::ReadU32(bytes.data()+8u);
    const uint32_t symModes=fo3esm::ReadU32(bytes.data()+12u);
    const uint32_t asymModes=fo3esm::ReadU32(bytes.data()+16u);
    const uint32_t basisVersion=fo3esm::ReadU32(bytes.data()+20u);
    if(vertices==0u || vertices>200000u ||
       symModes>256u || asymModes>256u){
        return false;
    }

    const uint64_t bytesPerMode=
        4ull+static_cast<uint64_t>(vertices)*6ull;
    const uint64_t required=
        64ull+bytesPerMode*
        (static_cast<uint64_t>(symModes)+
         static_cast<uint64_t>(asymModes));
    if(required>bytes.size()){
        Q230_LOGW("Q23.3 FACEGEN EGM TRUNCATED: egm=%s vertices=%u modes=(%u,%u) need=%llu have=%zu",
                  resolved.c_str(),vertices,symModes,asymModes,
                  static_cast<unsigned long long>(required),bytes.size());
        return false;
    }

    out.egmPath=resolved;
    out.vertexCount=vertices;
    out.symmetricModes=symModes;
    out.asymmetricModes=asymModes;
    out.geometryBasisVersion=basisVersion;
    out.deltaXYZ.assign(static_cast<size_t>(vertices)*3u,0.0f);

    size_t at=64u;
    auto consume=[&](uint32_t modeCount,const std::vector<float>& coeffs){
        for(uint32_t mode=0u;mode<modeCount;++mode){
            const float scale=fo3esm::ReadF32(bytes.data()+at);
            at+=4u;
            const float coefficient=
                mode<coeffs.size()?coeffs[mode]:0.0f;
            for(uint32_t vertex=0u;vertex<vertices;++vertex){
                const int16_t dx=static_cast<int16_t>(fo3esm::ReadU16(bytes.data()+at+0u));
                const int16_t dy=static_cast<int16_t>(fo3esm::ReadU16(bytes.data()+at+2u));
                const int16_t dz=static_cast<int16_t>(fo3esm::ReadU16(bytes.data()+at+4u));
                at+=6u;
                if(std::fabs(coefficient)>1.0e-8f){
                    const float factor=coefficient*scale;
                    const size_t base=static_cast<size_t>(vertex)*3u;
                    out.deltaXYZ[base+0u]+=static_cast<float>(dx)*factor;
                    out.deltaXYZ[base+1u]+=static_cast<float>(dy)*factor;
                    out.deltaXYZ[base+2u]+=static_cast<float>(dz)*factor;
                }
            }
        }
    };
    consume(symModes,symmetric);
    consume(asymModes,asymmetric);

    Q230_LOGI("Q23.3 FACEGEN EGM READY: nif=%s egm=%s vertices=%u modes=(%u,%u) coeffs=(%zu,%zu) basis=%u source=Fallout-Meshes.bsa+NPC_FGGS/FGGA",
              nifPath.c_str(),resolved.c_str(),vertices,
              symModes,asymModes,symmetric.size(),asymmetric.size(),
              basisVersion);
    return true;
}


bool LoadFo3FaceGenTextureQ234(
        const std::string& nifPath,
        const std::string& baseTexturePath,
        const std::vector<float>& symmetric,
        Fo3FaceGenTextureQ234& out) {
    out = {};
    if(nifPath.empty() || baseTexturePath.empty() || symmetric.empty())
        return false;

    std::string egtPath=nifPath;
    const size_t dot=egtPath.find_last_of('.');
    if(dot==std::string::npos) return false;
    egtPath.replace(dot,std::string::npos,".egt");

    std::vector<uint8_t> bytes;
    std::string resolved;
    if(!LoadFalloutMeshFile(egtPath,bytes,&resolved)) return false;
    if(bytes.size()<64u || std::memcmp(bytes.data(),"FREGT003",8u)!=0){
        return false;
    }

    const uint32_t rows=fo3esm::ReadU32(bytes.data()+8u);
    const uint32_t columns=fo3esm::ReadU32(bytes.data()+12u);
    const uint32_t symModes=fo3esm::ReadU32(bytes.data()+16u);
    const uint32_t asymModes=fo3esm::ReadU32(bytes.data()+20u);
    const uint32_t basisVersion=fo3esm::ReadU32(bytes.data()+24u);
    if(rows==0u || columns==0u || rows>8192u || columns>8192u ||
       symModes==0u || symModes>256u || asymModes>256u){
        return false;
    }

    const uint64_t pixels=
        static_cast<uint64_t>(rows)*static_cast<uint64_t>(columns);
    const uint64_t bytesPerMode=4ull+pixels*3ull;
    const uint64_t required=
        64ull+bytesPerMode*
        (static_cast<uint64_t>(symModes)+
         static_cast<uint64_t>(asymModes));
    if(required>bytes.size()) return false;

    Fo3RgbaTexture base;
    if(!LoadFalloutTextureRgba(baseTexturePath,base) ||
       base.width<=0 || base.height<=0 ||
       base.rgba.size()!=
           static_cast<size_t>(base.width)*base.height*4u){
        return false;
    }
    if(static_cast<uint64_t>(base.width)*rows !=
       static_cast<uint64_t>(base.height)*columns){
        Q230_LOGW("Q23.4 FACEGEN EGT ASPECT MISS: nif=%s egt=%s base=%s egtDims=%ux%u baseDims=%dx%d",
                  nifPath.c_str(),resolved.c_str(),baseTexturePath.c_str(),
                  columns,rows,base.width,base.height);
        return false;
    }

    std::vector<float> delta(static_cast<size_t>(pixels)*3u,0.0f);
    size_t at=64u;
    for(uint32_t mode=0u;mode<symModes;++mode){
        const float scale=fo3esm::ReadF32(bytes.data()+at);
        at+=4u;
        const uint8_t* r=bytes.data()+at;
        const uint8_t* g=r+pixels;
        const uint8_t* b=g+pixels;
        at+=static_cast<size_t>(pixels)*3u;
        const float coefficient=
            mode<symmetric.size()?symmetric[mode]:0.0f;
        if(std::fabs(coefficient)<1.0e-8f) continue;
        const float factor=coefficient*scale;
        for(size_t px=0u;px<static_cast<size_t>(pixels);++px){
            delta[px*3u+0u]+=
                static_cast<float>(static_cast<int8_t>(r[px]))*factor;
            delta[px*3u+1u]+=
                static_cast<float>(static_cast<int8_t>(g[px]))*factor;
            delta[px*3u+2u]+=
                static_cast<float>(static_cast<int8_t>(b[px]))*factor;
        }
    }
    // Fallout 3 NPC records expose only FGTS (symmetric texture coordinates).
    // Consume the asymmetric EGT payload only for validation/layout parity.
    at+=static_cast<size_t>(bytesPerMode)*asymModes;
    if(at>bytes.size()) return false;

    auto sampleDelta=[&](float sx,float sy,int channel){
        sx=std::clamp(sx,0.0f,static_cast<float>(columns-1u));
        sy=std::clamp(sy,0.0f,static_cast<float>(rows-1u));
        const uint32_t x0=static_cast<uint32_t>(std::floor(sx));
        const uint32_t y0=static_cast<uint32_t>(std::floor(sy));
        const uint32_t x1=std::min(x0+1u,columns-1u);
        const uint32_t y1=std::min(y0+1u,rows-1u);
        const float fx=sx-static_cast<float>(x0);
        const float fy=sy-static_cast<float>(y0);
        auto v=[&](uint32_t x,uint32_t y){
            return delta[
                (static_cast<size_t>(y)*columns+x)*3u+
                static_cast<size_t>(channel)];
        };
        const float a=v(x0,y0)*(1.0f-fx)+v(x1,y0)*fx;
        const float c=v(x0,y1)*(1.0f-fx)+v(x1,y1)*fx;
        return a*(1.0f-fy)+c*fy;
    };

    out.egtPath=resolved;
    out.baseTexturePath=baseTexturePath;
    out.rows=rows;
    out.columns=columns;
    out.symmetricModes=symModes;
    out.asymmetricModes=asymModes;
    out.textureBasisVersion=basisVersion;
    out.width=base.width;
    out.height=base.height;
    out.rgba=base.rgba;

    for(int y=0;y<base.height;++y){
        const float sy=
            ((static_cast<float>(y)+0.5f)/
             static_cast<float>(base.height))*
             static_cast<float>(rows)-0.5f;
        for(int x=0;x<base.width;++x){
            const float sx=
                ((static_cast<float>(x)+0.5f)/
                 static_cast<float>(base.width))*
                 static_cast<float>(columns)-0.5f;
            const size_t dst=
                (static_cast<size_t>(y)*base.width+x)*4u;
            for(int c=0;c<3;++c){
                const float value=
                    static_cast<float>(base.rgba[dst+c])+
                    // Supplied FO3 EGT images use the opposite vertical
                    // convention to their DDS base maps.
                    sampleDelta(sx,static_cast<float>(rows-1u)-sy,c);
                out.rgba[dst+c]=static_cast<uint8_t>(
                    std::lround(std::clamp(value,0.0f,255.0f)));
            }
        }
    }

    Q230_LOGI("Q23.4 FACEGEN EGT READY: nif=%s egt=%s base=%s egtDims=%ux%u baseDims=%dx%d modes=(%u,%u) coeffs=%zu basis=%u operation=base-plus-EGT-delta alpha=preserved",
              nifPath.c_str(),resolved.c_str(),baseTexturePath.c_str(),
              columns,rows,base.width,base.height,
              symModes,asymModes,symmetric.size(),basisVersion);
    return true;
}


bool LoadFo3MegatonExteriorActorsQ230(std::vector<Fo3NpcActorQ230>& actors) {
    return LoadFo3CellActors(0x00000A96u, actors);
}
