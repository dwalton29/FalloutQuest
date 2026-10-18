// Render-thread admission is separate from resident-world publication. Pausing
// must never reserve the asset lane or stop lifecycle/background preparation.
namespace {
fo3debug::Menu gFo3PauseMenu;
bool gFo3PauseMenuDown=false,gFo3PauseA=false,gFo3PauseB=false;
int gFo3PauseVertical=0,gFo3PauseHorizontal=0;
double gFo3PauseRepeatY=0,gFo3PauseRepeatX=0;
bool gFo3PauseNeutral=false,gFo3PauseSounds=false;
bool Fo3PauseMutationAllowed(){
    return gPlayerSession&&!gPlayerSession->saveBlocked&&gSceneReady&&
        !IsFo3LoadingVisible()&&!gSceneLoad.active&&!HasPendingFo3CellTransitionRequestQ74()&&!Fo3ResidentFrameBlocked()&&
        !Fo3ResidentLegacyStreamActive()&&!gFo3ResidentVisualPlan&&
        !gFo3ResidentInteractionAdmission&&!gFo3ResidentReferencePublicationPlan;
}
void UpdateFo3Pause(bool menu,bool a,bool b,float x,float y,double now,bool focused){
    const bool menuEdge=menu&&!gFo3PauseMenuDown;
    const bool aEdge=a&&!gFo3PauseA,bEdge=b&&!gFo3PauseB;
    gFo3PauseMenuDown=menu;gFo3PauseA=a;gFo3PauseB=b;
    if(!focused||!Fo3PauseMutationAllowed()){
        gFo3PauseMenu.Close();gFo3PauseOpen=false;gFo3PauseNeutral=false;return;
    }
    if(!gFo3PauseSounds){gFo3PauseSounds=true;
        for(const char* sound:{"UIMenuFocus","UIMenuOK","UIMenuCancel","UIMenuPrevNext"})fo3audio::PreloadSound(sound);
    }
    if(menuEdge){
        if(gFo3PauseOpen){gFo3PauseMenu.Close();gFo3PauseOpen=false;fo3audio::NamedSound("UIMenuCancel");}
        else {
            PauseFo3ResidentGameplayTime();gFo3PauseMenu.Open(gPlayerSession->player);gFo3PauseOpen=true;
            gFo3PauseNeutral=false;gFo3PauseVertical=gFo3PauseHorizontal=0;
            fo3audio::NamedSound("UIMenuOK");
        }
        return;
    }
    if(!gFo3PauseOpen)return;
    if(!gFo3PauseNeutral){if(!a&&!b&&std::fabs(x)<.25f&&std::fabs(y)<.25f)gFo3PauseNeutral=true;return;}
    auto step=[&](float value,int& previous,double& repeat){
        const int d=value>.65f?1:value<-.65f?-1:0;
        if(!d){previous=0;return 0;}
        if(d!=previous||now>=repeat){repeat=now+(d!=previous?.35:.12);previous=d;return d;}
        return 0;
    };
    const int vertical=step(y,gFo3PauseVertical,gFo3PauseRepeatY);
    const int horizontal=step(x,gFo3PauseHorizontal,gFo3PauseRepeatX);
    if(vertical){gFo3PauseMenu.Move(-vertical);fo3audio::NamedSound("UIMenuFocus");}
    if(horizontal){gFo3PauseMenu.Adjust(horizontal);fo3audio::NamedSound("UIMenuPrevNext");}
    if(bEdge){gFo3PauseMenu.Back();fo3audio::NamedSound("UIMenuCancel");}
    else if(aEdge){
        // A second admission check prevents a mutation during world handoff.
        const auto page=gFo3PauseMenu.page;
        if(Fo3PauseMutationAllowed()&&gFo3PauseMenu.Confirm()){
            if(page==fo3debug::Menu::Page::Special||page==fo3debug::Menu::Page::Perk)
                ResetFo3LevelUpDraftAfterDebug();
            FlushFo3PlayerState();
        }
        fo3audio::NamedSound("UIMenuOK");
    }
    gFo3PauseOpen=gFo3PauseMenu.panel.visible;
}
}
