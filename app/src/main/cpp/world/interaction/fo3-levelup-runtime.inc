// Fallout 3 LevelUpMenu: two canonical pages (skills, perks) followed by
// Intense Training's S.P.E.C.I.A.L. choice when selected. World-space panel
// uses the established VR text renderer; source data comes from PERK/GMST.
#include "player/fo3-levelup-rules.h"

struct Fo3LevelUpSession {
    enum class Page : uint8_t { Skills, Perks, Intense };
    fo3dialogue::Panel panel;
    fo3loot::Cursor cursor;
    fo3levelup::Skills original{},allocated{};
    std::vector<uint32_t> eligible;
    Page page=Page::Skills;
    fo3player::Player* owner=nullptr;
    uint32_t selectedPerk=0;
    int points=0,remaining=0;
    bool lastA=false,lastB=false;
    uint64_t serial=20000000;
};
Fo3LevelUpSession gLevelUp;

bool Fo3LevelUpFocus(){return gLevelUp.panel.visible;}

static constexpr std::array<const char*,14> Fo3LevelUpSkillNames{
    "Barter","Big Guns","Energy Weapons","Explosives","Lockpick",
    "Medicine","Melee Weapons","Repair","Science","Small Guns",
    "Sneak","Speech","Throwing","Unarmed"
};
static constexpr std::array<const char*,7> Fo3LevelUpSpecialNames{
    "Strength","Perception","Endurance","Charisma",
    "Intelligence","Agility","Luck"
};
void Fo3LevelUpRefresh() {
    auto& u=gLevelUp;
    if(!u.owner)return;
    const auto& player=*u.owner;
    auto& panel=u.panel;panel.choices.clear();panel.visible=true;
    const auto level=uint32_t(player.Snapshot().level)+1;
    panel.title="WELCOME TO LEVEL "+std::to_string(level);
    if(u.page==Fo3LevelUpSession::Page::Skills){
        panel.subtitle="ASSIGN "+std::to_string(u.points)+
            " SKILL POINTS    REMAINING "+std::to_string(u.remaining);
        for(size_t i=0;i<u.allocated.size();++i) {
            if(i==12)continue; // No Throwing skill in original Fallout 3.
            panel.choices.push_back(std::string(Fo3LevelUpSkillNames[i])+
                "  "+std::to_string(u.allocated[i])+
                (u.allocated[i]>u.original[i]?
                    " (+"+std::to_string(int(u.allocated[i])-int(u.original[i]))+")":""));
        }
        panel.choices.push_back("RESET SKILL POINTS");
        panel.choices.push_back("CONTINUE");
        panel.footer="A Add/Select    B Remove    Stick Navigate";
    }else if(u.page==Fo3LevelUpSession::Page::Perks){
        u.eligible.clear();
        const auto& data=player.Definitions().pipboy;
        for(const auto& p:data.perks)
            if(player.LevelUpPerkSupported(p.first)&&
               fo3levelup::PerkEligible(p.first,p.second,uint16_t(level),
                   player.Snapshot().special,u.allocated,data.playerFemale,
                   player.Snapshot().pipboy.perks))u.eligible.push_back(p.first);
        std::sort(u.eligible.begin(),u.eligible.end(),[&](uint32_t a,uint32_t b){
            const auto& x=data.perks.at(a).name;const auto& y=data.perks.at(b).name;
            return x==y?a<b:x<y;
        });
        for(uint32_t id:u.eligible) {
            const auto& p=data.perks.at(id);
            const auto acquired=player.Snapshot().pipboy.perks.find(id);
            panel.choices.push_back(p.name+
                (acquired==player.Snapshot().pipboy.perks.end()?"": 
                 " (RANK "+std::to_string(acquired->second+1)+")"));
        }
        panel.subtitle="CHOOSE 1 PERK";
        panel.footer="A Select Perk    B Back    Stick Navigate";
    }else {
        const auto& special=player.Snapshot().special;
        panel.subtitle="INTENSE TRAINING: CHOOSE 1 SPECIAL POINT";
        for(size_t i=0;i<special.size();++i)
            panel.choices.push_back(std::string(Fo3LevelUpSpecialNames[i])+
                " "+std::to_string(special[i])+
                (special[i]>=10?" (MAX)":""));
        panel.footer="A Select    B Back    Stick Navigate";
    }
    u.cursor.Target(uint32_t(u.page)+101,uint32_t(panel.choices.size()));
    panel.selected=u.cursor.selected;
    if(u.page==Fo3LevelUpSession::Page::Perks &&
       panel.selected<u.eligible.size())
        panel.subtitle+="\n"+
            player.Definitions().pipboy.perks.at(u.eligible[panel.selected]).description;
    panel.revision=++u.serial;
}

