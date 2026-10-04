#include "world/interaction/fo3-shoulder-zone.h"
#include "ui/interaction/fo3-item-notifications.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    using namespace fo3shoulder;
    Zone z;z.ready=true;z.shoulder={.2f,1.4f,0};
    const Point palm{.2f,1.4f,.09f};
    assert(z.Contains(palm,false));
    assert(!z.Contains({.2f,1.4f,-.1f},true)); // beside face / front of shoulder
    assert(!z.Contains({NAN,1.4f,.09f},false));
    assert(!z.Contains({.395f,1.4f,.09f},false));
    assert(z.Contains({.395f,1.4f,.09f},true)); // exit hysteresis shell
    z.right={0,0,-1};z.rear={1,0,0};z.shoulder={1,1.4f,2};
    assert(z.Contains({1.09f,1.4f,2},false)); // translated, 90-degree torso
    assert(!z.Contains({.9f,1.4f,2},true));
    z.ready=false;assert(!z.Contains({1.09f,1.4f,2},false));
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
    std::cout<<"Shoulder geometry and notification queue regressions passed\n";
}
