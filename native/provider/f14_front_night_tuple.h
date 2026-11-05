#pragma once
// Original binned Night tuples remain unchanged. The optional candidate admits
// only the four original full-YUV/UI-preview pairs to the same OEM calls.
// This does not select a usecase or modify streams, RAW, metadata or pixels.
static bool f14_front_night_full_trial(uint32_t id,const F14HighConfig* config) {
#ifdef OCCAM_F14_FRONT_NIGHT_FULL_TRIAL
 if(id!=1||!config||config->mode!=0x8019||config->count!=4||!config->streams)return false;
 static constexpr uint32_t layouts[][4]={
  {1440,1080,4608,3456}, {1080,1080,3456,3456},
  {1920,1080,4608,2592}, {2304,1040,4608,2080}
 };
 for(const auto&layout:layouts){
 unsigned input=0,raw=0,preview=0,yuv=0;bool valid=true;
 for(unsigned i=0;i<4;++i){const auto*s=config->streams[i];if(!s)return false;
  if(s->direction==1&&s->format==32&&s->width==2304&&s->height==1728)++input;
  else if(s->direction==0&&s->format==32&&s->width==2304&&s->height==1728)++raw;
  else if(s->direction==0&&s->format==34&&s->width==layout[0]&&s->height==layout[1])++preview;
  else if(s->direction==0&&s->format==35&&s->width==layout[2]&&s->height==layout[3])++yuv;
  else {valid=false;break;}
 }
 if(valid&&input==1&&raw==1&&preview==1&&yuv==1)return true;
 }
 return false;
#else
 (void)id;(void)config;return false;
#endif
}
// Observed camera1 Night configurations for all four original UI ratios.
// Each preview/YUV pair is a complete tuple from the ratio-failure trace;
// the unchanged RAW input/output is shared. The optional full-YUV candidate is
// separate and never alters the original stream dimensions or crop policy.
// Let the original OEM factory select its RAW reprocessing usecase. No stream,
// callback result, enabled flag, image data or capture setting is changed here.
static bool f14_front_night_tuple(uint32_t id,const F14HighConfig* config) {
 if(f14_front_night_full_trial(id,config))return true;
 if(id!=1||!config||config->mode!=0x8019||config->count!=4||!config->streams)return false;
 static constexpr uint32_t layouts[][4]={
  {1440,1080,1728,1728},
  {1080,1080,1728,1728},
  {1920,1080,1920,1440},
  {2304,1040,1920,1440},
  // Advertised binned outputs, same ratio as original requested YUV.
  {1440,1080,2304,1728},
  {1920,1080,2304,1296},
  {2304,1040,2304,1040}
 };
 for(const auto&layout:layouts){
  unsigned input=0,raw=0,preview=0,yuv=0;bool valid=true;
  for(unsigned i=0;i<4;++i){const auto*s=config->streams[i];if(!s){valid=false;break;}
   if(s->direction==1&&s->format==32&&s->width==2304&&s->height==1728)++input;
   else if(s->direction==0&&s->format==32&&s->width==2304&&s->height==1728)++raw;
   else if(s->direction==0&&s->format==34&&s->width==layout[0]&&s->height==layout[1])++preview;
   else if(s->direction==0&&s->format==35&&s->width==layout[2]&&s->height==layout[3])++yuv;
   else {valid=false;break;}
  }
  if(valid&&input==1&&raw==1&&preview==1&&yuv==1)return true;
 }
 return false;
}
