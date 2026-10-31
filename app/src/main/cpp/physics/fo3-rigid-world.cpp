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
    bool ragdoll=false;float ragdollScale=1,ragdollRadius=0,ragdollThinRadius=0;
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
    struct Grip {uint64_t id=0;btTransform anchor,offset,target;btVector3 carryVelocity{0,0,0};std::unique_ptr<btGeneric6DofSpring2Constraint> constraint;};
    struct Ragdoll {float quietSeconds=0;std::vector<uint64_t> bodies;std::vector<std::unique_ptr<btTypedConstraint>> joints;};
    std::unordered_map<uint32_t,Ragdoll> ragdolls;
    std::vector<uint32_t> activeRagdolls;
    std::vector<const btCollisionObject*> restingContacts;
    Grip grips[2];
    float accumulated=0;
    std::vector<std::pair<Entry*,btTransform>> previous;
    Impl(){world.setGravity({0,-9.80665f,0});world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_splitImpulse=true;}
    void ClipMotion(Entry& entry,const btTransform& from,const btTransform& to,const btCollisionObject* carriedCompanion=nullptr){
        auto& body=*entry.body;
        const float rotation=from.getRotation().angleShortestPath(to.getRotation());
        if((to.getOrigin()-from.getOrigin()).length2()<1e-8f&&rotation<.001f)return;
        struct Query:btCollisionWorld::ClosestConvexResultCallback{
            const btCollisionObject* self;const btCollisionObject* companion;
            btTransform from,to;
            Query(const btTransform& a,const btTransform& b,const btCollisionObject* object,const btCollisionObject* other):ClosestConvexResultCallback(a.getOrigin(),b.getOrigin()),self(object),companion(other),from(a),to(b){}
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
            bool needsCollision(btBroadphaseProxy* proxy)const override{return proxy->m_clientObject!=self&&proxy->m_clientObject!=companion&&ClosestConvexResultCallback::needsCollision(proxy);}
        } hit(from,to,&body,carriedCompanion);
        hit.m_collisionFilterGroup=2;hit.m_collisionFilterMask=3;
        const Entry* source=&entry;
        for(const auto& child:source->children)world.convexSweepTest(static_cast<const btConvexShape*>(child.get()),from,to,hit,.001f);
        if(!hit.hasHit()||hit.m_closestHitFraction>=1)return;
        const float fraction=std::max(0.f,float(hit.m_closestHitFraction)-.001f);
        btTransform safe(from.getRotation().slerp(to.getRotation(),fraction),from.getOrigin().lerp(to.getOrigin(),fraction));
        // A blocked rotational component must not discard safe translation
        // along the surface. Sweep that remaining slide with fixed rotation
        // so a second wall/corner still blocks it.
        auto slide=to.getOrigin()-safe.getOrigin();
        slide-=hit.m_hitNormalWorld*std::min(btScalar(0),slide.dot(hit.m_hitNormalWorld));
        for(int plane=0;plane<3&&slide.length2()>1e-10f;++plane){
            auto end=safe;end.setOrigin(safe.getOrigin()+slide);
            Query next(safe,end,&body,carriedCompanion);next.m_collisionFilterGroup=2;next.m_collisionFilterMask=3;
            for(const auto& child:source->children)world.convexSweepTest(static_cast<const btConvexShape*>(child.get()),safe,end,next,.001f);
            if(!next.hasHit()){safe=end;break;}
            safe.setOrigin(safe.getOrigin()+slide*std::max(0.f,float(next.m_closestHitFraction)-.001f));
            slide=end.getOrigin()-safe.getOrigin();
            slide-=next.m_hitNormalWorld*std::min(btScalar(0),slide.dot(next.m_hitNormalWorld));
        }
        body.setWorldTransform(safe);body.setInterpolationWorldTransform(safe);
        auto velocity=body.getLinearVelocity();const float inward=velocity.dot(hit.m_hitNormalWorld);
        if(inward<0)body.setLinearVelocity(velocity-hit.m_hitNormalWorld*inward);
        if(rotation>.001f)body.setAngularVelocity(body.getAngularVelocity()*fraction);
        world.updateSingleAabb(&body);
    }
    void WakeBounds(btVector3 lo,btVector3 hi){lo-=btVector3(.1f,.1f,.1f);hi+=btVector3(.1f,.1f,.1f);for(auto& b:bodies){btVector3 a,z;b.second.body->getAabb(a,z);if(TestAabbAgainstAabb2(lo,hi,a,z))b.second.body->activate(true);}}
    void Drop(int hand){auto& g=grips[hand];if(g.constraint)world.removeConstraint(g.constraint.get());g.constraint.reset();g.id=0;g.carryVelocity={0,0,0};}
    ~Impl(){for(auto& r:ragdolls)for(auto& j:r.second.joints)world.removeConstraint(j.get());ragdolls.clear();for(int h=0;h<2;++h)Drop(h);for(auto* map:{&bodies,&statics})for(auto& e:*map)world.removeRigidBody(e.second.body.get());}
};
World::World():impl(new Impl){}World::~World()=default;
World& Live(){static World instance;return instance;}
void World::RemoveStatic(uint64_t id){auto i=impl->statics.find(id);if(i==impl->statics.end())return;btVector3 lo,hi;i->second.body->getAabb(lo,hi);impl->WakeBounds(lo,hi);impl->world.removeRigidBody(i->second.body.get());impl->statics.erase(i);}
void World::ClearStatics(){for(auto& e:impl->statics)impl->world.removeRigidBody(e.second.body.get());impl->statics.clear();}
void World::Static(uint64_t id,const std::vector<Triangle>& triangles){
    RemoveStatic(id);if(triangles.empty())return;
    Entry e;e.mesh.reset(new btTriangleMesh);
    for(const auto& t:triangles)e.mesh->addTriangle(B(t.a),B(t.b),B(t.c),true);
    e.shape.reset(new btBvhTriangleMeshShape(e.mesh.get(),true));e.shape->setMargin(.001f);
    btRigidBody::btRigidBodyConstructionInfo info(0,nullptr,e.shape.get());e.body.reset(new btRigidBody(info));
    e.body->setFriction(.5f);impl->world.addRigidBody(e.body.get(),1,short(-1));impl->statics.emplace(id,std::move(e));
    // Newly published/moving support must wake resting nearby bodies.
    btVector3 lo,hi;impl->statics.at(id).body->getAabb(lo,hi);impl->WakeBounds(lo,hi);
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
void World::ClearBodies(){for(auto& rag:impl->ragdolls)for(auto& j:rag.second.joints)impl->world.removeConstraint(j.get());impl->ragdolls.clear();for(int h=0;h<2;++h)impl->Drop(h);for(auto& b:impl->bodies)impl->world.removeRigidBody(b.second.body.get());impl->bodies.clear();}
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
    g.target=frame;g.constraint->setFrames(frame,g.anchor);
    // Velocity servos damp the grab without a mass-dependent explicit spring.
    // Finite mass/inertia-scaled effort lets collision contacts win when the
    // tracked hand is beyond a wall, instead of an unlimited locked attachment
    // fighting the contact solver and eventually punching through a corner.
    g.constraint->setLinearLowerLimit({1,1,1});g.constraint->setLinearUpperLimit({-1,-1,-1});
    g.constraint->setAngularLowerLimit({1,1,1});g.constraint->setAngularUpperLimit({-1,-1,-1});
    const float mass=1.f/b.getInvMass();
    for(int axis=0;axis<6;++axis){
        g.constraint->enableMotor(axis,true);g.constraint->setServo(axis,true);g.constraint->setServoTarget(axis,0);
        g.constraint->setTargetVelocity(axis,axis<3?4.f:20.f);
        g.constraint->setParam(BT_CONSTRAINT_ERP,.55f,axis);
        const float inverseEffectiveMass=axis<3?b.getInvMass():b.getInvInertiaDiagLocal()[axis-3];
        g.constraint->setParam(BT_CONSTRAINT_CFM,.0001f*inverseEffectiveMass,axis);
        const float effortMass=axis<3?mass:(inverseEffectiveMass>0?1.f/inverseEffectiveMass:0.f)+mass*g.anchor.getOrigin().length2();
        g.constraint->setMaxMotorForce(axis,effortMass*(axis<3?120.f:400.f));
    }
    impl->world.addConstraint(g.constraint.get(),true);b.setActivationState(DISABLE_DEACTIVATION);return true;
}
void World::Target(int hand,const M& palm){if(hand<0||hand>1||!Finite(palm))return;auto& g=impl->grips[hand];if(!g.constraint)return;g.target=B(palm)*g.offset;}
void World::Release(int hand,const V* velocity,const V* angular){if(hand<0||hand>1)return;auto id=Held(hand);const auto carry=impl->grips[hand].carryVelocity;impl->Drop(hand);auto i=impl->bodies.find(id);if(i==impl->bodies.end())return;auto& b=*i->second.body;b.forceActivationState(ACTIVE_TAG);b.activate(true);if(velocity)b.setLinearVelocity(B(*velocity));else b.setLinearVelocity(b.getLinearVelocity()+carry);if(angular)b.setAngularVelocity(B(*angular));}
uint64_t World::Held(int hand)const{return hand>=0&&hand<2?impl->grips[hand].id:0;}
bool World::GripPoint(int hand,V& point)const{auto id=Held(hand);auto i=impl->bodies.find(id);if(i==impl->bodies.end())return false;point=A((i->second.body->getWorldTransform()*impl->grips[hand].anchor).getOrigin());return true;}
bool World::GripPalm(int hand,M& palm)const{auto id=Held(hand);auto i=impl->bodies.find(id);if(i==impl->bodies.end())return false;const auto& g=impl->grips[hand];palm=A(i->second.body->getWorldTransform()*g.anchor*g.offset.inverse());return true;}
void World::TargetBody(uint64_t id,const M& pose){if(!Finite(pose))return;for(auto& g:impl->grips)if(g.id==id&&g.constraint)g.target=B(pose)*impl->bodies.at(id).principal*g.anchor;}
// Artificial locomotion carries attached objects; controller-relative motion
// still goes through the finite-effort grip solver. Sweep the same convex hulls
// as normal integration, so transport cannot pull a held gun through a wall.
void World::MoveHeld(const M& previousApp,const M& currentApp,float seconds){
    if(!Finite(previousApp)||!Finite(currentApp)||!std::isfinite(seconds)||seconds<=0)return;
    if(previousApp==currentApp){for(auto& grip:impl->grips)grip.carryVelocity={0,0,0};return;}
    const auto delta=B(currentApp)*B(previousApp).inverse();
    for(int hand=0;hand<2;++hand){
        auto& grip=impl->grips[hand];
        auto found=impl->bodies.find(grip.id);if(!grip.constraint||found==impl->bodies.end())continue;
        auto& entry=found->second;auto& body=*entry.body;
        const auto from=body.getWorldTransform(),to=delta*from;
        body.setWorldTransform(to);body.setInterpolationWorldTransform(to);
        body.setLinearVelocity(delta.getBasis()*body.getLinearVelocity());
        body.setAngularVelocity(delta.getBasis()*body.getAngularVelocity());
        const auto other=impl->bodies.find(impl->grips[1-hand].id);
        impl->ClipMotion(entry,from,to,other==impl->bodies.end()?nullptr:other->second.body.get());
        const auto actual=body.getWorldTransform();
        grip.carryVelocity=(actual.getOrigin()-from.getOrigin())/seconds;
        grip.target=delta*grip.target;grip.constraint->setFrames(grip.target,grip.anchor);
        impl->world.updateSingleAabb(&body);body.activate(true);
    }
}
void World::SleepRagdoll(uint32_t actor){auto i=impl->ragdolls.find(actor);if(i==impl->ragdolls.end())return;for(auto id:i->second.bodies){auto& b=*impl->bodies.at(id).body;b.setLinearVelocity({0,0,0});b.setAngularVelocity({0,0,0});b.forceActivationState(ISLAND_SLEEPING);}}
bool World::RagdollSleeping(uint32_t actor)const{
 const auto i=impl->ragdolls.find(actor);if(i==impl->ragdolls.end())return false;
 for(auto id:i->second.bodies)if(impl->bodies.at(id).body->isActive())return false;
 return true;
}
bool World::HasRagdoll(uint32_t actor)const{return impl->ragdolls.count(actor)!=0;}
std::vector<uint32_t> World::RagdollActors()const{std::vector<uint32_t> ids;for(const auto& e:impl->ragdolls)ids.push_back(e.first);return ids;}
void World::RemoveRagdoll(uint32_t actor){
 auto i=impl->ragdolls.find(actor);if(i==impl->ragdolls.end())return;
 for(auto& j:i->second.joints)impl->world.removeConstraint(j.get());
 i->second.joints.clear();
 for(auto id:i->second.bodies)Remove(id);
 impl->ragdolls.erase(i);
}
bool World::RagdollPose(uint32_t actor,std::vector<M>& bones,bool* sleeping,std::vector<V>* linear,std::vector<V>* angular)const{
 const auto i=impl->ragdolls.find(actor);if(i==impl->ragdolls.end())return false;
 bones.resize(i->second.bodies.size());if(linear)linear->resize(bones.size());if(angular)angular->resize(bones.size());bool asleep=true;
 for(size_t k=0;k<bones.size();++k){const auto id=i->second.bodies[k];if(!Pose(id,bones[k],linear?&(*linear)[k]:nullptr,angular?&(*angular)[k]:nullptr))return false;asleep&=!impl->bodies.at(id).body->isActive();}
 if(sleeping)*sleeping=asleep;return true;
}
bool World::SetRagdollMotion(uint32_t actor,const std::vector<V>& linear,const std::vector<V>& angular){
 const auto i=impl->ragdolls.find(actor);if(i==impl->ragdolls.end()||linear.size()!=i->second.bodies.size()||angular.size()!=linear.size())return false;
 for(size_t n=0;n<linear.size();++n)if(!Finite(linear[n])||!Finite(angular[n]))return false;
 for(size_t n=0;n<linear.size();++n){auto& body=*impl->bodies.at(i->second.bodies[n]).body;body.setLinearVelocity(B(linear[n]));body.setAngularVelocity(B(angular[n]));body.activate(true);}return true;
}
bool World::AddRagdoll(uint32_t actor,const fo3rag::Definition& asset,const std::vector<M>& poses,float scale,const V& velocity,const std::vector<float>& bodyScales){
 if(HasRagdoll(actor))return true;
 if(!actor||asset.bodies.size()<2||asset.bodies.size()>64||asset.joints.size()+1!=asset.bodies.size()||poses.size()!=asset.bodies.size()||!std::isfinite(scale)||scale<=0||scale>1||!Finite(velocity))return false;
 if(!bodyScales.empty()&&bodyScales.size()!=poses.size())return false;
 for(float s:bodyScales)if(!std::isfinite(s)||s<=0||s>1)return false;
 for(const auto& p:poses)if(!Finite(p))return false;
 for(const auto& j:asset.joints)if(j.a>=poses.size()||j.b>=poses.size()||j.a==j.b||!Finite(j.frameA)||!Finite(j.frameB)||!Finite(j.minimum)||!Finite(j.maximum))return false;
 impl->ragdolls.emplace(actor,Impl::Ragdoll{});auto& rag=impl->ragdolls.at(actor);
 for(size_t n=0;n<asset.bodies.size();++n){
  const auto& d=asset.bodies[n];const uint64_t id=(uint64_t{1}<<62)|(uint64_t(actor)<<8)|n;
  if(Has(id)||d.mass<=0||!std::isfinite(d.mass)||!Finite(d.a)||!Finite(d.b)||!Finite(d.center)||!std::isfinite(d.radius)||d.radius<=0){RemoveRagdoll(actor);return false;}
  const float bodyScale=bodyScales.empty()?scale:bodyScales[n];
  Entry e;e.ragdoll=true;e.ragdollScale=bodyScale;e.ragdollRadius=(B(d.b)-B(d.a)).length()*.5f*bodyScale+d.radius*bodyScale;e.principal.setOrigin(B(d.center)*bodyScale);
  btVector3 inertia(d.inertia[0]*bodyScale*bodyScale,d.inertia[4]*bodyScale*bodyScale,d.inertia[8]*bodyScale*bodyScale);
  if(inertia.x()>0&&inertia.y()>0&&inertia.z()>0){btMatrix3x3 tensor(d.inertia[0],d.inertia[1],d.inertia[2],d.inertia[3],d.inertia[4],d.inertia[5],d.inertia[6],d.inertia[7],d.inertia[8]),axes;for(int r=0;r<3;++r)tensor[r]*=bodyScale*bodyScale;tensor.diagonalize(axes,1e-6f,32);if(tensor[0][0]>0&&tensor[1][1]>0&&tensor[2][2]>0){e.principal.setBasis(axes);inertia={tensor[0][0],tensor[1][1],tensor[2][2]};}}
  const auto a=B(d.a)*bodyScale,b=B(d.b)*bodyScale,axis=b-a;
  auto capsule=std::make_unique<btCapsuleShape>(d.radius*bodyScale,axis.length());capsule->setMargin(.001f);
  auto local=btTransform::getIdentity();local.setOrigin((a+b)*.5f);
  if(axis.length2()>1e-10f)local.setRotation(shortestArcQuat(btVector3(0,1,0),axis.normalized()));
  local=e.principal.inverse()*local;
  auto compound=std::make_unique<btCompoundShape>();compound->addChildShape(local,capsule.get());e.children.push_back(std::move(capsule));e.shape=std::move(compound);e.ragdollThinRadius=d.radius*bodyScale;
  // Authored full inertia is rotated into its principal frame above.
  if(inertia.x()<=0||inertia.y()<=0||inertia.z()<=0)e.shape->calculateLocalInertia(d.mass,inertia);
  btRigidBody::btRigidBodyConstructionInfo info(d.mass,nullptr,e.shape.get(),inertia);
  info.m_friction=std::clamp(d.friction,0.f,2.f);info.m_restitution=std::clamp(d.restitution,0.f,1.f);
  info.m_linearDamping=std::clamp(d.linearDamping,0.f,1.f);info.m_angularDamping=std::clamp(d.angularDamping,0.f,1.f);
  e.body=std::make_unique<btRigidBody>(info);const auto pose=B(poses[n])*e.principal;e.body->setWorldTransform(pose);e.body->setInterpolationWorldTransform(pose);e.body->setLinearVelocity(B(velocity));
  e.body->setSleepingThresholds(.06f,.25f);e.body->setCcdMotionThreshold(d.radius*bodyScale*.5f);e.body->setCcdSweptSphereRadius(d.radius*bodyScale*.8f);
  impl->world.addRigidBody(e.body.get(),4,7);impl->bodies.emplace(id,std::move(e));rag.bodies.push_back(id);
 }
 for(const auto& d:asset.joints){
  auto& a=impl->bodies.at(rag.bodies[d.a]);auto& b=impl->bodies.at(rag.bodies[d.b]);
  auto frame=[&](const M& source,const Entry& e){auto m=source;for(int k=0;k<3;++k)m[12+k]*=e.ragdollScale;return e.principal.inverse()*B(m);};
  const auto fa=frame(d.frameA,a),fb=frame(d.frameB,b);std::unique_ptr<btTypedConstraint> joint;
  const float torque=std::max(0.f,d.friction)*49*scale*scale;
  if(d.hinge){auto h=std::make_unique<btHingeConstraint>(*a.body,*b.body,fa,fb,true);h->setLimit(d.minimum[2],d.maximum[2]);if(torque>0)h->enableAngularMotor(true,0,torque/120.f);joint=std::move(h);}
  else{
   // Bullet angular box uses the authored twist/plane/cone extrema. Havok's
   // coupled cone surface and feedback motors are not claimed numerically equal.
   auto h=std::make_unique<btGeneric6DofSpring2Constraint>(*a.body,*b.body,fa,fb,RO_XYZ);
   h->setLinearLowerLimit({0,0,0});h->setLinearUpperLimit({0,0,0});h->setAngularLowerLimit(B(d.minimum));h->setAngularUpperLimit(B(d.maximum));
   for(int k=0;k<3;++k){h->enableMotor(3+k,torque>0);h->setTargetVelocity(3+k,0);h->setMaxMotorForce(3+k,torque);}joint=std::move(h);
  }
  impl->world.addConstraint(joint.get(),true);rag.joints.push_back(std::move(joint));
 }
 return true;
}
void World::Step(float seconds){
    if(!(seconds>0)||!std::isfinite(seconds))return;
    impl->accumulated+=std::min(seconds,.064f);constexpr float dt=1.f/120.f;
    while(impl->accumulated+1e-7f>=dt){
        impl->accumulated-=dt;
        auto& previous=impl->previous;previous.clear();
        for(auto& entry:impl->bodies)if(!entry.second.ragdoll&&entry.second.body->isActive())previous.push_back({&entry.second,entry.second.body->getWorldTransform()});
        for(auto& grip:impl->grips)if(grip.constraint)grip.constraint->setFrames(grip.target,grip.anchor);
        // Keep linked capsules in one constraint solve. Independent per-bone
        // post-step casts can tear joints on stair edges. Bound active ragdoll
        // surface displacement with up to eight smaller shared solver steps.
        auto& active=impl->activeRagdolls;active.clear();
        for(const auto& row:impl->ragdolls)if(!RagdollSleeping(row.first))active.push_back(row.first);
        int substeps=1;
        for(auto actor:active)for(auto id:impl->ragdolls.at(actor).bodies){const auto& e=impl->bodies.at(id);const auto& b=*e.body;if(!b.isActive())continue;const float speed=b.getLinearVelocity().length()+b.getAngularVelocity().length()*e.ragdollRadius;substeps=std::max(substeps,std::min(8,int(std::ceil(speed*dt/std::max(.002f,e.ragdollThinRadius*.5f)))));}
        for(int i=0;i<substeps;++i)impl->world.stepSimulation(dt/substeps,0);
        // Bullet's swept sphere protects linear COM motion. Sweep each authored
        // convex child as well so a fast-spinning thin plate/weapon cannot rotate
        // through a floor between ticks, including compound bodies.
        for(const auto& state:previous){
            auto& body=*state.first->body;const auto from=state.second,to=body.getWorldTransform();
            impl->ClipMotion(*state.first,from,to);
        }
        // Malleable/motor constraints can keep individual Bullet bodies awake
        // indefinitely at sub-centimetre contact jitter. Sleep the whole island
        // after two supported, quiet seconds, using speed at the capsule surface.
        // Gravity prevents an unsupported falling island satisfying this window.
        if(!active.empty()){
            auto& contacts=impl->restingContacts;contacts.clear();
            for(int i=0;i<impl->dispatcher.getNumManifolds();++i){auto* m=impl->dispatcher.getManifoldByIndexInternal(i);for(int k=0;k<m->getNumContacts();++k)if(m->getContactPoint(k).getDistance()<.005f){contacts.push_back(m->getBody0());contacts.push_back(m->getBody1());break;}}
            for(auto actor:active){auto& rag=impl->ragdolls.at(actor);bool quiet=true,supported=false,awake=false;
                for(auto id:rag.bodies){const auto& e=impl->bodies.at(id);const auto& body=*e.body;awake|=body.isActive();supported|=std::find(contacts.begin(),contacts.end(),&body)!=contacts.end();quiet&=body.getLinearVelocity().length()+body.getAngularVelocity().length()*e.ragdollRadius<.1f;}
                if(!awake){rag.quietSeconds=0;continue;}rag.quietSeconds=quiet&&supported?rag.quietSeconds+dt:0;
                if(rag.quietSeconds>=2){SleepRagdoll(actor);rag.quietSeconds=0;}
            }
        }

    }
}

