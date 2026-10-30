#include <cassert>
#include <fstream>
#include <iostream>
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
static std::vector<uint8_t> fixture;
bool LoadFalloutMeshFile(const std::string&,std::vector<uint8_t>& bytes,std::string*){bytes=fixture;return !bytes.empty();}
int main(int argc,char** argv){
 Fo3StaticNifMesh mesh;mesh.dismemberSkin=true;mesh.indices={0,1,2,3,4,5,6,7,8,9,10,11};mesh.skinTriangleBodyParts={1,101,201,10};
 const auto original=mesh;assert(Fo3SuppressIntactGoreCaps(mesh)==2);assert((mesh.indices==std::vector<uint32_t>{0,1,2,9,10,11}));assert((mesh.skinTriangleBodyParts==std::vector<uint16_t>{1,10}));assert(original.indices.size()==12);assert(Fo3SuppressIntactGoreCaps(mesh)==0);
 for(int arg=1;arg<argc;++arg){std::ifstream f(argv[arg],std::ios::binary);fixture.assign(std::istreambuf_iterator<char>(f),{});assert(!fixture.empty());std::vector<Fo3StaticNifMesh> shapes;assert(LoadFo3StaticNifMeshes("original.nif",shapes,true));size_t caps=0,kept=0;
  for(auto& shape:shapes){const auto source=shape;caps+=Fo3SuppressIntactGoreCaps(shape);kept+=shape.indices.size()/3;assert(source.indices.size()>=shape.indices.size());for(auto part:shape.skinTriangleBodyParts)assert(!((part>=101&&part<=113)||(part>=201&&part<=213)));}
  std::cout<<argv[arg]<<" caps removed="<<caps<<" intact triangles="<<kept<<"\n";assert(kept>0);if(argc==3)assert(caps==(arg==1?0:2161));
 }
}
