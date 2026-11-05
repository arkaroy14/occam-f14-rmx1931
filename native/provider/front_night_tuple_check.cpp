#include <stdint.h>
#include <assert.h>
#include <algorithm>
struct F14HighStream {int32_t direction;uint32_t width,height;int32_t format;};
struct F14HighConfig {uint32_t count;const F14HighStream*const*streams;uint32_t mode;};
#include "f14_front_night_tuple.h"
int main(){
 F14HighStream in{1,2304,1728,32},raw{0,2304,1728,32},prev{0,1440,1080,34},yuv{0,4608,3456,35};
 F14HighStream*parts[]={&in,&raw,&prev,&yuv};
 int order[]={0,1,2,3};unsigned cases=0;
 do{
  const F14HighStream*streams[]={parts[order[0]],parts[order[1]],parts[order[2]],parts[order[3]]};
  F14HighConfig c{4,streams,0x8019};
#ifdef OCCAM_F14_FRONT_NIGHT_FULL_TRIAL
  assert(f14_front_night_full_trial(1,&c)&&f14_front_night_tuple(1,&c));
#else
  assert(!f14_front_night_full_trial(1,&c)&&!f14_front_night_tuple(1,&c));
#endif
  assert(!f14_front_night_tuple(0,&c));c.mode=0x8001;assert(!f14_front_night_tuple(1,&c));c.mode=0x8019;
  in.width=4608;assert(!f14_front_night_tuple(1,&c));in.width=2304;
  prev.width=1920;assert(!f14_front_night_tuple(1,&c));prev.width=1440;
  yuv.height=2592;assert(!f14_front_night_tuple(1,&c));yuv.height=3456;
  c.count=5;assert(!f14_front_night_tuple(1,&c));c.count=4;
  const F14HighStream*bad[]={&in,&raw,&prev,&raw};c.streams=bad;assert(!f14_front_night_tuple(1,&c));
  ++cases;
 }while(std::next_permutation(order,order+4));
 assert(cases==24);assert(!f14_front_night_tuple(1,nullptr));
 const uint32_t full[][4]={{1440,1080,4608,3456},{1080,1080,3456,3456},
  {1920,1080,4608,2592},{2304,1040,4608,2080}};
 for(auto&l:full){prev.width=l[0];prev.height=l[1];yuv.width=l[2];yuv.height=l[3];
  int o[]={0,1,2,3};
  do{const F14HighStream*s[]={parts[o[0]],parts[o[1]],parts[o[2]],parts[o[3]]};F14HighConfig c{4,s,0x8019};
#ifdef OCCAM_F14_FRONT_NIGHT_FULL_TRIAL
   assert(f14_front_night_tuple(1,&c));
#else
   assert(!f14_front_night_tuple(1,&c));
#endif
   assert(!f14_front_night_tuple(0,&c));
   ++yuv.width;assert(!f14_front_night_tuple(1,&c));--yuv.width;
   ++prev.height;assert(!f14_front_night_tuple(1,&c));--prev.height;
  }while(std::next_permutation(o,o+4));
 }
 const uint32_t layouts[][4]={{1440,1080,1728,1728},{1080,1080,1728,1728},{1920,1080,1920,1440},
  {2304,1040,1920,1440},{1440,1080,2304,1728},{1920,1080,2304,1296},{2304,1040,2304,1040}};
 for(auto&l:layouts){prev.width=l[0];prev.height=l[1];yuv.width=l[2];yuv.height=l[3];
  const F14HighStream*streams[]={&in,&raw,&prev,&yuv};F14HighConfig c{4,streams,0x8019};assert(f14_front_night_tuple(1,&c));}
 return 0;
}