bool World::CanExpandCapsule(const V& feet,float radius,float oldHeight,float newHeight)const{
 if(newHeight<=oldHeight)return true;
 // Inset the head sweep slightly so a tangent wall contact cannot block standing.
 btSphereShape shape(std::max(.01f,radius-.005f));auto from=btTransform::getIdentity(),to=from;
 from.setOrigin(B(feet)+btVector3(0,std::max(radius,oldHeight-radius),0));to.setOrigin(B(feet)+btVector3(0,newHeight-radius,0));
 struct Upward:btCollisionWorld::ClosestConvexResultCallback{using ClosestConvexResultCallback::ClosestConvexResultCallback;btScalar addSingleResult(btCollisionWorld::LocalConvexResult& r,bool world)override{const auto normal=world?r.m_hitNormalLocal:r.m_hitCollisionObject->getWorldTransform().getBasis()*r.m_hitNormalLocal;if(normal.y()>=-.01f)return 1;return ClosestConvexResultCallback::addSingleResult(r,world);}}hit(from.getOrigin(),to.getOrigin());hit.m_collisionFilterGroup=4;hit.m_collisionFilterMask=3;
 impl->world.convexSweepTest(&shape,from,to,hit,.001f);return !hit.hasHit();
}
bool World::SweepCapsule(const V& from,const V& to,float radius,float height,V& resolved,V& normal,float minimumUpNormal)const{
    btCapsuleShape capsule(radius,std::max(0.f,height-2*radius));capsule.setMargin(.001f);
    btTransform a=btTransform::getIdentity(),b=a;a.setOrigin(B(from)+btVector3(0,height*.5f,0));b.setOrigin(B(to)+btVector3(0,height*.5f,0));
    struct SupportHit:btCollisionWorld::ClosestConvexResultCallback{
        float minimum;
        SupportHit(const btVector3& from,const btVector3& to,float threshold):ClosestConvexResultCallback(from,to),minimum(threshold){}
        btScalar addSingleResult(btCollisionWorld::LocalConvexResult& result,bool world)override{
            const auto n=world?result.m_hitNormalLocal:result.m_hitCollisionObject->getWorldTransform().getBasis()*result.m_hitNormalLocal;
            // A nearer rail/vertical edge must not hide the floor from a
            // support query. Other sweep callers retain the unfiltered path.
            if(n.y()<minimum)return 1;
            return ClosestConvexResultCallback::addSingleResult(result,world);
        }
    }hit(a.getOrigin(),b.getOrigin(),minimumUpNormal);
    hit.m_collisionFilterGroup=4;hit.m_collisionFilterMask=3;
    impl->world.convexSweepTest(&capsule,a,b,hit,.001f);
    if(!hit.hasHit()){resolved=to;normal={};return false;}
    normal=A(hit.m_hitNormalWorld);resolved=A(B(from).lerp(B(to),std::max(0.f,float(hit.m_closestHitFraction)-.001f)));return true;
}
} // namespace fo3rigid
