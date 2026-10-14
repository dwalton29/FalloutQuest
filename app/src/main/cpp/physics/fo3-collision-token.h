#pragma once
#include "fo3-collision-overlay.h"
#include <utility>

// Registry ownership is independent of live-world ownership. Destruction only
// discards the prepared/displaced token and is safe on the CPU worker too.
class Fo3CollisionSnapshotOwner {
public:
    Fo3CollisionSnapshotOwner()=default;
    explicit Fo3CollisionSnapshotOwner(uint64_t token):token_(token){}
    ~Fo3CollisionSnapshotOwner(){Reset();}
    Fo3CollisionSnapshotOwner(const Fo3CollisionSnapshotOwner&)=delete;
    Fo3CollisionSnapshotOwner& operator=(const Fo3CollisionSnapshotOwner&)=delete;
    Fo3CollisionSnapshotOwner(Fo3CollisionSnapshotOwner&& other)noexcept:token_(std::exchange(other.token_,0)){}
    Fo3CollisionSnapshotOwner& operator=(Fo3CollisionSnapshotOwner&& other)noexcept {
        if(this!=&other){Reset();token_=std::exchange(other.token_,0);}return *this;
    }
    uint64_t Get()const noexcept{return token_;}
    void Reset()noexcept{if(token_)DiscardFo3CollisionSnapshotQ1930(std::exchange(token_,0));}
private:
    uint64_t token_=0;
};
