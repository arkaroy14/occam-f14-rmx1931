#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <vector>
#include "night_signal_stats.h"
int main(){
 std::vector<uint16_t> pixels(4608*3456);
 for(unsigned y=0;y<3456;++y)for(unsigned x=0;x<4608;++x)
  pixels[y*4608+x]=(y%2)*2+x%2==0?32:((y%2)*2+x%2==1?128:((y%2)*2+x%2==2?1023:2048));
 auto original=pixels;const auto*p=reinterpret_cast<const uint8_t*>(pixels.data());nightsignal::Result r;
 assert(nightsignal::sample(p,pixels.size()*2,4608,3456,9216,r));
 unsigned expected[]={32,128,1023,2048};
 for(unsigned i=0;i<4;++i){auto&s=r.sites[i];assert(s.sum==256ULL*expected[i]&&s.low==expected[i]&&s.high==expected[i]);
  assert(s.below_black==(i==0?256:0)&&s.at_white==(i>=2?256:0)&&s.above_white==(i==3?256:0));
  unsigned total=0;for(auto h:s.hist)total+=h;assert(total==256);
 }
 assert(pixels==original);
 nightsignal::Result other;
 assert(!nightsignal::sample(nullptr,pixels.size()*2,4608,3456,9216,other));
 assert(!nightsignal::sample(p,pixels.size()*2-1,4608,3456,9216,other));
 assert(!nightsignal::sample(p,pixels.size()*2,4607,3456,9216,other));
 assert(!nightsignal::sample(p,pixels.size()*2,4608,3455,9216,other));
 assert(!nightsignal::sample(p,pixels.size()*2,4608,3456,9215,other));
 // Deterministic spatial hashes change if a sampled original pixel changes.
 unsigned row=(3456/10)&~1U,col=(4608/10)&~1U;pixels[row*4608+col]=33;
 assert(nightsignal::sample(p,pixels.size()*2,4608,3456,9216,other));
 assert(other.sites[0].hash!=r.sites[0].hash&&other.sites[1].hash==r.sites[1].hash);
}
