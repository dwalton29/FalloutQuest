#include "fo3-npc-q23.h"
#include "fo3-bsa-reader.h"

#include <android/log.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <zlib.h>

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr const char* ESM_PATH =
    "/data/user/0/com.falloutquest.app/files/Fallout3/Data/Fallout3.esm";
constexpr uint32_t TARGET_MEGATON_CELL = 0x00000A96u;
constexpr uint32_t FLAG_COMPRESSED = 0x00040000u;
constexpr uint64_t HEADER_SIZE = 24u;
constexpr uint32_t MAX_RECORD_BYTES = 64u * 1024u * 1024u;

#define Q230_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define Q230_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

struct GroupFrame {
    uint64_t end = 0u;
    uint32_t label = 0u;
    uint32_t type = 0u;
};

struct Locator {
    uint64_t payloadOffset = 0u;
    uint32_t storedSize = 0u;
    uint32_t flags = 0u;
    std::string type;
};

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
    std::string model2;
    std::string icon;
    std::vector<std::string> maleHeadModels;
};

uint16_t U16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8u);
}
uint32_t U32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8u) |
           (static_cast<uint32_t>(p[2]) << 16u) |
           (static_cast<uint32_t>(p[3]) << 24u);
}
float F32(const uint8_t* p) {
    const uint32_t bits = U32(p);
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}
void FloatArray(const uint8_t* p, uint32_t n, std::vector<float>& out) {
    out.clear();
    if ((n & 3u) != 0u) return;
    out.reserve(n / 4u);
    for (uint32_t at = 0u; at < n; at += 4u) out.push_back(F32(p + at));
}
bool ReadExact(FILE* f, void* dst, size_t n) {
    return std::fread(dst,1,n,f) == n;
}
int64_t FileSize(FILE* f) {
    const off_t old = ftello(f);
    if (old < 0) return -1;
    if (fseeko(f,0,SEEK_END) != 0) return -1;
    const off_t end = ftello(f);
    fseeko(f,old,SEEK_SET);
    return static_cast<int64_t>(end);
}
std::string FourCC(const uint8_t* p) {
    char s[5]{static_cast<char>(p[0]),static_cast<char>(p[1]),
              static_cast<char>(p[2]),static_cast<char>(p[3]),0};
    return s;
}
std::string ZString(const uint8_t* p, uint32_t n) {
    size_t len=0u;
    while(len<n && p[len]!=0u) ++len;
    return std::string(reinterpret_cast<const char*>(p),len);
}

bool Inflate(const std::vector<uint8_t>& stored,
             std::vector<uint8_t>& out) {
    if (stored.size()<4u) return false;
    const uint32_t n=U32(stored.data());
    if (n==0u || n>MAX_RECORD_BYTES) return false;
    out.resize(n);
    uLongf dest=n;
    const int rc=uncompress(
        reinterpret_cast<Bytef*>(out.data()),&dest,
        reinterpret_cast<const Bytef*>(stored.data()+4u),
        static_cast<uLong>(stored.size()-4u));
    if(rc!=Z_OK || dest!=n){ out.clear(); return false; }
    return true;
}

bool ReadPayload(FILE* f, const Locator& loc,
                 std::vector<uint8_t>& out) {
    if (loc.storedSize==0u || loc.storedSize>MAX_RECORD_BYTES) return false;
    if (fseeko(f,static_cast<off_t>(loc.payloadOffset),SEEK_SET)!=0) return false;
    std::vector<uint8_t> stored(loc.storedSize);
    if(!ReadExact(f,stored.data(),stored.size())) return false;
    if((loc.flags & FLAG_COMPRESSED)==0u){
        out.swap(stored);
        return true;
    }
    return Inflate(stored,out);
}

template <class Fn>
void Walk(const std::vector<uint8_t>& data, Fn fn) {
    size_t pos=0u;
    uint32_t extended=0u;
    while(pos+6u<=data.size()){
        const char* type=reinterpret_cast<const char*>(data.data()+pos);
        const uint16_t size16=U16(data.data()+pos+4u);
        pos+=6u;
        if(std::memcmp(type,"XXXX",4u)==0){
            if(size16!=4u || pos+4u>data.size()) return;
            extended=U32(data.data()+pos);
            pos+=4u;
            continue;
        }
        const uint32_t n=extended?extended:size16;
        extended=0u;
        if(n>data.size()-pos) return;
        fn(type,data.data()+pos,n);
        pos+=n;
    }
}

