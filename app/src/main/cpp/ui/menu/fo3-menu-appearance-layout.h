#pragma once
#include "fo3-menu-layout.h"
#include "ui/chargen/fo3-appearance-menu.h"

namespace fo3menu {
inline void BindAppearance(Layout& layout,const std::string& title,
                           const std::vector<fo3chargen::Row>& rows,size_t selected,unsigned page){
    const int background=layout.Find("RSM_Background");
    if(background<0)return;
    layout.Set(layout.Root(),"user0",double(page));
    layout.Set(layout.Root(),"user1","<");
    layout.Set(layout.Root(),"user2",">");

    const auto item=[&](const std::string& text,bool checked,bool highlighted,bool header){
        const int id=layout.Instantiate("RSM_list_item_template",background);
        layout.Set(id,"user0",double(page));
        layout.Set(id,"string",text);
        layout.Set(id,"_selected",checked?2.:0.);
        layout.Set(id,"mouseover",double(highlighted));
        layout.Set(id,"_is_header",double(header));
        return id;
    };
    item(title,false,false,true);

    const auto firstButton=std::find_if(rows.begin(),rows.end(),[](const auto& row){return row.Button();});
    const size_t rowCount=size_t(firstButton-rows.begin());
    const float top=layout.N(background,"_top_bound"),bottom=layout.N(background,"_bot_bound");
    const int sliderTemplate=layout.Instantiate("RSM_slider_option_template",background);
    const float sliderHeight=layout.N(sliderTemplate,"height");
    layout.Set(sliderTemplate,"visible",0.);
    const int header=layout.Find("RSM_list_item",background);
    const float listHeight=layout.N(header,"height");
    const auto height=[&](size_t i){return rows[i].Slider()?sliderHeight:listHeight;};

    size_t first=0;
    float needed=0;
    for(size_t i=0;i<rowCount&&i<=selected;++i){
        needed+=height(i);
        while(first<i&&needed>bottom-top)needed-=height(first++);
    }
    float y=top;
    size_t end=first;
    for(;end<rowCount&&y+height(end)<=bottom;++end){
        const auto& row=rows[end];
        if(row.Slider()){
            const int id=layout.Instantiate("RSM_slider_option_template",background);
            layout.Set(id,"user0",double(page));
            layout.Set(id,"string",row.label);
            layout.Set(id,"user1",double(row.current));
            layout.Set(id,"user2",double(row.minimum));
            layout.Set(id,"user3",double(row.maximum));
            layout.Set(id,"user5",row.value);
            layout.Set(id,"mouseover",double(end==selected));
            layout.Set(id,"y",y);
        }else{
            const int id=item(row.label,row.checked,end==selected,false);
            layout.Set(id,"y",y);
        }
        y+=height(end);
    }
    for(const auto& arrow:std::array<std::pair<const char*,bool>,2>{{
            {"RSM_scroll_up_target",first>0},{"RSM_scroll_down_target",end<rowCount}}}){
        layout.Set(arrow.first,"visible",double(arrow.second));
        layout.Set(arrow.first,"alpha",255.);
    }
    layout.Set("RSM_back_button","visible",0.);
    layout.Set("RSM_next_button","visible",0.);
    for(size_t i=rowCount;i<rows.size();++i){
        const auto* button=rows[i].kind==fo3chargen::Kind::Back?"RSM_back_button":"RSM_next_button";
        layout.Set(button,"visible",1.);
        layout.Set(button,"string",rows[i].label);
        layout.Set(button,"mouseover",double(i==selected));
        layout.Set(button,"_line_alpha",i==selected?255.:160.);
    }
}
}
