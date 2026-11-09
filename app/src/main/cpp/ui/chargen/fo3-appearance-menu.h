#pragma once

#include "player/fo3-player-state.h"
#include "world/interaction/fo3-loot-cursor.h"
#include <functional>
#include <random>

namespace fo3chargen {
enum class Page { Sex, Race, Face, Hair, Customize, HairStyle, HairColor, Beard, Eyes,
                  Shape, Asymmetry, Complexion };
enum class Kind { Sex, Race, Preset, Hair, Eyes, Beard, Page, Color, Control,
                  Randomize, Back, Next, Done, Yes, No };
struct Row {
    std::string label;
    Kind kind;
    size_t index=0;
    bool checked=false;
    std::string value;
    int minimum=0,maximum=0,current=0;
    bool Slider()const{return kind==Kind::Preset||kind==Kind::Color||kind==Kind::Control;}
    bool Button()const{return kind==Kind::Back||kind==Kind::Next||kind==Kind::Done;}
};
using Labels=std::function<std::string(const char*)>;

struct Menu {
    enum class Confirm { None, Randomize, Done };
    enum class Result { None, Changed, Confirmed };
    uint32_t owner=0;
    Page page=Page::Sex;
    Confirm confirmation=Confirm::None;
    fo3loot::Cursor cursor;
    size_t previousSelection=0;
    bool neutral=false,aDown=false,bDown=false;
    int horizontal=0;
    double repeatAt=0;

    unsigned Stage()const{
        switch(page){
            case Page::Sex:return 0;
            case Page::Race:return 1;
            case Page::Hair:case Page::HairStyle:case Page::HairColor:case Page::Beard:return 3;
            default:return 2;
        }
    }
    bool Primary()const{return page<=Page::Hair;}
    std::string Title(const Labels& label)const{
        static const char* stages[]={"sRSMSex","sRace","sRSMFace","sRSMHair"};
        switch(page){
            case Page::Customize:return label("sRSMCustomize");
            case Page::HairStyle:return label("sRSMHairStyle");
            case Page::HairColor:return label("sRSMHairColor");
            case Page::Beard:return label("sRSMFacialHair");
            case Page::Eyes:return label("sRSMEyeColor");
            case Page::Shape:return label("sRSMGeneral");
            case Page::Asymmetry:return "Asymmetry";
            case Page::Complexion:return label("sComplexion");
            default:return std::to_string(Stage()+1)+". "+label(stages[Stage()]);
        }
    }

