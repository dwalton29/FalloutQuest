#include "fo3-rigid-world.h"
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofSpring2Constraint.h>
#include <BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h>
#include <BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h>
#include <BulletCollision/NarrowPhaseCollision/btPointCollector.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
namespace fo3rigid {
namespace {
btVector3 B(const V& v){return {v[0],v[1],v[2]};}
V A(const btVector3& v){return {float(v.x()),float(v.y()),float(v.z())};}
btTransform B(const M& m){btTransform t;t.setFromOpenGLMatrix(m.data());return t;}
M A(const btTransform& t){M m;t.getOpenGLMatrix(m.data());return m;}
bool Finite(const V& p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x);});}
bool Finite(const M& m){return std::all_of(m.begin(),m.end(),[](float x){return std::isfinite(x);});}
btVector3 Nearest(const btVector3& p,const btVector3& a,const btVector3& b,const btVector3& c){
    const auto ab=b-a,ac=c-a,ap=p-a;const float d1=ab.dot(ap),d2=ac.dot(ap);
    if(d1<=0&&d2<=0)return a;const auto bp=p-b;const float d3=ab.dot(bp),d4=ac.dot(bp);
    if(d3>=0&&d4<=d3)return b;const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
    const auto cp=p-c;const float d5=ab.dot(cp),d6=ac.dot(cp);if(d6>=0&&d5<=d6)return c;
    const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
    const float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
    const float denominator=va+vb+vc;if(std::abs(denominator)<1e-12f)return a;
    return a+ab*(vb/denominator)+ac*(vc/denominator);
}
struct SurfaceQuery:btTriangleCallback {
    btVector3 point,best,normal;float distance2;bool found=false;
    SurfaceQuery(btVector3 p,float radius):point(p),distance2(radius*radius){}
    void processTriangle(btVector3* t,int,int)override{
        const auto q=Nearest(point,t[0],t[1],t[2]);const float d=(q-point).length2();
        if(d<=distance2){found=true;distance2=d;best=q;normal=point-q;if(normal.length2()>1e-10f)normal.normalize();else{normal=(t[1]-t[0]).cross(t[2]-t[0]);if(normal.length2()>1e-10f)normal.normalize();else normal={0,1,0};}}
    }
};
struct Entry {
    btTransform principal=btTransform::getIdentity();
    std::unique_ptr<btTriangleMesh> mesh,visualMesh;
    std::unique_ptr<btBvhTriangleMeshShape> visualShape;
    std::vector<std::unique_ptr<btCollisionShape>> children;
    std::unique_ptr<btCollisionShape> shape;
    std::unique_ptr<btRigidBody> body;
};
}
struct World::Impl {
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher{&config};
    btDbvtBroadphase broad;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world{&dispatcher,&broad,&solver,&config};
    std::unordered_map<uint64_t,Entry> statics,bodies;
    struct Grip {uint64_t id=0;btTransform anchor,offset;std::unique_ptr<btGeneric6DofSpring2Constraint> constraint;};
    Grip grips[2];
    float accumulated=0;
    std::vector<std::pair<Entry*,btTransform>> previous;
    Impl(){world.setGravity({0,-9.80665f,0});world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_splitImpulse=true;}
    void Drop(int hand){auto& g=grips[hand];if(g.constraint)world.removeConstraint(g.constraint.get());g.constraint.reset();g.id=0;}
    ~Impl(){for(int h=0;h<2;++h)Drop(h);for(auto* map:{&bodies,&statics})for(auto& e:*map)world.removeRigidBody(e.second.body.get());}
};
World::World():impl(new Impl){}World::~World()=default;
World& Live(){static World instance;return instance;}
void World::RemoveStatic(uint64_t id){auto i=impl->statics.find(id);if(i==impl->statics.end())return;impl->world.removeRigidBody(i->second.body.get());impl->statics.erase(i);}
void World::ClearStatics(){for(auto& e:impl->statics)impl->world.removeRigidBody(e.second.body.get());impl->statics.clear();}
void World::Static(uint64_t id,const std::vector<Triangle>& triangles){
    RemoveStatic(id);if(triangles.empty())return;
    Entry e;e.mesh.reset(new btTriangleMesh);
    for(const auto& t:triangles)e.mesh->addTriangle(B(t.a),B(t.b),B(t.c),true);
    e.shape.reset(new btBvhTriangleMeshShape(e.mesh.get(),true));e.shape->setMargin(.001f);
    btRigidBody::btRigidBodyConstructionInfo info(0,nullptr,e.shape.get());e.body.reset(new btRigidBody(info));
    e.body->setFriction(.5f);impl->world.addRigidBody(e.body.get(),1,short(-1));impl->statics.emplace(id,std::move(e));
    // Newly published/moving support must wake resting nearby bodies.
    for(auto& b:impl->bodies)b.second.body->activate(true);
}
bool World::Has(uint64_t id)const{return impl->bodies.count(id)!=0;}
bool World::Add(uint64_t id,const Asset& asset,const M& pose){
    if(Has(id))return true;
    if(!id||asset.hulls.empty()||!Finite(pose)||!std::isfinite(asset.material.mass)||asset.material.mass<=0)return false;
    for(const auto& hull:asset.hulls)for(const auto& p:hull)if(!Finite(p))return false;
    for(const auto& t:asset.surface)if(!Finite(t.a)||!Finite(t.b)||!Finite(t.c))return false;
    const auto& properties=asset.material;
    for(float x:{properties.friction,properties.restitution,properties.linearDamping,properties.angularDamping})if(!std::isfinite(x))return false;
    Entry e;auto compound=std::make_unique<btCompoundShape>();
    const auto& m=asset.material;btVector3 inertia=B(m.inertia);
    if(m.tensor[0]>0&&m.tensor[4]>0&&m.tensor[8]>0){
        btMatrix3x3 tensor(m.tensor[0],m.tensor[1],m.tensor[2],m.tensor[3],m.tensor[4],m.tensor[5],m.tensor[6],m.tensor[7],m.tensor[8]),axes;
        tensor.diagonalize(axes,1e-6f,32);
        if(tensor[0][0]>0&&tensor[1][1]>0&&tensor[2][2]>0){e.principal.setBasis(axes);inertia={tensor[0][0],tensor[1][1],tensor[2][2]};}
    }
    for(const auto& points:asset.hulls){if(points.size()<4)continue;
        auto hull=std::make_unique<btConvexHullShape>();hull->setMargin(.001f);
        for(const auto& p:points)hull->addPoint(e.principal.invXform(B(p)),false);
        hull->recalcLocalAabb();compound->addChildShape(btTransform::getIdentity(),hull.get());e.children.push_back(std::move(hull));
    }
    if(e.children.empty())return false;
    if(!asset.surface.empty()){
        e.visualMesh.reset(new btTriangleMesh);
        for(const auto& t:asset.surface)e.visualMesh->addTriangle(B(t.a),B(t.b),B(t.c),true);
        e.visualShape.reset(new btBvhTriangleMeshShape(e.visualMesh.get(),true));
    }
    e.shape=std::move(compound);
    if(inertia.x()<=0||inertia.y()<=0||inertia.z()<=0)e.shape->calculateLocalInertia(m.mass,inertia);
    btCollisionShape* collision=e.children.size()==1?e.children[0].get():e.shape.get();
    btRigidBody::btRigidBodyConstructionInfo info(m.mass,nullptr,collision,inertia);
    info.m_friction=std::clamp(m.friction,0.f,2.f);info.m_restitution=std::clamp(m.restitution,0.f,1.f);
    info.m_linearDamping=std::clamp(m.linearDamping,0.f,1.f);info.m_angularDamping=std::clamp(m.angularDamping,0.f,1.f);
    e.body.reset(new btRigidBody(info));e.body->setWorldTransform(B(pose)*e.principal);e.body->setInterpolationWorldTransform(B(pose)*e.principal);
    btVector3 lo,hi;e.shape->getAabb(btTransform::getIdentity(),lo,hi);const auto half=(hi-lo)*.5f;
    const float thin=std::max(.002f,float(std::min({half.x(),half.y(),half.z()})));
    e.body->setCcdMotionThreshold(thin*.5f);e.body->setCcdSweptSphereRadius(thin*.8f);
    RemoveStatic(id);
    impl->world.addRigidBody(e.body.get(),2,short(-1));impl->bodies.emplace(id,std::move(e));return true;
}
void World::Remove(uint64_t id){auto i=impl->bodies.find(id);if(i==impl->bodies.end())return;for(int h=0;h<2;++h)if(impl->grips[h].id==id)impl->Drop(h);impl->world.removeRigidBody(i->second.body.get());impl->bodies.erase(i);}
void World::ClearBodies(){for(int h=0;h<2;++h)impl->Drop(h);for(auto& b:impl->bodies)impl->world.removeRigidBody(b.second.body.get());impl->bodies.clear();}
bool World::Pose(uint64_t id,M& pose,V* velocity,V* angular)const{auto i=impl->bodies.find(id);if(i==impl->bodies.end())return false;const auto& b=*i->second.body;pose=A(b.getWorldTransform()*i->second.principal.inverse());if(velocity)*velocity=A(b.getLinearVelocity());if(angular)*angular=A(b.getAngularVelocity());return true;}
void World::Place(uint64_t id,const M& pose,const V& velocity,const V& angular){auto i=impl->bodies.find(id);if(i==impl->bodies.end()||!Finite(pose))return;auto& b=*i->second.body;b.setWorldTransform(B(pose)*i->second.principal);b.setInterpolationWorldTransform(B(pose)*i->second.principal);b.setLinearVelocity(B(velocity));b.setAngularVelocity(B(angular));b.activate(true);impl->world.updateSingleAabb(&b);}
bool World::Closest(uint64_t id,const V& point,V& surface,V& normal,float& distance)const{
    const auto i=impl->bodies.find(id);if(i==impl->bodies.end()||!Finite(point))return false;
    if(i->second.visualShape){
        const auto model=i->second.body->getWorldTransform()*i->second.principal.inverse();const auto local=model.invXform(B(point));
        SurfaceQuery query(local,.2f);const btVector3 reach(.2f,.2f,.2f);i->second.visualShape->processAllTriangles(&query,local-reach,local+reach);
        if(query.found){surface=A(model*query.best);normal=A(model.getBasis()*query.normal);distance=std::sqrt(query.distance2);return true;}
    }
    btSphereShape probe(0);probe.setMargin(0);bool found=false;float best=1e30f;
    for(const auto& child:i->second.children){
        btVoronoiSimplexSolver simplex;btGjkEpaPenetrationDepthSolver penetration;
        btGjkPairDetector detector(&probe,static_cast<btConvexShape*>(child.get()),&simplex,&penetration);
        btDiscreteCollisionDetectorInterface::ClosestPointInput input;input.m_transformA=btTransform::getIdentity();input.m_transformA.setOrigin(B(point));input.m_transformB=i->second.body->getWorldTransform();
        btPointCollector result;detector.getClosestPoints(input,result,nullptr);
        if(result.m_hasResult&&result.m_distance<best){found=true;best=result.m_distance;surface=A(result.m_pointInWorld);normal=A(result.m_normalOnBInWorld);}
    }
    distance=best;return found;
}
bool Surface(const Asset& asset,const M& pose,const V& point,V& surface,V& normal,float& distance){
    if(!Finite(pose)||!Finite(point))return false;
    if(!asset.surface.empty()){
        const auto transform=B(pose);SurfaceQuery query(transform.invXform(B(point)),.2f);
        for(const auto& t:asset.surface){btVector3 triangle[3]{B(t.a),B(t.b),B(t.c)};query.processTriangle(triangle,0,0);}
        if(query.found){surface=A(transform*query.best);normal=A(transform.getBasis()*query.normal);distance=std::sqrt(query.distance2);return true;}
        return false;
    }
    btSphereShape probe(0);probe.setMargin(0);bool found=false;float best=1e30f;
    for(const auto& vertices:asset.hulls){btConvexHullShape hull;hull.setMargin(.001f);for(const auto& p:vertices)hull.addPoint(B(p),false);hull.recalcLocalAabb();
        btVoronoiSimplexSolver simplex;btGjkEpaPenetrationDepthSolver penetration;btGjkPairDetector detector(&probe,&hull,&simplex,&penetration);
        btDiscreteCollisionDetectorInterface::ClosestPointInput input;input.m_transformA=btTransform::getIdentity();input.m_transformA.setOrigin(B(point));input.m_transformB=B(pose);
        btPointCollector result;detector.getClosestPoints(input,result,nullptr);
        if(result.m_hasResult&&result.m_distance<best){found=true;best=result.m_distance;surface=A(result.m_pointInWorld);normal=A(result.m_normalOnBInWorld);}
    }distance=best;return found;
}
bool World::Intersects(uint64_t id,const V& point,float radius)const{
    const auto i=impl->bodies.find(id);if(i==impl->bodies.end())return false;
    if(i->second.visualShape){
        const auto model=i->second.body->getWorldTransform()*i->second.principal.inverse();const auto local=model.invXform(B(point));
        SurfaceQuery query(local,radius);const btVector3 reach(radius,radius,radius);
        i->second.visualShape->processAllTriangles(&query,local-reach,local+reach);return query.found;
    }
    const auto* bounds=i->second.body->getBroadphaseHandle();
    for(int a=0;a<3;++a)if(point[a]<bounds->m_aabbMin[a]-radius||point[a]>bounds->m_aabbMax[a]+radius)return false;
    V p,n;float d;return Closest(id,point,p,n,d)&&d<radius;
}
bool World::Grab(uint64_t id,int hand,const M& palm,const V& surface){
    if(hand<0||hand>1||!Has(id)||!Finite(palm)||Held(1-hand)==id)return false;
    impl->Drop(hand);auto& b=*impl->bodies.at(id).body;auto& g=impl->grips[hand];g.id=id;
    btTransform frame=B(palm);frame.setOrigin(B(surface));g.anchor=b.getWorldTransform().inverse()*frame;g.offset=B(palm).inverse()*frame;
    g.constraint.reset(new btGeneric6DofSpring2Constraint(b,g.anchor));
    g.constraint->setFrames(frame,g.anchor);
    // Solve the grip as a compliant attachment in the contact solver. The old
    // explicit force spring used dimensional damping=1 for every mass/inertia:
    // heavy props lagged, while small angular inertias rang like a pendulum.
    // Velocity-level correction removes residual velocity every tick and keeps
    // collision contacts authoritative. No kinematic teleport or centre snap.
    g.constraint->setLinearLowerLimit({0,0,0});g.constraint->setLinearUpperLimit({0,0,0});
    g.constraint->setAngularLowerLimit({0,0,0});g.constraint->setAngularUpperLimit({0,0,0});
    for(int axis=0;axis<6;++axis){
        g.constraint->setParam(BT_CONSTRAINT_STOP_ERP,.55f,axis);
        const float inverseEffectiveMass=axis<3?b.getInvMass():b.getInvInertiaDiagLocal()[axis-3];
        g.constraint->setParam(BT_CONSTRAINT_STOP_CFM,.0001f*inverseEffectiveMass,axis);
    }
    impl->world.addConstraint(g.constraint.get(),true);b.setActivationState(DISABLE_DEACTIVATION);return true;
}
void World::Target(int hand,const M& palm){if(hand<0||hand>1||!Finite(palm))return;auto& g=impl->grips[hand];if(!g.constraint)return;g.constraint->setFrames(B(palm)*g.offset,g.anchor);}
void World::Release(int hand,const V* velocity,const V* angular){if(hand<0||hand>1)return;auto id=Held(hand);impl->Drop(hand);auto i=impl->bodies.find(id);if(i==impl->bodies.end())return;auto& b=*i->second.body;b.forceActivationState(ACTIVE_TAG);b.activate(true);if(velocity)b.setLinearVelocity(B(*velocity));if(angular)b.setAngularVelocity(B(*angular));}
uint64_t World::Held(int hand)const{return hand>=0&&hand<2?impl->grips[hand].id:0;}
bool World::GripPoint(int hand,V& point)const{auto id=Held(hand);auto i=impl->bodies.find(id);if(i==impl->bodies.end())return false;point=A((i->second.body->getWorldTransform()*impl->grips[hand].anchor).getOrigin());return true;}
bool World::GripPalm(int hand,M& palm)const{auto id=Held(hand);auto i=impl->bodies.find(id);if(i==impl->bodies.end())return false;const auto& g=impl->grips[hand];palm=A(i->second.body->getWorldTransform()*g.anchor*g.offset.inverse());return true;}
void World::TargetBody(uint64_t id,const M& pose){for(auto& g:impl->grips)if(g.id==id&&g.constraint)g.constraint->setFrames(B(pose)*impl->bodies.at(id).principal*g.anchor,g.anchor);}
void World::Step(float seconds){
    if(!(seconds>0)||!std::isfinite(seconds))return;
    impl->accumulated+=std::min(seconds,.064f);constexpr float dt=1.f/120.f;
    while(impl->accumulated+1e-7f>=dt){
        impl->accumulated-=dt;
        auto& previous=impl->previous;previous.clear();
        for(auto& entry:impl->bodies)if(entry.second.body->isActive())previous.push_back({&entry.second,entry.second.body->getWorldTransform()});
        impl->world.stepSimulation(dt,0);
        // Bullet's swept sphere protects linear COM motion. Sweep each authored
        // convex child as well so a fast-spinning thin plate/weapon cannot rotate
        // through a floor between ticks, including compound bodies.
        for(const auto& state:previous){
            auto& body=*state.first->body;const auto from=state.second,to=body.getWorldTransform();
            const float rotation=from.getRotation().angleShortestPath(to.getRotation());
            if((to.getOrigin()-from.getOrigin()).length2()<1e-8f&&rotation<.001f)continue;
            struct Query:btCollisionWorld::ClosestConvexResultCallback{
                const btCollisionObject* self;
                btTransform from,to;
                Query(const btTransform& a,const btTransform& b,const btCollisionObject* object):ClosestConvexResultCallback(a.getOrigin(),b.getOrigin()),self(object),from(a),to(b){}
                btScalar addSingleResult(btCollisionWorld::LocalConvexResult& result,bool normalWorld)override{
                    const auto normal=normalWorld?result.m_hitNormalLocal:result.m_hitCollisionObject->getWorldTransform().getBasis()*result.m_hitNormalLocal;
                    auto delta=to.getRotation()*from.getRotation().inverse();if(delta.w()<0)delta=-delta;
                    const auto turn=delta.getAxis()*delta.getAngle();
                    const auto centre=from.getOrigin().lerp(to.getOrigin(),result.m_hitFraction);
                    // A resting/tangential or separating contact is not a new
                    // obstruction. Test the contact point's rotational motion,
                    // not just COM motion, so gentle lift/slide is never latched.
                    const auto motion=to.getOrigin()-from.getOrigin()+turn.cross(result.m_hitPointLocal-centre);
                    // Sub-contact-slop normal motion belongs to the discrete
                    // contact solver; using an epsilon near float noise here
                    // re-latches resting bodies on different CPUs/toolchains.
                    constexpr float inwardSlop=.0001f; // 0.1 mm, sweep allowance is 1 mm
                    if(motion.dot(normal)>=-inwardSlop)return 1;
                    return ClosestConvexResultCallback::addSingleResult(result,normalWorld);
                }
                bool needsCollision(btBroadphaseProxy* proxy)const override{return proxy->m_clientObject!=self&&ClosestConvexResultCallback::needsCollision(proxy);}
            } hit(from,to,&body);
            hit.m_collisionFilterGroup=2;hit.m_collisionFilterMask=3;
            const Entry* source=state.first;
            for(const auto& child:source->children)impl->world.convexSweepTest(static_cast<const btConvexShape*>(child.get()),from,to,hit,.001f);
            if(!hit.hasHit()||hit.m_closestHitFraction>=1)continue;
            const float fraction=std::max(0.f,float(hit.m_closestHitFraction)-.001f);
            btTransform safe(from.getRotation().slerp(to.getRotation(),fraction),from.getOrigin().lerp(to.getOrigin(),fraction));
            body.setWorldTransform(safe);body.setInterpolationWorldTransform(safe);
            auto velocity=body.getLinearVelocity();const float inward=velocity.dot(hit.m_hitNormalWorld);
            if(inward<0)body.setLinearVelocity(velocity-hit.m_hitNormalWorld*inward);
            if(rotation>.001f)body.setAngularVelocity(body.getAngularVelocity()*fraction);
            impl->world.updateSingleAabb(&body);
        }
    }
}

bool World::SweepCapsule(const V& from,const V& to,float radius,float height,V& resolved,V& normal)const{
    btCapsuleShape capsule(radius,std::max(0.f,height-2*radius));capsule.setMargin(.001f);
    btTransform a=btTransform::getIdentity(),b=a;a.setOrigin(B(from)+btVector3(0,height*.5f,0));b.setOrigin(B(to)+btVector3(0,height*.5f,0));
    btCollisionWorld::ClosestConvexResultCallback hit(a.getOrigin(),b.getOrigin());hit.m_collisionFilterGroup=4;hit.m_collisionFilterMask=3;
    impl->world.convexSweepTest(&capsule,a,b,hit,.001f);
    if(!hit.hasHit()){resolved=to;normal={};return false;}
    normal=A(hit.m_hitNormalWorld);resolved=A(B(from).lerp(B(to),std::max(0.f,float(hit.m_closestHitFraction)-.001f)));return true;
}
} // namespace fo3rigid
