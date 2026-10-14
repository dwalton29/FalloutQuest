// Export original active CELL placements for the optional collision runtime test.
// Reads the user-provided master; output is metadata, never copied game assets.
#include "data/fo3-install-paths.h"
#include "world/fo3-transition.h"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#define Q71_LOGI(...) std::fprintf(stderr,__VA_ARGS__)
#define Q71_LOGE(...) std::fprintf(stderr,__VA_ARGS__)
#include "world/fo3-cell-interior.inc"
int main(int argc,char**argv) {
 if(argc!=4){std::fprintf(stderr,"usage: export_collision_fixture <data-root> <cell-hex> <manifest>\n");return 2;}
 if(setenv("FALLOUTQUEST_DATA_ROOT",argv[1],1))return 3;
 std::vector<Fo3WorldPlacement> placements;
 if(!LoadFo3InteriorCellPlacementsQ1700(std::stoul(argv[2],nullptr,16),placements,nullptr))return 4;
 std::ofstream f(argv[3]);f<<std::setprecision(std::numeric_limits<float>::max_digits10);
 for(const auto& p:SelectFo3AuthoredCollisionPlacements(placements))
  f<<p.refFormId<<' '<<p.baseRecordType<<' '<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.rx<<' '<<p.ry<<' '<<p.rz<<' '<<p.scale<<' '<<p.modelPath<<'\n';
 return f?0:5;
}