    std::vector<Row> Rows(const fo3player::Player& player,
                          const std::vector<fo3character::Control>& controls,
                          const Labels& label)const{
        const auto draft=player.AppearanceDraft();
        const auto& catalog=player.Definitions().pipboy.appearance;
        std::vector<Row> rows;
        const auto choice=[&](const std::string& name,Kind kind,size_t id,bool checked=false){
            rows.push_back({name,kind,id,checked,{}});
        };
        const auto slider=[&](const std::string& name,Kind kind,size_t id,int value,int lo,int hi){
            rows.push_back({name,kind,id,false,std::to_string(value),lo,hi,value});
        };
        const auto options=[&](const auto& table,const auto& ids,Kind kind,uint32_t selected){
            for(auto id:ids){
                const auto it=table.find(id);
                choice(it==table.end()?label("sNone"):it->second.name,kind,id,id==selected);
            }
        };
        const auto link=[&](const char* text,Page target){choice(label(text)+" >",Kind::Page,size_t(target));};
        if(confirmation!=Confirm::None){
            choice(label("sYes"),Kind::Yes,0);
            choice(label("sNo"),Kind::No,0);
            return rows;
        }
        switch(page){
            case Page::Sex:
                choice(label("sMale"),Kind::Sex,0,!player.PlayerFemale());
                choice(label("sFemale"),Kind::Sex,1,player.PlayerFemale());
                break;
            case Page::Race:{
                auto races=catalog.Races();
                std::stable_sort(races.begin(),races.end(),[&](auto a,auto b){return catalog.races.at(a).name<catalog.races.at(b).name;});
                options(catalog.races,races,Kind::Race,draft.race);
                break;
            }
            case Page::Face:{
                const auto presets=catalog.Presets(draft.race,player.PlayerFemale());
                const auto it=std::find(presets.begin(),presets.end(),draft.preset);
                if(it!=presets.end())slider(label("sRSMPreset"),Kind::Preset,0,int(it-presets.begin()+1),1,int(presets.size()));
                if(draft.race)link("sRSMCustomize",Page::Customize);
                if(presets.size()>1)choice(label("sRSMRandomize"),Kind::Randomize,0);
                break;
            }
            case Page::Hair:
                if(draft.race){
                    link("sRSMHairStyle",Page::HairStyle);
                    link("sRSMHairColor",Page::HairColor);
                    if(!player.PlayerFemale())link("sRSMFacialHair",Page::Beard);
                }
                break;
            case Page::Customize:
                link("sRSMEyeColor",Page::Eyes);
                if(!controls.empty()){
                    link("sRSMGeneral",Page::Shape);
                    choice("Asymmetry >",Kind::Page,size_t(Page::Asymmetry));
                    link("sComplexion",Page::Complexion);
                }
                break;
            case Page::HairStyle:
                options(catalog.hair,catalog.Hair(draft.race,player.PlayerFemale()),Kind::Hair,draft.hair);
                break;
            case Page::Eyes:
                options(catalog.eyes,catalog.Eyes(draft.race),Kind::Eyes,draft.eyes);
                break;
            case Page::Beard:{
                std::vector<uint32_t> beards{0};
                for(const auto& b:catalog.beards)beards.push_back(b.first);
                std::sort(beards.begin()+1,beards.end(),[&](auto a,auto b){return catalog.beards.at(a).name<catalog.beards.at(b).name;});
                options(catalog.beards,beards,Kind::Beard,draft.beard);
                break;
            }
            case Page::HairColor:{
                static const char* colors[]={"sRSMRedAbbrev","sRSMGreenAbbrev","sRSMBlueAbbrev"};
                for(size_t i=0;i<draft.color.size();++i)
                    slider(label(colors[i]),Kind::Color,i,draft.color[i],0,255);
                break;
            }
            default:{
                const unsigned basis=unsigned(page)-unsigned(Page::Shape);
                for(size_t i=0;i<std::min(controls.size(),draft.adjustments.size());++i)
                    if(controls[i].basis==basis)
                        slider(controls[i].label,Kind::Control,i,draft.adjustments[i],-10,10);
                break;
            }
        }
        if(page!=Page::Sex)choice(label("sBack"),Kind::Back,0);
        if(Primary())choice(label(page==Page::Hair?"sDone":"sNext"),page==Page::Hair?Kind::Done:Kind::Next,0);
        return rows;
    }

    void Open(Page target,const fo3player::Player& player,
              const std::vector<fo3character::Control>& controls,const Labels& labels){
        page=target;
        cursor={};
        const auto rows=Rows(player,controls,labels);
        cursor.Target(owner,rows.size());
        const auto selected=std::find_if(rows.begin(),rows.end(),[](const Row& r){return r.checked;});
        if(selected!=rows.end())cursor.selected=size_t(selected-rows.begin());
    }
    void Back(const fo3player::Player& player,const std::vector<fo3character::Control>& controls,const Labels& labels){
        if(page==Page::Sex)return;
        auto target=Page::Customize;
        if(Primary())target=Page(unsigned(page)-1);
        else if(page==Page::Customize)target=Page::Face;
        else if(Stage()==3)target=Page::Hair;
        Open(target,player,controls,labels);
    }

