#pragma once

#include "player/fo3-player-state.h"
#include "world/interaction/fo3-loot-cursor.h"

namespace fo3chargen {
enum class Page { Main, Color, Shape, Asymmetry, Complexion };
enum class Kind { Race, Preset, Hair, Eyes, Beard, Page, Color, Control, Back, Done };
struct Row {
    std::string label;
    Kind kind;
    size_t index=0;
    std::vector<uint32_t> values{};
    uint32_t current=0;
};
struct Menu {
    uint32_t owner=0;
    Page page=Page::Main;
    fo3loot::Cursor cursor;
    bool neutral=false,aDown=false,bDown=false;
    int horizontal=0;
    double repeatAt=0;

    std::vector<Row> Rows(const fo3player::Player& player,
                          const std::vector<fo3character::Control>& controls) const {
        const auto draft=player.AppearanceDraft();
        const auto& catalog=player.Definitions().pipboy.appearance;
        std::vector<Row> rows;
        if(page==Page::Color){
            static const char* colors[]={"Red","Green","Blue"};
            for(size_t i=0;i<draft.color.size();++i)
                rows.push_back({std::string(colors[i])+"  < "+std::to_string(draft.color[i])+" >",Kind::Color,i});
        }else if(page!=Page::Main){
            const unsigned basis=unsigned(page)-unsigned(Page::Shape);
            for(size_t i=0;i<controls.size();++i)
                if(controls[i].basis==basis)
                    rows.push_back({controls[i].label+"  < "+std::to_string(draft.adjustments[i])+" >",Kind::Control,i});
        }else if(draft.race){
            const auto option=[&](const char* title,const auto& table,uint32_t id){
                const auto it=table.find(id);
                return std::string(title)+"  < "+(it==table.end()?"None":it->second.name)+" >";
            };
            const auto presets=catalog.Presets(draft.race,player.PlayerFemale());
            const auto number=std::find(presets.begin(),presets.end(),draft.preset)-presets.begin()+1;
            rows.push_back({option("Race",catalog.races,draft.race),Kind::Race,0,catalog.Races(),draft.race});
            rows.push_back({"Face Preset  < "+std::to_string(number)+" >",Kind::Preset,0,presets,draft.preset});
            rows.push_back({option("Hair",catalog.hair,draft.hair),Kind::Hair,0,catalog.Hair(draft.race,player.PlayerFemale()),draft.hair});
            rows.push_back({option("Eyes",catalog.eyes,draft.eyes),Kind::Eyes,0,catalog.Eyes(draft.race),draft.eyes});
            if(!player.PlayerFemale()){
                std::vector<uint32_t> beards{0};
                for(const auto& b:catalog.beards)beards.push_back(b.first);
                std::sort(beards.begin()+1,beards.end(),[&](uint32_t a,uint32_t b){
                    return catalog.beards.at(a).name<catalog.beards.at(b).name;
                });
                rows.push_back({option("Facial Hair",catalog.beards,draft.beard),Kind::Beard,0,std::move(beards),draft.beard});
            }
            rows.push_back({"Hair Color",Kind::Page,size_t(Page::Color)});
            if(!controls.empty()){
                rows.push_back({"Face Shape",Kind::Page,size_t(Page::Shape)});
                rows.push_back({"Face Asymmetry",Kind::Page,size_t(Page::Asymmetry)});
                rows.push_back({"Complexion",Kind::Page,size_t(Page::Complexion)});
            }
        }
        rows.push_back(page==Page::Main?Row{"Done",Kind::Done}:Row{"Back",Kind::Back});
        return rows;
    }

    enum class Result { None, Changed, Confirmed };
    Result Step(fo3player::Player& player,const std::vector<fo3character::Control>& controls,
                double now,float x,float y,bool a,bool b,bool focused,bool previewReady){
        const auto requested=player.Snapshot().appearanceMenuOwner;
        if(requested!=owner){*this={};owner=requested;aDown=a;bDown=b;return Result::Changed;}
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
        if(pressB&&page!=Page::Main){page=Page::Main;cursor={};return Result::Changed;}
        const auto rows=Rows(player,controls);
        const auto old=cursor.selected;
        cursor.Target(owner,rows.size());
        cursor.Scroll(y,now,rows.size());
        const auto& row=rows[cursor.selected];
        if(row.kind==Kind::Done&&pressA&&previewReady&&player.ConfirmAppearance())return Result::Confirmed;
        if(pressA&&(row.kind==Kind::Page||row.kind==Kind::Back)){
            page=row.kind==Kind::Back?Page::Main:Page(row.index);
            cursor={};return Result::Changed;
        }
        if(std::abs(x)<.3f)horizontal=0;
        int step=pressA?1:0;
        if(std::abs(x)>.65f){
            const int direction=x>0?1:-1;
            if(direction!=horizontal||now>=repeatAt){
                step=direction;repeatAt=now+(direction!=horizontal?.35:.14);horizontal=direction;
            }
        }
        if(step){
            auto draft=player.AppearanceDraft();
            const auto& catalog=player.Definitions().pipboy.appearance;
            if(row.kind==Kind::Color)
                draft.color[row.index]=uint8_t(std::clamp(int(draft.color[row.index])+8*step,0,255));
            else if(row.kind==Kind::Control)
                draft.adjustments[row.index]=int8_t(std::clamp(int(draft.adjustments[row.index])+step,-10,10));
            else if(!row.values.empty()){
                const auto found=std::find(row.values.begin(),row.values.end(),row.current);
                const auto count=int(row.values.size());
                const uint32_t value=row.values[(int(found-row.values.begin())+step+count)%count];
                switch(row.kind){
                    case Kind::Race:draft=catalog.Default(value,player.PlayerFemale());break;
                    case Kind::Preset:draft=catalog.Select(value,player.PlayerFemale());break;
                    case Kind::Hair:draft.hair=value;break;
                    case Kind::Eyes:draft.eyes=value;break;
                    case Kind::Beard:draft.beard=value;break;
                    default:break;
                }
            }
            if(player.EditAppearance(draft))return Result::Changed;
        }
        return cursor.selected!=old?Result::Changed:Result::None;
    }
};
}
