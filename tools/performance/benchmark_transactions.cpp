// Host workload benchmark. Not a headset FPS measurement.
#include "player/fo3-player-state.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    fo3player::Catalog c;std::string error;
    if(argc==2) {
        if(!fo3player::LoadCatalog(argv[1],c,error)){std::cerr<<error;return 1;}
    } else {
        c.initial.baseHealth=100;c.initial.special.fill(5);
        for(uint32_t i=1;i<=10000;++i)c.initial.actors[i].cell=1;
    }
    fo3player::Player p(std::move(c));
    if(argc==2)p.RegisterOriginalActors();
    fo3pipdata::ResultScript script;script.source="RewardKarma 1";
    std::vector<double> times;
    for(int i=0;i<220;++i){
        const auto start=std::chrono::steady_clock::now();
        auto plan=p.PrepareDialogueTransaction(script,{},error);
        if(!plan||!p.CommitScriptTransaction(*plan)){std::cerr<<error;return 1;}
        plan.reset(); // include reclaiming old state, as the frame thread does
        const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        if(i>=20)times.push_back(us);
    }
    std::sort(times.begin(),times.end());
    std::cout<<"actors="<<p.Snapshot().actors.size()<<" transactionUs median="<<times[100]
             <<" p90="<<times[180]<<" p99="<<times[198]<<'\n';
    if(argc==2){
        std::vector<uint32_t> resident;
        for(const auto& t:p.Definitions().pipboy.targets)
            if(t.second.cell==0xa96u)resident.push_back(t.first);
        p.SetScriptPlayerLocation(0,0,0,0xa96u,0xa74u);
        p.SetScriptEventDiagnostic([](const std::string&){});
        times.clear();
        for(int i=0;i<220;++i){
            const auto start=std::chrono::steady_clock::now();
            p.PumpWorldAwareGameMode(.25,resident,[](std::unique_ptr<fo3player::ScriptTransactionPlan>){return false;});
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            if(i>=20)times.push_back(us);
        }
        std::sort(times.begin(),times.end());
        std::cout<<"originalGameModeTickUs resident="<<resident.size()<<" median="<<times[100]
                 <<" p90="<<times[180]<<" p99="<<times[198]<<'\n';
    }
}
