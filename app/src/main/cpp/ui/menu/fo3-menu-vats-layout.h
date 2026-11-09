#pragma once
#include "fo3-menu-layout.h"
#include <cmath>

namespace fo3menu {
struct VatsReadout {
  std::string name;
  unsigned chance=0;
  float condition=1;
  bool selected=false;
  const char* alignment="down";
};

inline void HideVatsChrome(Layout& layout) {
  for(const auto* name:{"left_arrow","right_arrow","Select_button","accept_button","return_button",
                        "BodyPart_button","Target_button","separator","queued_actions",
                        "HitPoints","ActionPoints","EnemyHealth"})
    layout.Set(name,"visible",0.);
}

inline int BindVatsLimb(Layout& layout,const VatsReadout& readout) {
  HideVatsChrome(layout);
  const int row=layout.Instantiate("body_part_percent",layout.Find("GlowBranch"));
  layout.Set(row,"x",layout.width*.5);
  layout.Set(row,"y",layout.height*.5);
  const std::string align=readout.alignment;
  if(align=="left"||align=="right")layout.Set(row,"justify",align=="left"?0.:2.);
  else layout.Set(row,"justify",align);
  layout.Set(row,"alpha",readout.selected?255.:152.);
  layout.Set(row,"string",std::to_string(readout.chance)+"%");
  layout.Set(row,"_LimbName",readout.name);
  layout.Set(row,"_LimbNameVisible",double(readout.selected));
  layout.Set(row,"_MeterPercent",double(std::clamp(readout.condition,0.f,1.f)));
  // Use the authored limb meter as the shared alignment reference for both
  // text lines. Its position already follows the selected divider variant.
  const int meter=layout.Find("meter",row);
  const double justify=layout.N(meter,"justify");
  const double textX=layout.N(meter,"x")+layout.N(meter,"width")*justify*.5;
  for(const char* name:{"limb_name","chance_to_hit"}){
    const int text=layout.Find(name,row);
    layout.Set(text,"x",textX);layout.Set(text,"justify",justify);
  }
  return row;
}

inline int VatsText(Layout& layout,int parent,const std::string& text,double x,double y) {
  const int id=layout.Instantiate("template_justify_left_text",parent);
  layout.Set(id,"string",text);layout.Set(id,"x",x);layout.Set(id,"y",y);
  layout.Set(id,"visible",1.);layout.Set(id,"alpha",255.);
  return id;
}

inline void VatsMeter(Layout& layout,int parent,float available,float reserved) {
  const float width=224;
  const auto fill=[&](float fraction,float alpha){
    if(fraction<=0)return;
    const int meter=layout.Instantiate("template_meter",parent);
    layout.Set(meter,"filename","Interface\\VATS\\vats_bar.dds");
    layout.Set(meter,"tile",0.);
    layout.Set(meter,"x",44.);layout.Set(meter,"y",47.);
    layout.Set(meter,"width",double(width*std::clamp(fraction,0.f,1.f)));
    layout.Set(meter,"height",14.);
    layout.Set(meter,"alpha",double(alpha));
  };
  fill(1,55);
  fill(available,150);
  if(reserved>0)fill(available-reserved,255);
}

inline void BindVatsChrome(Layout& layout,const std::string& hpLabel,const std::string& apLabel,
                           float hp,float ap,float reserved,const std::string& target,float targetHealth,
                           const std::vector<std::string>& actions,const std::string& status,bool aiming) {
  layout.Set(layout.Root(),"alpha",255.);
  for(const auto* name:{"left_arrow","right_arrow","BodyPart_button","Target_button"})
    layout.Set(name,"visible",0.);
  const std::pair<const char*,const char*> buttons[]={
      {"Select_button","glow_general_button_rt.dds"},
      {"accept_button","glow_general_button_a.dds"},
      {"return_button","glow_general_button_b.dds"}};
  const double rightEdge=layout.width-70.;
  double buttonWidth=0;
  for(const auto& [name,texture]:buttons)
    buttonWidth=std::max(buttonWidth,double(layout.N(layout.Find(name),"width")));
  for(const auto& [name,texture]:buttons){
    const int button=layout.Find(name);
    layout.Set(button,"visible",double(aiming));
    // text_box.xml applies right justification to this anchor itself.
    layout.Set(button,"_x",rightEdge);
    layout.Set(button,"_fixedwidth",buttonWidth);
    const int icon=layout.Find("xbox_button",button);
    layout.Set(icon,"visible",1.);
    layout.Set(icon,"filename",texture);
  }
  layout.Set("accept_button","target",double(!actions.empty()));
  // The native menu creates these HUD-template children at runtime.
  const auto resource=[&](const char* name,const char* bracket,const std::string& label,
                          float value,float pending,double x){
    const int parent=layout.Find(name);
    layout.Set(parent,"locus",1.);layout.Set(parent,"x",x);
    layout.Set(parent,"y",double(layout.height-110));
    const int art=layout.Instantiate(bracket,parent);
    layout.Set(art,"x",0.);layout.Set(art,"y",-15.);
    VatsText(layout,parent,label,44,4);
    VatsMeter(layout,parent,value,pending);
  };
  resource("HitPoints","template_left_bracket",hpLabel,hp,0,26);
  resource("ActionPoints","template_right_bracket",apLabel,ap,reserved,layout.width-338);
  if(!target.empty()){
    const int enemy=layout.Find("EnemyHealth");
    layout.Set(enemy,"x",double((layout.width-350)*.5f));
    layout.Set(enemy,"y",double(layout.height-104));
    layout.Instantiate("template_enemy_health_bracket",enemy);
    const int label=VatsText(layout,enemy,target,44,-3);
    layout.Set(label,"font",7.);
    VatsMeter(layout,enemy,targetHealth,0);
  }
  const int list=layout.Find("queued_actions");
  layout.Set(list,"visible",double(!actions.empty()));
  layout.Set(list,"locus",1.);layout.Set(list,"alpha",255.);
  layout.Set(list,"x",rightEdge-15.);layout.Set(list,"height",double(actions.size()*34));
  for(size_t i=0;i<actions.size();++i){
    const int row=layout.Instantiate("queued_action",list);
    layout.Set(row,"listindex",double(i));layout.Set(row,"height",34.);
    layout.Set(row,"y",double(i*34));layout.Set(row,"locus",1.);layout.Set(row,"string",actions[i]);
  }
  if(!status.empty()){
    const int text=layout.Instantiate("template_aiming_text",layout.Find("GlowBranch"));
    layout.Set(text,"visible",1.);layout.Set(text,"font",7.);layout.Set(text,"justify",1.);
    layout.Set(text,"string",status);layout.Set(text,"x",layout.width*.5);
    layout.Set(text,"y",layout.height*.18);
  }
}
}
