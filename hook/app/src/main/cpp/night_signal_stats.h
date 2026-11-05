#pragma once
// Pure bounded statistics on the verified RAW4608x3456/9216 layout.
// Caller validates the original locked buffer lifetime/readable extent first.
namespace nightsignal {
struct Site {uint64_t sum=0,hash=14695981039346656037ULL;uint32_t low=65535,high=0,below_black=0,at_white=0,above_white=0,hist[16]={};};
struct Result {Site sites[4];};
static bool sample(const uint8_t*p,size_t bytes,uint32_t width,uint32_t height,uint32_t pitch,Result&out){
 if(!p||width!=4608||height!=3456||pitch!=9216||bytes<31850496)return false;
 for(unsigned gy=0;gy<16;++gy)for(unsigned gx=0;gx<16;++gx){
  // Central80% avoids the known SDK crop/outside-image margins. No assumption
  // about scene brightness or pedestal is inferred from sample minima.
  unsigned row=((height/10)+gy*(height*8/10)/15)&~1U;
  unsigned col=((width/10)+gx*(width*8/10)/15)&~1U;
  for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){
   uint16_t value=0;memcpy(&value,p+(row+dy)*pitch+(col+dx)*2,2);Site&s=out.sites[dy*2+dx];
   s.sum+=value;s.low=value<s.low?value:s.low;s.high=value>s.high?value:s.high;
   s.below_black+=value<=63;s.at_white+=value>=1023;s.above_white+=value>1023;
   ++s.hist[value>1023?15:value/64];s.hash=(s.hash^value)*1099511628211ULL;
  }
 }
 return true;
}
}
