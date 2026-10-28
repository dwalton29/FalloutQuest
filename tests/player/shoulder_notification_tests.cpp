#include "world/interaction/fo3-shoulder-zone.h"
#include "ui/interaction/fo3-item-notifications.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    using namespace fo3shoulder;
    Zone z;z.ready=true;z.shoulder={.2f,1.4f,0};
    const Point palm{.34f,1.48f,.24f};
    assert(z.Contains(palm,false)); // natural hand-over-right-shoulder reach
    assert(z.Contains({.39f,1.58f,.28f},false)); // high/outward/rear remains forgiving
    assert(!z.Contains({.2f,1.4f,-.1f},true)); // face/chest side is rejected
    assert(!z.Contains({NAN,1.4f,.09f},false));
    assert(z.Contains({.54f,1.4f,.17f},false)); // enlarged horizontal access
    assert(z.Contains({.2f,1.8f,.17f},false)); // enlarged vertical access
    assert(!z.Contains({.64f,1.4f,.17f},false));
    assert(z.Contains({.64f,1.4f,.17f},true)); // exit hysteresis shell
    z.right={0,0,-1};z.rear={1,0,0};z.shoulder={1,1.4f,2};
    assert(z.Contains({1.24f,1.48f,1.86f},false)); // translated, 90-degree torso
    assert(!z.Contains({.95f,1.4f,2},true));
    z.ready=false;assert(!z.Contains({1.09f,1.4f,2},false));
    // Restore the original torso frame before testing the gesture state machine.
    z={};z.ready=true;z.shoulder={.2f,1.4f,0};
    Gesture gesture;
    assert(!gesture.Update(1,10,true,true,1.0f,z,palm,1.0));
    assert(gesture.inside && gesture.armed);
    assert(gesture.Update(1,10,true,true,0.0f,z,palm,1.01));
    gesture.Reset();
    assert(!gesture.Update(0,10,true,true,1.0f,z,palm,2.0));
    assert(!gesture.armed);

    using namespace fo3notify;
    assert(ItemText("10mm Pistol",1)=="10mm Pistol added");
    assert(ItemText("Bottle",3)=="3 Bottle(s) added");
    Queue q;q.Advance(100);q.Enqueue("first");q.Enqueue("second");
    auto rev=q.Revision();
    assert(q.Size()==2 && std::strcmp(q.Text(),"first")==0 && q.Alpha()==1);
    q.Advance(101.9);assert(q.Alpha()==1);
    q.Advance(102.25);assert(std::fabs(q.Alpha()-.5)<1e-5);
    // Both eyes read identical logical state; they cannot advance the queue.
    for(int eye=0;eye<2;++eye) {assert(q.Revision()==rev);assert(q.Size()==2);assert(q.Alpha()==.5);}
    q.Advance(102.5);assert(q.Size()==1 && std::strcmp(q.Text(),"second")==0 && q.Alpha()==1);
    q.Advance(105);assert(!q.Size() && !q.Text()[0]);
    q.Advance(110);for(int i=0;i<40;++i)q.Enqueue(std::to_string(i));
    assert(q.Size()==Queue::Capacity && std::strcmp(q.Text(),"24")==0);
    q.Advance(151);assert(q.Size()==0); // bounded backlog expires after suspension
    q.Enqueue("clear on transition");q.Clear();assert(q.Size()==0);
    Queue xpQueue;xpQueue.Advance(198);
    const ExperienceMeter vanilla{true,199,300,1,0,200};
    xpQueue.EnqueueExperience("+101 XP\nLEVEL UP","UILevelUp",vanilla);
    assert(std::strcmp(xpQueue.Text(),"+101 XP\nLEVEL UP")==0);
    assert(xpQueue.CurrentXP()&&xpQueue.CurrentXP()->total==300);
    assert(Queue::Fraction(0,0,200)==0.f);
    assert(Queue::Fraction(100,0,200)==.5f);
    assert(Queue::Fraction(300,0,200)==1.f);
    assert(Queue::Fraction(400,200,550)>0.f);
    assert(!xpQueue.Quest()&&xpQueue.TakeSound()=="UILevelUp"&&
           xpQueue.TakeSound().empty());
    xpQueue.Advance(200.5);
    assert(xpQueue.Size()==0&&!xpQueue.CurrentXP());
    xpQueue.Enqueue("Bottle added");
    assert(!xpQueue.CurrentXP()); // Reused slots cannot retain old meter art
    xpQueue.Clear();
    xpQueue.EnqueueExperience("+5 XP","UIPopUpExperienceUp",
                              {true,10,9,1,0,200});
    assert(!xpQueue.CurrentXP()); // Declining XP is invalid
    xpQueue.EnqueueExperience("+5 XP","UIPopUpExperienceUp",
                              {true,2,3,1,200,200});
    xpQueue.Advance(205);
    assert(!xpQueue.CurrentXP());

    q.Advance(200);q.EnqueueQuest("Quest added\nAuthored title","UIPopUpQuestNew");
    assert(q.Quest()&&q.Alpha()==0&&q.TakeSound()=="UIPopUpQuestNew");
    assert(q.TakeSound().empty());q.Advance(201);assert(q.Alpha()==.5f);
    const auto pausedRevision=q.Revision();q.Advance(230,false);assert(q.Revision()==pausedRevision&&q.Alpha()==.5f);
    q.Advance(231);assert(q.Alpha()==1);q.Advance(235);assert(q.Alpha()==1);
    const double end=229+q.Duration();q.Advance(end-1);assert(q.Alpha()==.5f);q.Advance(end+.001);assert(q.Size()==0);
    assert(Queue::CharacterAlpha(1,0)==.5f&&Queue::CharacterAlpha(1,1)==0);
    assert(Queue::CharacterAlpha(3,1)==1&&Queue::CharacterAlpha(8,1)==.5f);
    std::cout<<"Shoulder geometry and notification queue regressions passed\n";
}
