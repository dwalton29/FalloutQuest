#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace fo3vrheight {
// Device preferences, independent of Fallout saves and authored actor scale.
struct Settings {
    float adjustment=0,standing=0,crouchDrop=.28f;
    bool physicalCrouch=true;
    bool Valid()const{return std::isfinite(adjustment)&&adjustment>=-.5f&&adjustment<=.5f&&
        std::isfinite(standing)&&(standing==0||(standing>=.7f&&standing<=2.5f))&&
        std::isfinite(crouchDrop)&&crouchDrop>=.15f&&crouchDrop<=.5f;}
    bool Load(const std::string& path){
        FILE* f=std::fopen(path.c_str(),"r");if(!f)return false;
        Settings next;int version=0,physical=0;
        const int read=std::fscanf(f,"FQVR %d %f %f %f %d",&version,&next.adjustment,&next.standing,&next.crouchDrop,&physical);
        std::fclose(f);next.physicalCrouch=physical!=0;
        if(read!=5||version!=1||(physical!=0&&physical!=1)||!next.Valid())return false;
        *this=next;return true;
    }
    bool Save(const std::string& path)const{
        if(path.empty()||!Valid())return false;
        const std::string temp=path+".tmp";FILE* f=std::fopen(temp.c_str(),"w");if(!f)return false;
        std::fprintf(f,"FQVR 1 %.6f %.6f %.6f %d\n",adjustment,standing,crouchDrop,physicalCrouch);
        const bool ok=!std::ferror(f);const bool closed=std::fclose(f)==0;
        return ok&&closed&&std::rename(temp.c_str(),path.c_str())==0;
    }
};
struct Tracking {
    float floor=-1.55f,head=0,candidate=0,stableSeconds=0;
    bool floorReady=false,stageFloor=false,headTracked=false;
    void Observe(float headY,bool tracked,bool floorValid,float floorY,const Settings& settings){
        headTracked=tracked&&std::isfinite(headY)&&std::abs(headY)<20;
        if(!headTracked)return;
        head=headY;
        if(floorValid&&std::isfinite(floorY)&&std::abs(floorY)<20){floor=floorY;floorReady=stageFloor=true;}
        else if(!floorReady){floor=headY-(settings.standing>0?settings.standing:1.55f);floorReady=true;}
    }
    float PhysicalHeight(float y)const{return y-floor;}
    float Shift(float sceneFloor,const Settings& settings)const{return sceneFloor-floor+settings.adjustment;}
    bool Calibrate(Settings& settings)const{
        const float height=PhysicalHeight(head);
        if(!headTracked||!floorReady||height<.7f||height>2.5f)return false;
        settings.standing=height;return true;
    }
    bool AutoCalibrate(Settings& settings,float dt){
        if(settings.standing>0)return false;
        const float height=PhysicalHeight(head);
        if(!headTracked||height<.7f||height>2.5f){candidate=stableSeconds=0;return false;}
        if(candidate==0||std::abs(height-candidate)>.04f){candidate=height;stableSeconds=0;return false;}
        stableSeconds+=std::clamp(dt,0.f,.05f);
        return stableSeconds>=.5f&&Calibrate(settings);
    }
    void Recenter(float oldFromNewY){
        if(floorReady&&std::isfinite(oldFromNewY))floor-=oldFromNewY;
        headTracked=false;
        candidate=stableSeconds=0;
    }
};
// A capsule may become a sphere, but may never be shorter than its diameter.
inline float CapsuleHeight(float desired,float radius,float scale){
    return std::clamp(desired,2*radius+.002f*scale,3.1f*scale);
}
}
