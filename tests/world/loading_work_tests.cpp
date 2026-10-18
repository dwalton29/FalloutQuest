#include "rendering/loading-transfer.h"
#include <cassert>
#include <iostream>
#include <vector>
struct Image {int width=0,height=0;std::vector<uint8_t> rgba;};
int main(){
 fqloading::FrameBudget budget;
 budget.Begin(1000,13889,0,13889,0,true);
 assert(budget.Allow(1000));budget.Charge(1000,3500);
 assert(!budget.Allow(3500));
 budget.Begin(4000,13889,25000,28000,9000,true);
 assert(budget.allowance<2000&&budget.Allow(4000));
 budget.gpuReady=false;assert(!budget.Allow(4000));
 budget.Begin(8000,13889,0,13889,0,true);
 // An overrun cannot permanently starve a queued bounded transfer.
 assert(budget.Allow(80000));budget.Charge(80000,80001);assert(!budget.Allow(80001));
 budget.Begin(90000,8333,0,8333,0,true);assert(budget.allowance<=8333/4);
 budget.Begin(100000,13889,999999,999999,999999,false);assert(budget.Allow(999999));
 for(size_t width:{size_t(1),size_t(1025),size_t(8192)}) {
  Image image;image.width=int(width);image.height=513;image.rgba.resize(width*image.height*4);
  for(size_t i=0;i<image.rgba.size();++i)image.rgba[i]=uint8_t(i*17+3);
  fqloading::ImageTransfer transfer;const auto mips=driver::mips;
  assert(!transfer.Step(image,true,true));assert(driver::transfers==0||!transfer.rowsComplete);
  assert(driver::levels==(width==1?10:width==1025?11:14));
  for(int calls=0;!transfer.ready&&calls<1000;++calls)transfer.Step(image,true,true);
  assert(transfer.ready&&driver::pixels==image.rgba&&driver::mips==mips+1);
  for(auto n:driver::writes)assert(n==1);
  auto id=transfer.Take();assert(id&&!transfer.id&&!transfer.ready);
  const auto deleted=driver::deletions;
  assert(!transfer.Step(image,false,false));transfer.Cancel();
  assert(driver::deletions==deleted+1&&!transfer.id);
 }
 Image small{1,1,{1,2,3,4}};fqloading::ImageTransfer ui;
 assert(!ui.Step(small,false,false));assert(ui.Step(small,false,false));assert(driver::levels==1);
 assert(ui.Take());
 Image invalid{4,4,{1}};assert(!ui.Step(invalid,false,false)&&!ui.id&&ui.failed);ui.Cancel();
 driver::error=123;assert(!ui.Step(small,true,true)&&ui.failed&&!ui.ready);
 const auto deleted=driver::deletions;ui.Cancel();assert(driver::deletions==deleted+1);
 assert(!ui.Step(small,false,false)&&!ui.failed);assert(ui.Step(small,false,false));ui.Cancel();
 std::cout<<"Loading budget, backlog admission, exact odd-sized transfers, mip completion and cancellation passed\n";
}