bool InMegatonCell(const std::vector<GroupFrame>& groups) {
    for(auto it=groups.rbegin();it!=groups.rend();++it){
        if(it->label==TARGET_MEGATON_CELL &&
           (it->type==6u || it->type==8u ||
            it->type==9u || it->type==10u)) return true;
    }
    return false;
}

bool ParseActorPlacement(const std::vector<uint8_t>& data,
                         RawActor& out) {
    bool base=false;
    Walk(data,[&](const char* type,const uint8_t* p,uint32_t n){
        if(std::memcmp(type,"NAME",4u)==0 && n>=4u){
            out.baseFormId=U32(p);
            base=out.baseFormId!=0u;
        } else if(std::memcmp(type,"DATA",4u)==0 && n>=24u){
            out.x=F32(p+0u); out.y=F32(p+4u); out.z=F32(p+8u);
            out.rx=F32(p+12u); out.ry=F32(p+16u); out.rz=F32(p+20u);
        } else if(std::memcmp(type,"XSCL",4u)==0 && n>=4u){
            out.scale=F32(p);
        }
    });
    return base;
}

bool ScanIndexAndActors(
        FILE* f,
        std::vector<RawActor>& actors,
        std::unordered_map<uint32_t,Locator>& locators) {
    const int64_t fileSize=FileSize(f);
    if(fileSize<static_cast<int64_t>(HEADER_SIZE)) return false;

    std::vector<GroupFrame> groups;
    uint64_t pos=0u;
    while(pos+HEADER_SIZE<=static_cast<uint64_t>(fileSize)){
        while(!groups.empty() && pos>=groups.back().end) groups.pop_back();
        if(fseeko(f,static_cast<off_t>(pos),SEEK_SET)!=0) break;
        uint8_t h[HEADER_SIZE]{};
        if(!ReadExact(f,h,sizeof(h))) break;
        const uint32_t size=U32(h+4u);

        if(std::memcmp(h,"GRUP",4u)==0){
            if(size<HEADER_SIZE || pos+size>static_cast<uint64_t>(fileSize)) break;
            groups.push_back({pos+size,U32(h+8u),U32(h+12u)});
            pos+=HEADER_SIZE;
            continue;
        }

        const uint32_t flags=U32(h+8u);
        const uint32_t formId=U32(h+12u);
        const uint64_t payloadOffset=pos+HEADER_SIZE;
        const uint64_t end=payloadOffset+size;
        if(end>static_cast<uint64_t>(fileSize)) break;
        const std::string type=FourCC(h);

        // Keep only actor-assembly record classes; this is tiny compared with a
        // full FormID index and lets later linked-record reads seek directly.
        if(type=="NPC_" || type=="RACE" || type=="HAIR" ||
           type=="HDPT" || type=="ARMO" || type=="EYES"){
            locators[formId]={payloadOffset,size,flags,type};
        }

        if(type=="ACHR" && InMegatonCell(groups)){
            Locator loc{payloadOffset,size,flags,type};
            std::vector<uint8_t> payload;
            if(ReadPayload(f,loc,payload)){
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
    if(!ReadPayload(f,loc,data)) return false;
    out.formId=formId;
    Walk(data,[&](const char* type,const uint8_t* p,uint32_t n){
        if(std::memcmp(type,"EDID",4u)==0 && out.editorId.empty())
            out.editorId=ZString(p,n);
        else if(std::memcmp(type,"FULL",4u)==0 && out.fullName.empty())
            out.fullName=ZString(p,n);
        else if(std::memcmp(type,"MODL",4u)==0 && out.skeleton.empty())
            out.skeleton=ZString(p,n);
        else if(std::memcmp(type,"ACBS",4u)==0 && n>=4u)
            out.baseFlags=U32(p);
        else if(std::memcmp(type,"RNAM",4u)==0 && n>=4u)
            out.race=U32(p);
        else if(std::memcmp(type,"HNAM",4u)==0 && n>=4u)
            out.hair=U32(p);
        else if(std::memcmp(type,"ENAM",4u)==0 && n>=4u)
            out.eyes=U32(p);
        else if(std::memcmp(type,"PNAM",4u)==0 && n>=4u)
            out.headParts.push_back(U32(p));
        else if(std::memcmp(type,"CNTO",4u)==0 && n>=8u)
            out.inventory.push_back({
                U32(p),static_cast<int32_t>(U32(p+4u))});
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

bool ParseLinked(FILE* f, const Locator& loc, Linked& out) {
    std::vector<uint8_t> data;
    if(!ReadPayload(f,loc,data)) return false;
    out.type=loc.type;

    // Fallout 3 RACE records contain an indexed male head-part table between
    // NAM0/MNAM and FNAM.  Q23.0 incorrectly treated the first MODL as the
    // entire race head.  Preserve every authored male piece (head, mouth,
    // teeth, tongue, left/right eye) by its INDX slot.
    bool raceHeadData=false;
    bool raceMaleHead=false;
    uint32_t raceHeadIndex=0xffffffffu;

    Walk(data,[&](const char* type,const uint8_t* p,uint32_t n){
        if(std::memcmp(type,"EDID",4u)==0 && out.editorId.empty())
            out.editorId=ZString(p,n);
        else if(std::memcmp(type,"FULL",4u)==0 && out.fullName.empty())
            out.fullName=ZString(p,n);

        if(loc.type=="RACE"){
            if(std::memcmp(type,"NAM0",4u)==0){
                raceHeadData=true;
                raceMaleHead=false;
                raceHeadIndex=0xffffffffu;
                return;
            }
            if(std::memcmp(type,"NAM1",4u)==0){
                raceHeadData=false;
                raceMaleHead=false;
                raceHeadIndex=0xffffffffu;
                return;
            }
            if(raceHeadData && std::memcmp(type,"MNAM",4u)==0){
                raceMaleHead=true;
                raceHeadIndex=0xffffffffu;
                return;
            }
            if(raceHeadData && std::memcmp(type,"FNAM",4u)==0){
                raceMaleHead=false;
                raceHeadIndex=0xffffffffu;
                return;
            }
            if(raceHeadData && raceMaleHead &&
               std::memcmp(type,"INDX",4u)==0 && n>=4u){
                raceHeadIndex=U32(p);
                return;
            }
        }

        if(std::memcmp(type,"MODL",4u)==0){
            const std::string path=ZString(p,n);
            if(out.model.empty()) out.model=path;
            if(loc.type=="RACE" && raceHeadData && raceMaleHead &&
               raceHeadIndex<8u && !path.empty()){
                if(out.maleHeadModels.size()<8u)
                    out.maleHeadModels.resize(8u);
                out.maleHeadModels[raceHeadIndex]=path;
            }
        } else if(std::memcmp(type,"MOD2",4u)==0 && out.model2.empty())
            out.model2=ZString(p,n);
        else if(std::memcmp(type,"ICON",4u)==0 && out.icon.empty())
            out.icon=ZString(p,n);
    });
    return true;
}

} // namespace

bool LoadFo3MegatonExteriorActorsQ230(
        std::vector<Fo3NpcActorQ230>& outActors) {
    outActors.clear();
    FILE* f=std::fopen(ESM_PATH,"rb");
    if(!f){
        Q230_LOGW("Q23.0 NPC ESM OPEN FAILED: %s",ESM_PATH);
        return false;
    }

    std::vector<RawActor> raw;
    std::unordered_map<uint32_t,Locator> locators;
    const bool indexed=ScanIndexAndActors(f,raw,locators);
    if(!indexed){
        std::fclose(f);
        Q230_LOGW("Q23.0 NPC INDEX FAILED: targetCell=%08X",TARGET_MEGATON_CELL);
        return false;
    }

    for(const RawActor& placed:raw){
        const auto npcLoc=locators.find(placed.baseFormId);
        if(npcLoc==locators.end() || npcLoc->second.type!="NPC_") continue;

        NpcBase npc;
        if(!ParseNpc(f,npcLoc->second,placed.baseFormId,npc)) continue;

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
        actor.scale=placed.scale;

        auto resolve=[&](uint32_t id, Linked& linked){
            const auto it=locators.find(id);
            return it!=locators.end() && ParseLinked(f,it->second,linked);
        };

        Linked race;
        if(npc.race!=0u && resolve(npc.race,race)){
            actor.raceEditorId=race.editorId;
            actor.raceHeadModel=race.model;
            actor.raceHeadModels=race.maleHeadModels;
        }

        Linked hair;
        if(npc.hair!=0u && resolve(npc.hair,hair))
            actor.hairModel=hair.model;

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
                if(ParseLinked(f,it->second,linked)){
                    item.recordType=linked.type;
                    item.editorId=linked.editorId;
                    item.fullName=linked.fullName;
                    if(linked.type=="ARMO"){
                        item.modelPath=
                            actor.female && !linked.model2.empty()
                                ? linked.model2
                                : linked.model;
                    }
                }
            }
            actor.inventory.push_back(std::move(item));
        }

        outActors.push_back(std::move(actor));
    }

    std::fclose(f);

    Q230_LOGI("Q23.0 NPC CENSUS: cell=%08X actors=%zu source=Fallout3.esm ACHR->NPC_",
              TARGET_MEGATON_CELL,outActors.size());
    for(const Fo3NpcActorQ230& a:outActors){
        size_t armorModels=0u;
        for(const auto& item:a.inventory)
            if(item.recordType=="ARMO" && !item.modelPath.empty()) ++armorModels;
        Q230_LOGI("Q23.3 NPC: ref=%08X base=%08X EDID=%s FULL=%s female=%d actorFlags=%08X race=%08X raceEDID=%s skeleton=%s maleRaceHeadParts=%zu hair=%s npcHeadParts=%zu eyeTex=%s inventory=%zu armorModels=%zu faceGen=(%zu,%zu,%zu) hairRGB=(%u,%u,%u) pos=(%.1f %.1f %.1f) rot=(%.3f %.3f %.3f)",
                  a.refFormId,a.baseFormId,
                  a.editorId.empty()?"<none>":a.editorId.c_str(),
                  a.fullName.empty()?"<none>":a.fullName.c_str(),
                  a.female?1:0,a.actorBaseFlags,a.raceFormId,
                  a.raceEditorId.empty()?"<none>":a.raceEditorId.c_str(),
                  a.skeletonModel.empty()?"<none>":a.skeletonModel.c_str(),
                  a.raceHeadModels.size(),
                  a.hairModel.empty()?"<none>":a.hairModel.c_str(),
                  a.headPartModels.size(),
                  a.eyeTexturePath.empty()?"<none>":a.eyeTexturePath.c_str(),
                  a.inventory.size(),armorModels,
                  a.faceGenGeometrySymmetric.size(),
                  a.faceGenGeometryAsymmetric.size(),
                  a.faceGenTextureSymmetric.size(),
                  static_cast<unsigned>(a.hairColor[0]),
                  static_cast<unsigned>(a.hairColor[1]),
                  static_cast<unsigned>(a.hairColor[2]),
                  a.x,a.y,a.z,a.rx,a.ry,a.rz);
        for(const auto& item:a.inventory){
            if(item.recordType=="ARMO" && !item.modelPath.empty()){
                Q230_LOGI("Q23.0 NPC ARMOR: actor=%s form=%08X EDID=%s FULL=%s count=%d model=%s",
                          a.editorId.c_str(),item.formId,
                          item.editorId.empty()?"<none>":item.editorId.c_str(),
                          item.fullName.empty()?"<none>":item.fullName.c_str(),
                          item.count,item.modelPath.c_str());
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

    const uint32_t vertices=U32(bytes.data()+8u);
    const uint32_t symModes=U32(bytes.data()+12u);
    const uint32_t asymModes=U32(bytes.data()+16u);
    const uint32_t basisVersion=U32(bytes.data()+20u);
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
            const float scale=F32(bytes.data()+at);
            at+=4u;
            const float coefficient=
                mode<coeffs.size()?coeffs[mode]:0.0f;
            for(uint32_t vertex=0u;vertex<vertices;++vertex){
                const int16_t dx=static_cast<int16_t>(U16(bytes.data()+at+0u));
                const int16_t dy=static_cast<int16_t>(U16(bytes.data()+at+2u));
                const int16_t dz=static_cast<int16_t>(U16(bytes.data()+at+4u));
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
