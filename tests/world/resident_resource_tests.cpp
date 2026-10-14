#include "fo3-resident-resources.h"
#include <cassert>
#include <iostream>
#include <limits>

static Fo3WorldPlacement Placement(uint32_t id) {
    Fo3WorldPlacement p; p.refFormId=id; p.baseFormId=100+id;
    p.modelPath="architecture\\fixture.nif"; p.baseRecordType="STAT";
    p.x=float(id); p.owningCellFormId=1000; return p;
}
int main() {
    using namespace fo3scene;
    auto a=Placement(1), b=Placement(2), c=Placement(3);
    std::string error; ResidentResourceSelection selected;
    const std::vector<ResidentVisualRecord> live{{1,false},{1,false},{2,false},{9,false},{0,true}};
    assert(SelectResidentResources({a,b},{b,c},live,{1,3},selected,error));
    assert(error.empty()&&selected.retainedSlots==std::vector<size_t>({2,3,4}));
    assert(selected.removedReferences==std::vector<uint32_t>({1})&&selected.incomingReferences==std::vector<uint32_t>({3}));
    assert(selected.incomingPlacements.size()==1&&SameResidentPlacement(selected.incomingPlacements[0],c));
    assert(selected.collisionPlacements.size()==2&&SameResidentPlacement(selected.collisionPlacements[0],b));
    ResidentResourceSelection unchanged; unchanged.retainedSlots={999};
    auto rejects=[&](const std::vector<Fo3WorldPlacement>& before,const std::vector<Fo3WorldPlacement>& after,
                     const std::vector<ResidentVisualRecord>& records,const std::vector<uint32_t>& affected) {
        assert(!SelectResidentResources(before,after,records,affected,unchanged,error));
        assert(!error.empty()&&unchanged.retainedSlots==std::vector<size_t>({999}));
    };
    rejects({a,b},{b,c},live,{1}); // uncovered addition
    rejects({a,b},{b,c},live,{3}); // uncovered removal
    rejects({a,a},{a},live,{});
    rejects({a},{a,a},live,{});
    rejects({a},{a},live,{0}); rejects({a},{a},live,{1,1}); rejects({a},{a},live,{3,1});
    auto moved=a; moved.x+=1; rejects({a},{moved},live,{1});
    moved=a; moved.modelPath="another-fixture.nif"; rejects({a},{moved},live,{1});
    moved=a; moved.x=std::numeric_limits<float>::quiet_NaN(); rejects({moved},{a},live,{1});
    moved=a; moved.scale=0; rejects({a},{moved},live,{1});
    rejects({a},{a},{{0,false}},{});
    rejects({a},{a,c},{{3,false}},{3}); // stale/ambiguous live scope
    rejects({a},{a},{{9,false}},{9}); // changed live reference omitted from both placement scopes
    assert(SelectResidentResources({a,b},{},live,{1,2},selected,error));
    assert(selected.incomingPlacements.empty()&&selected.collisionPlacements.empty());
    assert(selected.retainedSlots==std::vector<size_t>({3,4}));
    assert(SelectResidentResources({},{c},{}, {3},selected,error)&&selected.retainedSlots.empty());
    assert(SelectResidentResources({a,b},{a,b},live,{},selected,error)&&selected.retainedSlots.size()==live.size());

    // The worker prepares only newly enabled references; collision remains the
    // complete authored candidate rather than the number of visible NIF parts.
    assert(SelectResidentResources({a},{a,b,c},{{1,false}},{2,3},selected,error));
    std::atomic<bool> cancelled{false}; std::vector<uint32_t> parts{999}; unsigned calls=0;
    auto build=[&](const Fo3WorldPlacement& p,std::vector<uint32_t>& out) {
        ++calls; out={p.refFormId*10,p.refFormId*10+1}; return true;
    };
    auto skip=[](const Fo3WorldPlacement&) { return false; };
    assert(PrepareResidentCpuParts(selected,parts,build,skip,cancelled,error));
    assert(calls==2&&parts==std::vector<uint32_t>({20,21,30,31})&&selected.collisionPlacements.size()==3);
    parts={999}; calls=0;
    assert(!PrepareResidentCpuParts(selected,parts,[&](const auto& p,auto& out) {
        build(p,out); return p.refFormId!=3;
    },skip,cancelled,error));
    assert(parts==std::vector<uint32_t>({999})&&calls==2);
    assert(!PrepareResidentCpuParts(selected,parts,[](const auto&,auto&) { return true; },skip,cancelled,error));
    assert(parts==std::vector<uint32_t>({999})); // no geometry is not successful decode
    calls=0;
    assert(!PrepareResidentCpuParts(selected,parts,build,[](const auto&) { return true; },cancelled,error));
    assert(calls==0&&parts==std::vector<uint32_t>({999}));
    cancelled=true;
    assert(!PrepareResidentCpuParts(selected,parts,build,skip,cancelled,error)&&calls==0);
    cancelled=false;
    assert(!PrepareResidentCpuParts(selected,parts,[&](const auto& p,auto& out) {
        build(p,out); cancelled=true; return true;
    },skip,cancelled,error));
    assert(parts==std::vector<uint32_t>({999})&&calls==1);
    cancelled=false;
    try {
        PrepareResidentCpuParts(selected,parts,[](const auto&,std::vector<uint32_t>&) -> bool {
            throw std::bad_alloc();
        },skip,cancelled,error);
        assert(false);
    } catch(const std::bad_alloc&) { assert(parts==std::vector<uint32_t>({999})); }
    std::cout<<"Resident placement delta, retained multi-part slots, complete collision inputs and atomic CPU preparation passed\n";
}
