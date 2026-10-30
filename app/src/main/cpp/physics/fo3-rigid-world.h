#pragma once
#include "fo3-ragdoll-data.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
namespace fo3rigid {
using V=std::array<float,3>;
using M=std::array<float,16>;
inline M Identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
struct Triangle {V a,b,c;};
struct Material {
    float mass=1, friction=.5f, restitution=0, linearDamping=.1f, angularDamping=.05f;
    V inertia{};
    bool authored=false;
    std::array<float,9> tensor{};
};
struct Asset {std::vector<std::vector<V>> hulls; Material material;std::vector<Triangle> surface;};
// One render-thread-owned CPU world. IDs are runtime identities, never fake REFRs.
class World {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    World();~World();
    World(const World&)=delete;World& operator=(const World&)=delete;
    void Static(uint64_t id,const std::vector<Triangle>& triangles);
    void RemoveStatic(uint64_t id);
    void ClearStatics();
    bool Add(uint64_t id,const Asset& asset,const M& pose);
    bool Has(uint64_t id)const;
    void Remove(uint64_t id);
    void ClearBodies();
    bool Pose(uint64_t id,M& pose,V* velocity=nullptr,V* angular=nullptr)const;
    void Place(uint64_t id,const M& pose,const V& velocity={},const V& angular={});
    bool Closest(uint64_t id,const V& point,V& surface,V& normal,float& signedDistance)const;
    bool Intersects(uint64_t id,const V& point,float radius)const;
    bool Grab(uint64_t id,int hand,const M& palm,const V& surface);
    void Target(int hand,const M& palm);
    void Release(int hand,const V* velocity=nullptr,const V* angular=nullptr);
    bool GripPoint(int hand,V& point)const;
    bool GripPalm(int hand,M& palm)const;
    void TargetBody(uint64_t id,const M& pose);
    uint64_t Held(int hand)const;
    void MoveHeld(const M& previousApp,const M& currentApp,float seconds);
    bool AddRagdoll(uint32_t actor,const fo3rag::Definition&,const std::vector<M>& boneWorld,float unitsToMetres,const V& velocity={},const std::vector<float>& bodyScales={});
    void RemoveRagdoll(uint32_t actor);
    bool HasRagdoll(uint32_t actor)const;
    void SleepRagdoll(uint32_t actor);
    bool RagdollPose(uint32_t actor,std::vector<M>& bones,bool* sleeping=nullptr,std::vector<V>* linear=nullptr,std::vector<V>* angular=nullptr)const;
    bool SetRagdollMotion(uint32_t actor,const std::vector<V>& linear,const std::vector<V>& angular);
    std::vector<uint32_t> RagdollActors()const;
    void Step(float seconds);
    bool SweepCapsule(const V& fromFeet,const V& toFeet,float radius,float height,V& resolved,V& normal)const;
};
World& Live();
bool Surface(const Asset& asset,const M& pose,const V& point,V& surface,V& normal,float& distance);
constexpr uint64_t WeaponId(uint64_t id){return id | (uint64_t{1}<<63);}
} // namespace fo3rigid