bool Fo3LevelUpCombatActive() {
    // Original level-up is deferred while any live actor is attacking the player.
    for(const auto& actor:gQ230NpcActors)
        if(actor.runtime.activity==fo3npc::Activity::Combat&&
           actor.runtime.combatTarget==fo3player::PlayerRef)return true;
    return false;
}

void UpdateFo3LevelUp(double now,float scroll,bool pressedA,bool pressedB,
                      bool focused) {
    auto& u=gLevelUp;
    const bool a=pressedA&&!u.lastA,b=pressedB&&!u.lastB;
    u.lastA=pressedA;u.lastB=pressedB;
    if(!gPlayerSession||gPlayerSession->saveBlocked||
       IsFo3LoadingVisible()||!gSceneReady||!focused||
       Fo3LevelUpCombatActive()) {
        if(u.panel.visible)u.panel.visible=false;
        u.owner=nullptr;
        return;
    }
    auto& player=gPlayerSession->player;
    if(!u.panel.visible) {
        if(!player.ExperienceProgress().ready||
           Fo3DialogueFocus()||Fo3ScriptMessageFocus()||Fo3PipboyFocus()||
           Fo3LockpickFocus()||Fo3BarterFocus()||
           GetFo3LootPanel().reference||
           fo3notify::Notifications().Size())return;
        const int budget=fo3levelup::SkillBudget(
            player.Snapshot().special,
            player.Definitions().levelSkillPointsBase,
            player.Definitions().levelSkillPointsInterval,
            player.Snapshot().pipboy.perks,player.Definitions().pipboy.perks);
        if(budget<0)return;
        u.owner=&player;u.original=player.Snapshot().skills;
        u.allocated=u.original;u.points=u.remaining=budget;
        u.page=Fo3LevelUpSession::Page::Skills;u.selectedPerk=0;
        u.cursor={};u.lastA=pressedA;u.lastB=pressedB;
        Fo3LevelUpRefresh();
        fo3audio::NamedSound("UILevelUpText");
        return;
    }
    if(u.owner!=&player) {u.panel.visible=false;u.owner=nullptr;return;}
    const auto prev=u.cursor.selected;
    u.cursor.Scroll(scroll,now,u.panel.choices.size());
    if(prev!=u.cursor.selected) {
        Fo3LevelUpRefresh();
        fo3audio::Scroll();
    }
    const size_t selected=u.cursor.selected;
    if(u.page==Fo3LevelUpSession::Page::Skills){
        // 13 playable skills, then Reset and Continue.
        if(selected<13) {
            const size_t slot=selected<12?selected:selected+1;
            if(a&&u.remaining>0&&u.allocated[slot]<100){
                ++u.allocated[slot];--u.remaining;Fo3LevelUpRefresh();
                fo3audio::NamedSound("UIMenuPrevNext");
            } else if(b&&u.allocated[slot]>u.original[slot]){
                --u.allocated[slot];++u.remaining;Fo3LevelUpRefresh();
                fo3audio::NamedSound("UIMenuPrevNext");
            }
        }else if(a&&selected==13){
            u.allocated=u.original;u.remaining=u.points;
            Fo3LevelUpRefresh();fo3audio::NamedSound("UIMenuOK");
        }else if(a&&selected==14&&u.remaining==0){
            u.page=Fo3LevelUpSession::Page::Perks;
            Fo3LevelUpRefresh();fo3audio::NamedSound("UIMenuOK");
        }
        return;
    }
    if(u.page==Fo3LevelUpSession::Page::Perks) {
        if(b) {u.page=Fo3LevelUpSession::Page::Skills;
            Fo3LevelUpRefresh();fo3audio::NamedSound("UIMenuOK");return;}
        if(!a||selected>=u.eligible.size())return;
        u.selectedPerk=u.eligible[selected];
        const auto special=player.Definitions().pipboy.formNames.find("intensetraining");
        if(special!=player.Definitions().pipboy.formNames.end()&&
           u.selectedPerk==special->second){
            u.page=Fo3LevelUpSession::Page::Intense;
            Fo3LevelUpRefresh();fo3audio::NamedSound("UIMenuOK");return;
        }
        if(player.CommitLevelUp(u.allocated,u.selectedPerk)){
            u.panel.visible=false;u.owner=nullptr;
            fo3audio::NamedSound("UIMenuOK");
        }
        return;
    }
    if(b){u.page=Fo3LevelUpSession::Page::Perks;
        Fo3LevelUpRefresh();fo3audio::NamedSound("UIMenuOK");return;}
    if(a&&selected<7&&player.CommitLevelUp(
           u.allocated,u.selectedPerk,int(selected))){
        u.panel.visible=false;u.owner=nullptr;
        fo3audio::NamedSound("UIMenuOK");
    }
}
