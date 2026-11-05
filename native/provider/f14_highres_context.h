// Original F14 callback ABI verified from chi_hal_override_entry@53f58.
// OEM identity contexts for exact rear64M and observed front Night RAW tuples.
#pragma once
#include <elf.h>
#include <link.h>
#include <stddef.h>
struct Query { const char* name; void* address; };
static int lookup(dl_phdr_info* info, size_t, void* argument) {
    if (!info->dlpi_name || (!strstr(info->dlpi_name, "/linker64") && !strstr(info->dlpi_name, "ld-android.so"))) return 0;
    const ElfW(Dyn)* dynamic = nullptr;
    for (size_t i = 0; i < info->dlpi_phnum; ++i)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
            dynamic = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    if (!dynamic) return 0;
    const ElfW(Sym)* symbols = nullptr; const char* strings = nullptr;
    const uint32_t* gnu = nullptr; size_t count = 0, string_size = 0;
    for (auto* entry = dynamic; entry->d_tag != DT_NULL; ++entry) {
        uintptr_t pointer = info->dlpi_addr + entry->d_un.d_ptr;
        switch (entry->d_tag) {
            case DT_SYMTAB: symbols = reinterpret_cast<const ElfW(Sym)*>(pointer); break;
            case DT_STRTAB: strings = reinterpret_cast<const char*>(pointer); break;
            case DT_STRSZ: string_size = entry->d_un.d_val; break;
            case DT_HASH: count = reinterpret_cast<const uint32_t*>(pointer)[1]; break;
            case DT_GNU_HASH: gnu = reinterpret_cast<const uint32_t*>(pointer); break;
        }
    }
    if (!symbols || !strings) return 0;
    if (!count && gnu && gnu[0] < 65536 && gnu[2] < 65536) {
        const uint32_t* buckets = reinterpret_cast<const uint32_t*>(reinterpret_cast<const ElfW(Addr)*>(gnu + 4) + gnu[2]);
        const uint32_t* chains = buckets + gnu[0]; uint32_t largest = 0;
        for (uint32_t i = 0; i < gnu[0]; ++i) if (buckets[i] > largest) largest = buckets[i];
        if (largest >= gnu[1] && largest < 65536) {
            while (largest < 65536 && !(chains[largest - gnu[1]] & 1)) ++largest;
            count = largest + 1;
        }
    }
    if (count > 65536) return 0;
    auto* query = static_cast<Query*>(argument);
    for (size_t i = 0; i < count; ++i) {
        if (symbols[i].st_shndx == SHN_UNDEF || symbols[i].st_name >= string_size) continue;
        if (!strcmp(strings + symbols[i].st_name, query->name)) {
            query->address = reinterpret_cast<void*>(info->dlpi_addr + symbols[i].st_value);
            return 1;
        }
    }
    return 0;
}
template <class T> static T resolve(const char* name) {
    Query query{name, nullptr}; dl_iterate_phdr(lookup, &query);
    return reinterpret_cast<T>(query.address);
}

