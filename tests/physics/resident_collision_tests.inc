// Production private candidate path. Inputs are synthetic NIF bytes, while the
// parser, filtering, transforms, registry, derived structures and swaps are real.
static std::vector<uint8_t> privateNif(const std::vector<std::string>& types,
        const std::vector<std::vector<uint8_t>>& blocks) {
    std::string line="Gamebryo File Format, Version 20.2.0.7\n";
    std::vector<uint8_t> b(line.begin(),line.end());
    u32(b,0x14020007);b.push_back(1);u32(b,11);u32(b,blocks.size());u32(b,34);
    b.insert(b.end(),{0,0,0});u16(b,types.size());
    for(const auto& type:types)str(b,type);
    for(size_t i=0;i<types.size();++i)u16(b,i);
    for(const auto& block:blocks)u32(b,block.size());
    u32(b,0);u32(b,0);u32(b,0);
    for(const auto& block:blocks)b.insert(b.end(),block.begin(),block.end());
    return b;
}
static void privateCollisionTests(const Fo3WorldPlacement& a,const Fo3WorldPlacement& b,
        const Fo3WorldPlacement& item,const Fo3WorldPlacement& door) {
    using namespace fo3_q6f_collision_detail;
    const auto countRef=[](uint32_t ref){size_t count=0;
        for(const auto& tri:gWorldTriangles){auto source=gSurfaceSourcesQ722.find(tri.surfaceKeyQ714);
            if(source!=gSurfaceSourcesQ722.end()&&source->second.refFormId==ref)++count;}return count;};
    const auto fixture=packedNif();NifHeader header;assert(ParseHeader(fixture,header));
    std::vector<std::string> types;std::vector<std::vector<uint8_t>> blocks;
    for(uint32_t i=0;i<header.numBlocks;++i){types.push_back(BlockType(header,i));
        const auto* data=BlockData(fixture,header,i);blocks.emplace_back(data,data+header.blockSizes[i]);}
    types.push_back("bhkListShape");types.push_back("bhkUnsupportedShape");
    blocks[1][0]=4; // rigid body root becomes a list with one supported/missing leaf
    std::vector<uint8_t> list;u32(list,2);u32(list,2);u32(list,5);
    blocks.push_back(list);blocks.emplace_back(4);
    assets["PartialCollision.nif"]=privateNif(types,blocks);
    assets["NoCollision.nif"]=privateNif({"NiNode"},{{}});
    std::vector<Fo3NifCollisionShapeQ6F> decoded;
    assert(LoadFo3NifCollisionShapesImpl("PartialCollision.nif",decoded,false)&&decoded.size()==1);
    assert(!LoadFo3NifCollisionShapesComplete("PartialCollision.nif",decoded)&&decoded.size()==1);
    assert(LoadFo3NifCollisionShapesComplete("NoCollision.nif",decoded)&&decoded.empty());
    assert(!LoadFo3NifCollisionShapesComplete("MissingPrivate.nif",decoded)&&decoded.empty());
    std::atomic<bool> cancelled{false};
    for(bool exterior:{false,true}) {
        SetFo3CollectedCollisionRefs({});
        publish(prepare({a,b,item},exterior,123));
        const auto state=CaptureFo3CollisionMutationState();
        assert(state.dynamicOnlyModels.count(NormalizeFo3CollisionModelPathQ710(item.modelPath)));
        auto candidate=item;candidate.refFormId=777;candidate.baseRecordType="STAT";
        // Poison ordinary caches: the private path must decode from source and
        // must not read, clear, prime or append to those published-scope caches.
        gQ1820NoCollisionModels.insert(item.modelPath);
        gQ1820NegativeCollisionPlacementCache.insert(candidate.refFormId);
        gQ1990CollisionCacheContextValid=false; // private chunks have their own explicit context
        const auto cacheCount=gQ1990CollisionPlacementCache.size();
        const auto negative=gQ1820NegativeCollisionPlacementCache;
        const auto noModels=gQ1820NoCollisionModels;
        const auto cacheSerial=gQ1990CollisionCacheSerial;
        const auto cacheContext=gQ1990CollisionCacheContextValid;
        const auto live=gWorldTriangles.data();const auto revision=GetFo3CollisionPublicationRevision();
        const auto registered=gQ1930PreparedSnapshots.size();
        uint64_t token=0;
        assert(PrepareFo3ResidentCollisionSnapshot({candidate},0,0,0,0,0,70,state,cancelled,&token,exterior,123));
        assert(token&&gWorldTriangles.data()==live&&GetFo3CollisionPublicationRevision()==revision);
        assert(Fo3DynamicOnlyCollisionModelsQ710()==state.dynamicOnlyModels);
        assert(gQ1990CollisionPlacementCache.size()==cacheCount&&gQ1990CollisionCacheSerial==cacheSerial);
        assert(gQ1820NegativeCollisionPlacementCache==negative&&gQ1820NoCollisionModels==noModels&&
            gQ1990CollisionCacheContextValid==cacheContext&&!gQ1990CollisionPlacementCache.count(777));
        {
            Fo3CollisionSnapshotOwner owner(token);Fo3CollisionSnapshotOwner moved(std::move(owner));
            assert(!owner.Get()&&moved.Get()==token&&CanExchangeFo3CollisionMutationSnapshot(token));
            for(int repeat=0;repeat<2;++repeat){
                forbidAllocations=true;
                const bool forward=ExchangeFo3CollisionMutationSnapshot(token);
                const bool canRollback=CanExchangeFo3CollisionMutationSnapshot(token);
                forbidAllocations=false;
                assert(forward&&canRollback&&Fo3DynamicOnlyCollisionModelsQ710().empty()&&countRef(777)==2);
                forbidAllocations=true;
                const bool rollback=ExchangeFo3CollisionMutationSnapshot(token);
                forbidAllocations=false;
                assert(rollback&&gWorldTriangles.data()==live&&Fo3DynamicOnlyCollisionModelsQ710()==state.dynamicOnlyModels);
            }
            Fo3DynamicOnlyCollisionModelsQ710().insert("textures-not-a-collision-model");
            assert(!CanExchangeFo3CollisionMutationSnapshot(token)&&!ExchangeFo3CollisionMutationSnapshot(token));
            Fo3DynamicOnlyCollisionModelsQ710().erase("textures-not-a-collision-model");
        }
        assert(gQ1930PreparedSnapshots.size()==registered&&!CanExchangeFo3CollisionMutationSnapshot(token));
        gQ1820NoCollisionModels.erase(item.modelPath);
        // Named missing/unsupported/malformed inputs never become empty worlds.
        auto rejects=[&](std::vector<Fo3WorldPlacement> placements){
            token=99;const auto before=gQ1930PreparedSnapshots.size();
            assert(!PrepareFo3ResidentCollisionSnapshot(placements,0,0,0,0,0,70,
                CaptureFo3CollisionMutationState(),cancelled,&token,exterior,123));
            assert(!token&&gQ1930PreparedSnapshots.size()==before&&gWorldTriangles.data()==live);
        };
        auto bad=candidate;bad.modelPath="MissingPrivate.nif";rejects({bad});
        bad.modelPath="PartialCollision.nif";rejects({bad});
        bad=candidate;bad.scale=0;rejects({bad});
        bad=candidate;bad.x=std::numeric_limits<float>::quiet_NaN();rejects({bad});
        rejects({candidate,candidate});
        cancelled=true;rejects({candidate});cancelled=false;
        cancelPrivateAsset=&cancelled;rejects({candidate});assert(cancelled);cancelled=false;
        auto alias=candidate;alias.refFormId=778;
        for(char& ch:alias.modelPath){if(ch>='a'&&ch<='z')ch=char(ch-'a'+'A');if(ch=='\\')ch='/';}
        assert(PrepareFo3ResidentCollisionSnapshot({candidate,alias},0,0,0,0,0,70,
            CaptureFo3CollisionMutationState(),cancelled,&token,exterior,123));
        {Fo3CollisionSnapshotOwner owner(token);assert(ExchangeFo3CollisionMutationSnapshot(token));
         assert(countRef(777)==2&&countRef(778)==2);assert(ExchangeFo3CollisionMutationSnapshot(token));}
        bool threw=false;forbidAllocations=true;
        try{PrepareFo3ResidentCollisionSnapshot({candidate},0,0,0,0,0,70,state,cancelled,&token,exterior,123);}
        catch(const std::bad_alloc&){threw=true;}
        forbidAllocations=false;assert(threw&&gWorldTriangles.data()==live);
        if(exterior){std::vector<Fo3WorldPlacement> capped(1025,candidate);
            for(size_t n=0;n<capped.size();++n)capped[n].refFormId=10000+n;
            rejects(capped);}

        // Explicit no-collision assets and empty selections are valid private
        // resources. Finalize-after-commit keeps only the newly published world.
        auto absent=candidate;absent.modelPath="NoCollision.nif";
        for(const auto& selected:std::vector<std::vector<Fo3WorldPlacement>>{{},{absent}}){
            assert(PrepareFo3ResidentCollisionSnapshot(selected,0,0,0,0,0,70,
                CaptureFo3CollisionMutationState(),cancelled,&token,exterior,123));
            Fo3CollisionSnapshotOwner owner(token);
            assert(ExchangeFo3CollisionMutationSnapshot(token));emptyQueries(exterior);
            assert(ExchangeFo3CollisionMutationSnapshot(token)&&gWorldTriangles.data()==live);
        }
        // Captured door movement and collection changes invalidate the token.
        assert(PrepareFo3ResidentCollisionSnapshot({candidate},0,0,0,0,0,70,
            CaptureFo3CollisionMutationState(),cancelled,&token,exterior,123));
        {Fo3CollisionSnapshotOwner owner(token);SetFo3CollectedCollisionRefs({a.refFormId});
         assert(!CanExchangeFo3CollisionMutationSnapshot(token)&&!ExchangeFo3CollisionMutationSnapshot(token));}
        SetFo3CollectedCollisionRefs({});
        if(exterior)continue; // Animated door collision currently belongs to interiors.
        publish(prepare({a,door},exterior,124));
        assert(SetFo3DoorCollisionTransformQ2400(door.refFormId,nullptr,false));
        assert(PrepareFo3ResidentCollisionSnapshot({door},0,0,0,0,0,70,
            CaptureFo3CollisionMutationState(),cancelled,&token,exterior,124));
        {Fo3CollisionSnapshotOwner owner(token);assert(ExchangeFo3CollisionMutationSnapshot(token));emptyQueries(exterior);
         assert(ExchangeFo3CollisionMutationSnapshot(token));}
        assert(PrepareFo3ResidentCollisionSnapshot({door},0,0,0,0,0,70,
            CaptureFo3CollisionMutationState(),cancelled,&token,false,124));
        {Fo3CollisionSnapshotOwner owner(token);assert(SetFo3DoorCollisionTransformQ2400(door.refFormId,nullptr,true));
         assert(!CanExchangeFo3CollisionMutationSnapshot(token)&&!ExchangeFo3CollisionMutationSnapshot(token));}
    }
    // A cancelled worker retains its completed registry token until the lane
    // joins it, then releases the token without ever exchanging the live world.
    struct Packet{Fo3CollisionSnapshotOwner collision;};
    fo3scene::ResidentPreparationLane<Packet> lane;
    const auto ticket=lane.Acquire();assert(lane.Poll(ticket,{}));
    const auto state=CaptureFo3CollisionMutationState();
    const auto registered=gQ1930PreparedSnapshots.size();
    const auto live=gWorldTriangles.data();
    std::atomic<bool> ready{false},finish{false};
    assert(lane.Start(ticket,[&](Packet& packet,const std::atomic<bool>& cancel){
        uint64_t token=0;
        if(!PrepareFo3ResidentCollisionSnapshot({a},0,0,0,0,0,70,state,cancel,&token,false,124))return false;
        packet.collision=Fo3CollisionSnapshotOwner(token);ready.store(true,std::memory_order_release);
        while(!finish.load(std::memory_order_acquire))std::this_thread::yield();
        return true;
    }));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready.load(std::memory_order_acquire)){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    {std::lock_guard<std::mutex> lock(gQ1930SnapshotMutex);assert(gQ1930PreparedSnapshots.size()==registered+1);}
    assert(!lane.Cancel(ticket)&&lane.Blocked());finish.store(true,std::memory_order_release);
    while(!lane.Cancel(ticket)){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(gQ1930PreparedSnapshots.size()==registered&&gWorldTriangles.data()==live);
    std::cout<<"Private resident collision policy/cache isolation, complete chains and reversible ownership passed\n";
}
