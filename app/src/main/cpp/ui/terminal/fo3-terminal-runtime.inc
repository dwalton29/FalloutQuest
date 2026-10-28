#ifndef FO3_TERMINAL_HEADLESS
#include "fo3-terminal-renderer.h"
#endif
fo3terminal::Session gTerminal;
bool gTerminalInputLatch=false;
std::shared_ptr<const fo3terminal::Surface> gTerminalScreen;
uint32_t gTerminalCell=0;
bool Fo3TerminalFocus(){return gTerminal.reference!=0;}
bool Fo3TerminalInputBlocked(){return Fo3TerminalFocus()||gTerminalInputLatch;}
void CancelFo3Terminal(){if(gTerminal.note&&fo3audio::PlayingNote()==gTerminal.note)fo3audio::Note(0,{});gTerminal.note=0;gTerminal.reference=0;gTerminalScreen.reset();fo3audio::MenuLoop("");}
bool BeginFo3Terminal(uint32_t reference){
  if(!gPlayerSession||gPlayerSession->saveBlocked||Fo3ResidentFrameBlocked())return false;
  auto& p=gPlayerSession->player;const auto ref=p.Definitions().references.find(reference);
  if(ref==p.Definitions().references.end())return false;
  const auto terminal=p.Definitions().pipboy.terminals.find(ref->second.base);
  if(terminal==p.Definitions().pipboy.terminals.end()||!terminal->second.valid)return false;
  std::shared_ptr<const fo3terminal::Surface> screen;
  for(const auto& object:gObjects)if(object.refFormId==reference&&object.terminalScreen&&
      (!screen||object.terminalScreen->width*object.terminalScreen->height>screen->width*screen->height))screen=object.terminalScreen;
  if(!screen){fo3notify::Notifications().Enqueue("Terminal screen mesh is unavailable.");return false;}
  // Honor authored OnActivate scripts before opening their default interface.
  if(p.HasReferenceEvent(reference,"OnActivate")){
    std::string error;auto event=DispatchFo3ScriptedActivation(reference,fo3player::PlayerRef,error);
    if(event!=fo3player::ScriptEventResult::Executed){if(!error.empty())fo3notify::Notifications().Enqueue(error);return false;}
  }
  fo3terminalui::Invalidate();
  gTerminalScreen=std::move(screen);gTerminalCell=gCurrentCellFormId;gTerminalInputLatch=true;
  p.EndContainerInteraction();ResetFo3Pipboy();gTerminal.Open(p,reference,0);
  fo3audio::MenuLoop("UIHackingFanHumLP");fo3audio::NamedSound("UIHackingCharEnter");FlushFo3PlayerState();return true;
}
void RenderFo3Terminal(const float* mvp){
  if(!gTerminalScreen||!gPlayerSession)return;
  const auto& p=gPlayerSession->player;const auto t=p.Definitions().pipboy.terminals.find(gTerminal.menu);
  fo3terminalui::Render(mvp,*gTerminalScreen,gTerminal.display,t!=p.Definitions().pipboy.terminals.end()&&(t->second.flags&4),!gTerminal.image.empty());
}
void UpdateFo3Terminal(double now,float x,float y,bool a,bool b,bool focused,float trigger=0,bool gripsDown=false){
  fo3terminalui::Advance();fo3terminalui::AdvanceImage(Fo3TerminalFocus()?gTerminal.image:std::string{});
  if(gPlayerSession){uint32_t back=0;while(gPlayerSession->player.PollTerminalBack(back))if(back==gTerminal.reference){gTerminal.Back(gPlayerSession->player);if(!Fo3TerminalFocus())CancelFo3Terminal();}}
  if(!Fo3TerminalFocus()){if(focused&&!a&&!b&&std::fabs(x)<.2f&&std::fabs(y)<.2f&&trigger<.1f&&!gripsDown)gTerminalInputLatch=false;return;}
  if(fo3terminalui::failed){fo3notify::Notifications().Enqueue("Terminal assets missing: Fixedsys font or Menus/FalloutDict.txt.");CancelFo3Terminal();return;}
  if(!focused||!gPlayerSession||gPlayerSession->saveBlocked||IsFo3LoadingVisible()||gCurrentCellFormId!=gTerminalCell){CancelFo3Terminal();return;}
  auto& p=gPlayerSession->player;bool disabled=false;std::string error;
  if(!p.ReferenceDisabled(gTerminal.reference,disabled,error)||disabled){CancelFo3Terminal();return;}
  const auto position=fo3terminal::Point{gQ210Head[0],gQ210Head[1],gQ210Head[2]};
  if(gTerminalScreen&&fo3terminal::Dot(fo3terminal::Sub(position,gTerminalScreen->origin),fo3terminal::Sub(position,gTerminalScreen->origin))>16){CancelFo3Terminal();return;}
  const bool pressA=a&&!gTerminal.lastA,pressB=b&&!gTerminal.lastB;gTerminal.lastA=a;gTerminal.lastB=b;
  if(!gTerminal.released){gTerminal.released=!a&&!b&&std::fabs(x)<.2f&&std::fabs(y)<.2f;return;}
  if(pressB){if(gTerminal.note&&fo3audio::PlayingNote()==gTerminal.note)fo3audio::Note(0,{});gTerminal.Back(p);if(!Fo3TerminalFocus())CancelFo3Terminal();return;}
  using Page=fo3terminal::Session::Page;
  if(gTerminal.page==Page::Initializing){
    if(fo3terminalui::failed){gTerminal.page=Page::Locked;gTerminal.result="Original terminal font or Menus/FalloutDict.txt is missing.";gTerminal.Refresh(p);}
    else if(fo3terminalui::ready&&!gTerminal.BeginHack(p,fo3terminalui::assets.dictionary,gLockpickRandom())){
      gTerminal.page=Page::Locked;gTerminal.result="Original hacking dictionary has no words for this terminal.";gTerminal.Refresh(p);}
    return;
  }
  const bool moved=(std::fabs(x)>.5f||std::fabs(y)>.5f)&&now>=gTerminal.repeat;
  if(moved){gTerminal.repeat=now+.16;fo3audio::NamedSound("UIHackingCharScroll");
    if(gTerminal.page==Page::Hacking){if(std::fabs(x)>std::fabs(y))gTerminal.hacking.Move(x>0?1:-1,0);else gTerminal.hacking.Move(0,y>0?-1:1);}
    else if(gTerminal.page==Page::Menu&&!gTerminal.entries.empty()){
      if(std::fabs(x)>std::fabs(y)){gTerminal.followSelection=false;if(x<0&&gTerminal.scroll)gTerminal.scroll--;else if(x>0)gTerminal.scroll++;}
      else {gTerminal.followSelection=true;const int size=int(gTerminal.entries.size());gTerminal.selected=size_t((int(gTerminal.selected)+(y>0?-1:1)+size)%size);}}
    else if(gTerminal.page==Page::Text){if(y>0&&gTerminal.scroll)gTerminal.scroll--;else if(y<0)gTerminal.scroll++;}
    gTerminal.Refresh(p);
  }else if(std::fabs(x)<.2f&&std::fabs(y)<.2f)gTerminal.repeat=0;
  if(p.Revision()!=gTerminal.playerRevision)gTerminal.Refresh(p);
  if(!pressA)return;
  fo3audio::NamedSound("UIHackingCharEnter");
  if(gTerminal.page==Page::Text){if(gTerminal.note&&fo3audio::PlayingNote()==gTerminal.note)fo3audio::Note(0,{});gTerminal.Back(p);return;}
  if(gTerminal.page==Page::Hacking){
    auto& game=gTerminal.hacking;const auto span=game.Span(game.cursor);
    const auto chosen=game.dump.substr(size_t(span.first),size_t(span.second-span.first+1));
    const auto event=game.Select();using E=fo3hacking::Game::Event;
    if(event==E::None)return;
    gTerminal.log.push_back(">"+chosen);
    if(event==E::Granted){if(p.CompleteTerminalHack(gTerminal.reference)){
      fo3audio::NamedSound("UIHackingPassGood");FlushFo3PlayerState();gTerminal.page=Page::Menu;gTerminal.selected=0;}
      else {gTerminal.page=Page::Locked;gTerminal.result="Terminal access changed.";}}
    if(event==E::Denied||event==E::Lockout){fo3audio::NamedSound("UIHackingPassBad");gTerminal.log.push_back(gTerminal.String(p,"sHackingDenied","Entry denied"));
      gTerminal.log.push_back(std::to_string(game.likeness)+"/"+std::to_string(game.words[game.password].text.size())+" "+gTerminal.String(p,"sHackingCorrect","correct"));}
    if(event==E::Lockout){p.FailTerminalHack(gTerminal.reference);FlushFo3PlayerState();gTerminal.page=Page::Locked;
      gTerminal.result=gTerminal.String(p,"sHackingLockout3","TERMINAL LOCKED")+"\n"+gTerminal.String(p,"sHackingLockout4","PLEASE CONTACT AN ADMINISTRATOR");}
    if(event==E::Dud)gTerminal.log.push_back(gTerminal.String(p,"sHackingDudRemoved","Dud removed."));
    if(event==E::Reset)gTerminal.log.push_back(gTerminal.String(p,"sHackingToleranceReset1","Allowance")+" "+gTerminal.String(p,"sHackingToleranceReset2","replenished."));
    gTerminal.Refresh(p);return;
  }
  if(gTerminal.page!=Page::Menu||gTerminal.entries.empty())return;
  const size_t index=gTerminal.entries[gTerminal.selected];
  const auto& entry=p.Definitions().pipboy.terminals.at(gTerminal.menu).entries[index];
  auto plan=p.PrepareTerminalTransaction(gTerminal.reference,gTerminal.menu,index,error,fo3player::ScriptProgramCapture::EnteredPrograms);
  if(!plan){gTerminal.result=error;gTerminal.page=Page::Text;gTerminal.scroll=0;gTerminal.Refresh(p);return;}
  if(plan->HasReferenceChanges()){
    // The resident world exchange owns focus while publishing. Close first so
    // its admission checks see an idle interaction. Its own release barrier
    // owns held controls while active; the terminal latch must not deadlock it.
    CancelFo3Terminal();gTerminalInputLatch=false;
    const auto ticket=BeginFo3ResidentPreparedWorkflow({},std::move(plan));
    if(ticket){gFo3ResidentGameplayTicket=ticket;FlushFo3PlayerState();}
    else {gTerminalInputLatch=true;fo3notify::Notifications().Enqueue("Terminal action could not reserve the resident scene.");}return;
  }
  if(!p.CommitScriptTransaction(*plan)){gTerminal.result="Terminal state changed; select the entry again.";gTerminal.page=Page::Text;gTerminal.Refresh(p);return;}
  FlushFo3PlayerState();
  uint32_t back=0;bool goBack=false;while(p.PollTerminalBack(back))goBack|=back==gTerminal.reference;
  if(goBack){gTerminal.Back(p);if(!Fo3TerminalFocus())CancelFo3Terminal();return;}
  if(entry.submenu){
    if(gTerminal.parents.size()>=32||!p.Definitions().pipboy.terminals.count(entry.submenu)){gTerminal.result="Terminal submenu unavailable.";gTerminal.page=Page::Text;}
    else {gTerminal.parents.push_back(gTerminal.menu);gTerminal.menu=entry.submenu;gTerminal.selected=0;gTerminal.scroll=0;gTerminal.page=Page::Menu;}
  }else if(!entry.result.empty()||entry.note){
    gTerminal.result=entry.result;if(entry.note){const auto note=p.Definitions().pipboy.notes.find(entry.note);
      if(note!=p.Definitions().pipboy.notes.end()){
        gTerminal.result+="\n"+note->second.text;gTerminal.image=note->second.image;gTerminal.note=entry.note;
        auto paths=fo3pipdata::NoteAudio(p.Definitions().pipboy,entry.note);if(!paths.empty())fo3audio::Note(entry.note,paths);
      }}
    gTerminal.page=Page::Text;gTerminal.scroll=0;
  }
  // ANAM Force Redraw re-evaluates CTDAs immediately; even other entries are
  // refreshed after action state changes instead of retaining stale choices.
  gTerminal.Refresh(p);
}
