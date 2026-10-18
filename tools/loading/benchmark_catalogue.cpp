// Host microbenchmark, not a Quest startup benchmark. Link with the same
// sources as player_state_tests; original ESM stays outside the repository.
#include "player/fo3-player-state.h"
#include <chrono>
#include <iostream>
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    fo3player::Catalog catalog;std::string error;
    const auto started=std::chrono::steady_clock::now();
    if(!fo3player::LoadCatalog(argv[1],catalog,error)){std::cerr<<error<<'\n';return 1;}
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count();
    std::cout<<"catalogueUs="<<us<<" items="<<catalog.items.size()<<" references="<<catalog.references.size()
        <<" fingerprint="<<catalog.fingerprint<<" loot="<<catalog.lootFingerprint<<" world="<<catalog.worldFingerprint<<'\n';
}