    Result Step(fo3player::Player& player,const std::vector<fo3character::Control>& controls,
                const Labels& labels,double now,float x,float y,bool a,bool b,bool focused,bool previewReady){
        const auto requested=player.Snapshot().appearanceMenuOwner;
        if(requested!=owner){
            *this={};owner=requested;aDown=a;bDown=b;
            if(owner)Open(Page::Sex,player,controls,labels);
            return Result::Changed;
        }
        if(!owner)return Result::None;
        const bool pressA=a&&!aDown,pressB=b&&!bDown;
        aDown=a;bDown=b;
        if(!focused||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(now)){
            neutral=false;horizontal=0;return Result::None;
        }
        if(!neutral){
            neutral=!a&&!b&&std::abs(x)<.3f&&std::abs(y)<.3f;
            return Result::None;
        }
        const auto rows=Rows(player,controls,labels);
        const auto old=cursor.selected;
        cursor.Target(owner,rows.size());
        cursor.Scroll(y,now,rows.size());
        const auto& row=rows[cursor.selected];
        auto draft=player.AppearanceDraft();
        const auto& catalog=player.Definitions().pipboy.appearance;
        if(confirmation!=Confirm::None){
            if(!pressA&&!pressB)return cursor.selected!=old?Result::Changed:Result::None;
            if(!pressB&&row.kind==Kind::Yes&&confirmation==Confirm::Done){
                return previewReady&&player.ConfirmAppearance()?Result::Confirmed:Result::None;
            }
            if(!pressB&&row.kind==Kind::Yes){
                auto presets=catalog.Presets(draft.race,player.PlayerFemale());
                presets.erase(std::remove(presets.begin(),presets.end(),draft.preset),presets.end());
                if(!presets.empty()){
                    static thread_local std::mt19937 random{std::random_device{}()};
                    player.EditAppearance(catalog.Select(presets[std::uniform_int_distribution<size_t>(0,presets.size()-1)(random)],player.PlayerFemale()));
                }
            }
            confirmation=Confirm::None;cursor={};cursor.Target(owner,Rows(player,controls,labels).size());
            cursor.selected=previousSelection;
            return Result::Changed;
        }
        if(pressB||(pressA&&row.kind==Kind::Back)){
            Back(player,controls,labels);return Result::Changed;
        }
        if(pressA){
            switch(row.kind){
                case Kind::Page:Open(Page(row.index),player,controls,labels);return Result::Changed;
                case Kind::Next:Open(Page(unsigned(page)+1),player,controls,labels);return Result::Changed;
                case Kind::Done:case Kind::Randomize:
                    previousSelection=cursor.selected;
                    confirmation=row.kind==Kind::Done?Confirm::Done:Confirm::Randomize;
                    cursor={};cursor.Target(owner,2);cursor.selected=1;
                    return Result::Changed;
                case Kind::Sex:
                    if(player.PlayerFemale()!=bool(row.index)){
                        player.SetPlayerSex(bool(row.index));
                        if(draft.race)player.EditAppearance(catalog.Default(draft.race,player.PlayerFemale()));
                        return Result::Changed;
                    }
                    break;
                case Kind::Race:
                    if(draft.race!=row.index&&player.EditAppearance(catalog.Default(uint32_t(row.index),player.PlayerFemale())))return Result::Changed;
                    break;
                case Kind::Hair:draft.hair=uint32_t(row.index);break;
                case Kind::Eyes:draft.eyes=uint32_t(row.index);break;
                case Kind::Beard:draft.beard=uint32_t(row.index);break;
                default:break;
            }
        }
        if(std::abs(x)<.3f)horizontal=0;
        int step=pressA?1:0;
        if(std::abs(x)>.65f){
            const int direction=x>0?1:-1;
            if(direction!=horizontal||now>=repeatAt){
                step=direction;repeatAt=now+(direction!=horizontal?.35:.14);horizontal=direction;
            }
        }
        if(step&&row.Slider()){
            if(row.kind==Kind::Color)
                draft.color[row.index]=uint8_t(std::clamp(row.current+8*step,0,255));
            else if(row.kind==Kind::Control)
                draft.adjustments[row.index]=int8_t(std::clamp(row.current+step,row.minimum,row.maximum));
            else{
                const auto presets=catalog.Presets(draft.race,player.PlayerFemale());
                const int i=(row.current-1+step+int(presets.size()))%int(presets.size());
                draft=catalog.Select(presets[size_t(i)],player.PlayerFemale());
            }
        }
        if((pressA||step)&&player.EditAppearance(draft))return Result::Changed;
        return cursor.selected!=old?Result::Changed:Result::None;
    }
};
}