static thread_local bool f14_highres_context;
static std::atomic<const void*> f14_highres_device{nullptr};
static thread_local bool f14_front_context;
static std::atomic<const void*> f14_front_device{nullptr};
// Exact original rear SAT/front Photo/front Night tuples observed on F14. This context is used
// only by the preview-node OEM identity call, not the CHI factory helpers.
static thread_local bool f14_preview_context;
static std::atomic<const void*> f14_preview_device{nullptr};
struct F14HighStream { int32_t direction; uint32_t width,height; int32_t format; };
struct F14HighConfig { uint32_t count; const F14HighStream*const* streams; uint32_t mode; };
static_assert(offsetof(F14HighConfig,streams)==8 && offsetof(F14HighConfig,mode)==16,"camera3configABI");
#include "f14_environment_audit.h"
#include "f14_tuning_entry.h"
#include "f14_ultradark_oem.h"
#include "f14_metadata_ops_audit.h"
static bool f14_highres_tuple(uint32_t id,const F14HighConfig* config) {
 if(id!=0 || !config || config->mode!=0x8001 || config->count!=3 || !config->streams)return false;
 unsigned full=0,binned=0,preview=0;
 for(unsigned i=0;i<3;++i){auto*s=config->streams[i];if(!s||s->direction!=0)return false;
  if(s->format==35&&s->width==9280&&s->height==6944)++full;
  else if(s->format==35&&s->width==4640&&s->height==3472)++binned;
  else if(s->format==34&&s->width>0&&s->height>0&&s->width<=4608&&s->height<=3456)++preview;
  else return false;
 }
 return full==1&&binned==1&&preview==1;
}
#include "f14_front_night_tuple.h"
// Public-sized front still streams observed in the camera service. This does
// not enable the failed4608x3456 front YUV tuple or change a stream size.
static bool f14_front_still_tuple(uint32_t id,const F14HighConfig* config) {
 if(id!=1||!config||config->mode!=0x8001||config->count!=3||!config->streams)return false;
 unsigned full=0,preview=0,small=0;
 for(unsigned i=0;i<3;++i){const auto*s=config->streams[i];if(!s||s->direction!=0)return false;
  if(s->format==35&&s->width==1728&&s->height==1728)++full;
  else if(s->format==35&&s->width==1440&&s->height==1080)++small;
  else if(s->format==34&&s->width==1440&&s->height==1080)++preview;
  else return false;
 }
 return full==1&&small==1&&preview==1;
}
// Original preview node needs its final OEM identity query to publish the
// existing fb.face.info blob. Match only measured camera1 Photo layouts,
// order independently. The final Full example in the handoff merely swaps
// the two YUV streams and is covered by the same tuple. No dimensions change.
static bool f14_front_photo_preview_tuple(uint32_t id,const F14HighConfig* config) {
 if(id!=1||!config||config->mode!=0x8001||config->count!=3||!config->streams)return false;
 static constexpr uint32_t layouts[][6]={
  {1440,1080,1440,1080,1728,1728},
  {1080,1080,1080,1080,1728,1728},
  {1920,1080,1920,1080,1920,1440},
  {2304,1040,2304,1040,1920,1440},
  {1440,1080,1440,1080,2304,1728},
  {1920,1080,1920,1080,2304,1296},
  {2304,1040,2304,1040,2304,1040}
 };
 for(const auto&layout:layouts){
  bool used[3]={false,false,false};bool valid=true;
  for(unsigned i=0;i<3;++i){const auto*s=config->streams[i];if(!s||s->direction!=0){valid=false;break;}
   bool found=false;
   for(unsigned target=0;target<3;++target){
    int format=target==0?34:35;
    if(!used[target]&&s->format==format&&s->width==layout[target*2]&&s->height==layout[target*2+1]){
     used[target]=true;found=true;break;
    }
   }
   if(!found){valid=false;break;}
  }
  if(valid&&used[0]&&used[1]&&used[2])return true;
 }
 return false;
}
// Exact original front Photo QCFA ordering compatibility. Every stream is an observed original
// request; only their pointer order is changed during original initialization.
static bool f14_front_full_tuple(uint32_t id,const F14HighConfig* config) {
 if(id!=1||!config||config->mode!=0x8001||config->count!=3||!config->streams)return false;
 static constexpr uint32_t layouts[][4]={
  {1440,1080,4608,3456}, {1080,1080,3456,3456},
  {1920,1080,4608,2592}, {2304,1040,4608,2080}
 };
 for(const auto&layout:layouts){
  unsigned preview=0,small=0,full=0;bool valid=true;
  for(unsigned i=0;i<3;++i){const auto*s=config->streams[i];if(!s||s->direction!=0){valid=false;break;}
   if(s->format==34&&s->width==layout[0]&&s->height==layout[1])++preview;
   else if(s->format==35&&s->width==layout[0]&&s->height==layout[1])++small;
   else if(s->format==35&&s->width==layout[2]&&s->height==layout[3])++full;
   else {valid=false;break;}
  }
  if(valid&&preview==1&&small==1&&full==1)return true;
 }
 return false;
}
static bool f14_sat_photo_tuple(uint32_t id,const F14HighConfig* config) {
 if(id!=7||!config||config->mode!=0x8001||config->count!=6||!config->streams)return false;
 // Whole main/UW/tele triples observed in the untouched app's ratio trace.
 // Preview geometry remains the app's choice, as in the original highres
 // tuple predicate; no dimensions, ratio or metadata are changed here.
 static constexpr uint32_t layouts[][6]={
  {4608,3456,3264,2448,4000,3000},
  {4608,2592,3264,1836,4000,2250},
  {3456,3456,2448,2448,3000,3000},
  {4608,2080,3200,1440,4000,1808}
 };
 for(const auto&layout:layouts){
  unsigned input=0,raw=0,preview=0,main=0,wide=0,tele=0;
  bool valid=true;
  for(unsigned i=0;i<6;++i){const auto*s=config->streams[i];if(!s){valid=false;break;}
   if(s->direction==1&&s->format==32&&s->width==4608&&s->height==3456)++input;
   else if(s->direction==0&&s->format==32&&s->width==4608&&s->height==3456)++raw;
   else if(s->direction==0&&s->format==34&&s->width>0&&s->height>0&&s->width<=4608&&s->height<=3456)++preview;
   else if(s->direction==0&&s->format==35&&s->width==layout[0]&&s->height==layout[1])++main;
   else if(s->direction==0&&s->format==35&&s->width==layout[2]&&s->height==layout[3])++wide;
   else if(s->direction==0&&s->format==35&&s->width==layout[4]&&s->height==layout[5])++tele;
   else {valid=false;break;}
  }
  if(valid&&input==1&&raw==1&&preview==1&&main==1&&wide==1&&tele==1)return true;
 }
 return false;
}
using F14Initialize=uint64_t(*)(uint32_t,const void*,const void*,F14HighConfig*,bool*,void**);
using F14Process=uint64_t(*)(const void*,void*,void*);
using F14Entry=void(*)(void*);
static F14Initialize f14_original_initialize;
static F14Process f14_original_process;
static F14Entry f14_original_entry;
#ifdef OCCAM_F14_POWER_LEASE
#include "f14_power_lease.h"
#endif
static void f14_front_full_stream_log(const F14HighConfig*config,const char*phase){
 if(!config||config->count>8||!config->streams)return;
 for(unsigned i=0;i<config->count;++i){const auto*s=config->streams[i];if(!s)continue;
  // Original F14 camera3 stream ABI, already established by retained decompiles.
  uint32_t usage=0,max_buffers=0,dataspace=0;
  memcpy(&usage,reinterpret_cast<const char*>(s)+0x10,4);
  memcpy(&max_buffers,reinterpret_cast<const char*>(s)+0x14,4);
  memcpy(&dataspace,reinterpret_cast<const char*>(s)+0x20,4);
  __android_log_print(ANDROID_LOG_INFO,"OcF14Front","FRONT_NIGHT_FULL_%s index=%u direction=%d size=%ux%u format=%d usage=%x max_buffers=%u dataspace=%x",
   phase,i,s->direction,s->width,s->height,s->format,usage,max_buffers,dataspace);
 }
}
static uint64_t f14_scoped_initialize(uint32_t id,const void*device,const void*ops,F14HighConfig*config,bool*enabled,void**private_data){
 int entry_errno=errno;EnvContext previous_env=env_context;env_context=env_begin(id,device,config);
 tuning_configure(id,config);
 ud_oem_session.store(false);
 bool previous=f14_highres_context;bool scope=f14_highres_tuple(id,config);
 bool previous_front=f14_front_context;
 // The observed non4:3 Photo tuples also need the original OEM factory;
 // otherwise CHI selects plain Snapshot and never creates the preview node.
 bool front_full=f14_front_full_tuple(id,config);
 bool front_still=f14_front_still_tuple(id,config)||f14_front_photo_preview_tuple(id,config)||front_full;
 bool front_night=f14_front_night_tuple(id,config);
 bool full_night_trial=f14_front_night_full_trial(id,config);
 if(full_night_trial)f14_front_full_stream_log(config,"BEFORE");
 bool front_scope=front_night||front_still;
 bool previous_preview=f14_preview_context;
 bool sat_preview=f14_sat_photo_tuple(id,config);
 bool front_preview=f14_front_photo_preview_tuple(id,config)||front_full||front_night;
 // Rear64M uses the same original preview-node OEM gate; exact existing
 // rear0/op8001/full9280+binned4640+PRIVATE tuple only.
 bool preview_scope=sat_preview||front_preview||scope;
 // A later configuration must not inherit either preview identity context.
 f14_preview_device.store(nullptr);
 f14_preview_context=preview_scope;
 bool observe_front_night=id==1&&config&&config->mode==0x8019&&config->count<=8&&config->streams;
 bool observe_front_still=id==1&&config&&config->mode==0x8001&&config->count<=8&&config->streams;
 const char*front_label=observe_front_night?"FRONT_NIGHT":"FRONT_STILL";
 if(observe_front_night||observe_front_still){
  __android_log_print(ANDROID_LOG_INFO,"OcF14Front","%s_CONFIG streams=%u mode=%x identity_override=%d",front_label,config->count,config->mode,front_scope);
  for(unsigned i=0;i<config->count;++i){const auto*s=config->streams[i];if(s)
   __android_log_print(ANDROID_LOG_INFO,"OcF14Front","%s_STREAM index=%u direction=%d width=%u height=%u format=%d",front_label,i,s->direction,s->width,s->height,s->format);
  }
 }
 if(f14_highres_device.load()==device)f14_highres_device.store(nullptr);
 if(f14_front_device.load()==device)f14_front_device.store(nullptr);
 f14_highres_context=scope;
 f14_front_context=front_scope;
 if(front_still)__android_log_print(ANDROID_LOG_INFO,"OcF14Front","FRONT_STILL_CONTEXT_BEGIN observed3publicstreams camera1 mode8001; original factory delegated");
 if(scope)__android_log_print(ANDROID_LOG_INFO,"OcF14High","HIGHRES_CONTEXT_BEGIN rear0 mode8001 exact3streams; original factory decides QCFA");
 if(sat_preview)__android_log_print(ANDROID_LOG_INFO,"OcF14Preview","PREVIEW_CONTEXT_BEGIN rearSAT7 mode8001 exact6streams; node identity only");
 if(front_preview)__android_log_print(ANDROID_LOG_INFO,"OcF14Preview","PREVIEW_CONTEXT_BEGIN front camera1 mode=%x original measured tuple; original face blob",config->mode);
 // Reorder only the caller's three stream pointers during the synchronous
 // original factory call, then restore them. No temporary array escapes.
 const F14HighStream* previous_streams[3]={};
 auto** mutable_streams=front_full?const_cast<const F14HighStream**>(config->streams):nullptr;
 if(front_full){
  for(unsigned i=0;i<3;++i)previous_streams[i]=config->streams[i];
  unsigned j=0;
  for(unsigned i=0;i<3;++i)if(previous_streams[i]->format==35&&previous_streams[i]->width>=3456)mutable_streams[j++]=previous_streams[i];
  for(unsigned i=0;i<3;++i)if(!(previous_streams[i]->format==35&&previous_streams[i]->width>=3456))mutable_streams[j++]=previous_streams[i];
  __android_log_print(ANDROID_LOG_INFO,"OcF14Front","FRONT_FULL_ORDER original full YUV first; original stream objects preserved; original selector decides");
 }
 errno=entry_errno;uint64_t result=f14_original_initialize(id,device,ops,config,enabled,private_data);int saved=errno;
 if(full_night_trial)f14_front_full_stream_log(config,"AFTER");
#ifdef OCCAM_F14_POWER_LEASE
 power_lease_configured(id,device,config,result,enabled);
#endif
 if(front_full)for(unsigned i=0;i<3;++i)mutable_streams[i]=previous_streams[i];
 if(observe_front_night||observe_front_still)__android_log_print(ANDROID_LOG_INFO,"OcF14Front","%s_CONFIG_RESULT original=%llu override_enabled=%d",front_label,static_cast<unsigned long long>(result),enabled?*enabled:-1);
 ud_oem_session.store(id==7&&config&&config->mode==0x8019&&result==0&&enabled&&*enabled);
 if(scope&&result==0&&enabled&&*enabled)f14_highres_device.store(device);
 if(front_scope&&result==0&&enabled&&*enabled)f14_front_device.store(device);
 if(preview_scope&&result==0&&enabled&&*enabled)f14_preview_device.store(device);
 if(front_still)__android_log_print(ANDROID_LOG_INFO,"OcF14Front","FRONT_STILL_CONTEXT_END original=%llu override_enabled=%d",static_cast<unsigned long long>(result),enabled?*enabled:-1);
 if(scope)__android_log_print(ANDROID_LOG_INFO,"OcF14High","HIGHRES_CONTEXT_END originalResult=%llu override=%d",static_cast<unsigned long long>(result),enabled?*enabled:-1);
 f14_highres_context=previous;f14_front_context=previous_front;f14_preview_context=previous_preview;env_context=previous_env;errno=saved;return result;
}
static uint64_t f14_scoped_process(const void*device,void*request,void*private_data){
 int entry_errno=errno;EnvContext previous_env=env_context;env_context=env_for_device(device);
 bool previous=f14_highres_context;
 bool previous_front=f14_front_context;
 bool previous_preview=f14_preview_context;
 f14_highres_context=device&&device==f14_highres_device.load();errno=entry_errno;
 f14_front_context=device&&device==f14_front_device.load();
 f14_preview_context=device&&device==f14_preview_device.load();
 tuning_process_request(request);errno=entry_errno;
 uint64_t result=f14_original_process(device,request,private_data);int saved=errno;
#ifdef OCCAM_F14_POWER_LEASE
 power_lease_request(device,result);
#endif
 f14_highres_context=previous;f14_front_context=previous_front;f14_preview_context=previous_preview;env_context=previous_env;errno=saved;return result;
}
static void f14_scoped_entry(void* callbacks){
 f14_original_entry(callbacks);int saved=errno;
 if(callbacks){auto**v=static_cast<void**>(callbacks);Dl_info initialize{},process{};
  if(dladdr(v[3],&initialize)&&initialize.dli_fbase&&initialize.dli_fname
   &&!strcmp(initialize.dli_fname,"/vendor/lib64/hw/com.qti.chi.override.so")
   &&reinterpret_cast<uintptr_t>(v[3])-reinterpret_cast<uintptr_t>(initialize.dli_fbase)==0x54518
   &&dladdr(v[6],&process)&&process.dli_fbase==initialize.dli_fbase
   &&reinterpret_cast<uintptr_t>(v[6])-reinterpret_cast<uintptr_t>(process.dli_fbase)==0x57150){
    f14_original_initialize=reinterpret_cast<F14Initialize>(v[3]);f14_original_process=reinterpret_cast<F14Process>(v[6]);
    tuning_chi_base.store(reinterpret_cast<uintptr_t>(initialize.dli_fbase));
    v[3]=reinterpret_cast<void*>(f14_scoped_initialize);v[6]=reinterpret_cast<void*>(f14_scoped_process);
    __android_log_print(ANDROID_LOG_INFO,"OcF14High","HIGHRES_CALLBACK_BOUND exactF14 slots3/6, initialize54518 process57150");
  }
 }
 errno=saved;
}
using F14LoaderDlsym=void*(*)(void*,const char*,const void*);
#ifdef OCCAM_F14_PERF_AUDIT
#include "f14_perf_audit.h"
#endif
static std::atomic<F14LoaderDlsym> f14_loader_symbol{nullptr};
extern "C" void* dlsym(void*handle,const char*symbol){
 int entry_errno=errno;auto loader=f14_loader_symbol.load();
 if(!loader){loader=resolve<F14LoaderDlsym>("__loader_dlsym");if(!loader){errno=entry_errno;return nullptr;}f14_loader_symbol.store(loader);}
 const void*caller=__builtin_return_address(0);errno=entry_errno;void*result=loader(handle,symbol,caller);int saved=errno;
#ifdef OCCAM_F14_PERF_AUDIT
 result=perf_audit_bind(result,symbol,caller,loader);
#endif
 if(result&&symbol&&!strcmp(symbol,"ChiEntry")){
  Dl_info from{},to{};
  bool from_ok=dladdr(caller,&from),to_ok=dladdr(result,&to);
  __android_log_print(ANDROID_LOG_INFO,"OcMetaOps","CHI_ENTRY_RESOLVE caller=%s rva=%lx target=%s rva=%lx",
    from_ok&&from.dli_fname?from.dli_fname:"?",static_cast<unsigned long>(from_ok?reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(from.dli_fbase):0),
    to_ok&&to.dli_fname?to.dli_fname:"?",static_cast<unsigned long>(to_ok?reinterpret_cast<uintptr_t>(result)-reinterpret_cast<uintptr_t>(to.dli_fbase):0));
  if(from_ok&&from.dli_fname&&from.dli_fbase
    &&!strcmp(from.dli_fname,"/vendor/lib64/hw/com.qti.chi.override.so")
    &&reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(from.dli_fbase)>=0x5b280
    &&reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(from.dli_fbase)<0x5bef0
    &&to_ok&&to.dli_fname&&to.dli_fbase
    &&!strcmp(to.dli_fname,"/vendor/lib64/hw/camera.qcom.so")
    &&reinterpret_cast<uintptr_t>(result)-reinterpret_cast<uintptr_t>(to.dli_fbase)==0x4adf80){
      metadata_hal_base.store(reinterpret_cast<uintptr_t>(to.dli_fbase));
      metadata_hal_entry.store(reinterpret_cast<HalEntry>(result));
      result=reinterpret_cast<void*>(metadata_observe_entry);
  }
 }
 if(result&&symbol&&!strcmp(symbol,"chi_hal_override_entry")){
  Dl_info from{},to{};
  if(dladdr(caller,&from)&&from.dli_fname&&!strcmp(from.dli_fname,"/vendor/lib64/hw/camera.qcom.so")
    &&dladdr(result,&to)&&to.dli_fname&&!strcmp(to.dli_fname,"/vendor/lib64/hw/com.qti.chi.override.so")){
    f14_original_entry=reinterpret_cast<F14Entry>(result);result=reinterpret_cast<void*>(f14_scoped_entry);
  }
 }
 errno=saved;return result;
}
