#pragma once
#include <algorithm>
#include <cmath>

namespace fqvk {
// Six small steps span the 0.90 presentation scale down to 0.75 for the world.
// Only completed GPU timestamps drive this controller. A CPU stall cannot lower it.
struct WorldResolution {
    int step=0;
    unsigned slow=0,fast=0;
    double mean=0,budget=0;
    float Ratio() const { return (36-step)/36.f; }
    void Observe(double gpuMs,double budgetMs,int submittedStep) {
        if(submittedStep!=step||!std::isfinite(gpuMs)||gpuMs<=0||gpuMs>200||
           !std::isfinite(budgetMs)||budgetMs<4||budgetMs>60)return;
        if(std::abs(budgetMs-budget)>1){mean=0;slow=fast=0;}
        budget=budgetMs;
        mean=mean?mean+(gpuMs-mean)*.125:gpuMs;
        slow=mean>budget*.98?slow+1:0;
        fast=mean<budget*.79?fast+1:0;
        if(slow>=10&&step<6){++step;slow=fast=0;mean=0;}
        if(fast>=60&&step>0){--step;slow=fast=0;mean=0;}
    }
};
}
