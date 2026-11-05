// Optional read-only provider gate coverage. No property/selection override here.
#pragma once
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
static std::atomic<bool> env_audit_enabled{false};
static std::atomic<unsigned long> env_audit_sequence{0},env_audit_queries{0};
struct EnvContext { unsigned long session; uint32_t camera,mode; };
static thread_local EnvContext env_context{0,~0u,0};
struct EnvDevice { const void*device; EnvContext context; };
static EnvDevice env_devices[16]{};
static pthread_mutex_t env_lock=PTHREAD_MUTEX_INITIALIZER;
static uint64_t env_seen[256]{};
static unsigned env_used=0;
static unsigned long env_repeated=0,env_overflow=0;
static uint64_t env_owner_hash=0;
static bool env_interesting_key(const char*k){
 return k&&(!strcmp(k,"oppo.camera.packname")||!strcmp(k,"oplus.camera.packname")
  ||!strncmp(k,"vendor.oppo.camera.",19)||!strncmp(k,"persist.vendor.camera.",22)
  ||!strcmp(k,"ro.product.model")||!strcmp(k,"ro.product.manufacturer")
  ||!strcmp(k,"ro.build.version.sdk"));
}
static EnvContext env_begin(uint32_t camera,const void*device,const F14HighConfig*config){
 int saved=errno;
 bool enabled=access("/data/vendor/camera/ocfresh_environment_audit",R_OK)==0;
 uint64_t owner_hash=1469598103934665603ULL;
 if(enabled){
  FILE*marker=fopen("/data/vendor/camera/ocfresh_environment_audit","r");
  if(marker){char text[128];size_t count=fread(text,1,sizeof(text),marker);fclose(marker);for(size_t i=0;i<count;++i)owner_hash=(owner_hash^static_cast<unsigned char>(text[i]))*1099511628211ULL;}
  else enabled=false;
 }
 env_audit_enabled.store(enabled);
 EnvContext context{0,camera,config?config->mode:0};
 if(enabled){
  pthread_mutex_lock(&env_lock);
  if(env_owner_hash!=owner_hash){
   env_owner_hash=owner_hash;env_used=0;env_repeated=0;env_overflow=0;env_audit_queries.store(0);
   memset(env_seen,0,sizeof(env_seen));
   __android_log_print(ANDROID_LOG_INFO,"OcEnvGate","AUDIT_START owner_hash=%llx fresh_budget=256",static_cast<unsigned long long>(owner_hash));
  }
  __android_log_print(ANDROID_LOG_INFO,"OcEnvGate","COUNTS queries=%lu duplicate=%lu overflow=%lu unique=%u limit=256",env_audit_queries.load(),env_repeated,env_overflow,env_used);
  context.session=++env_audit_sequence;
  if(camera<16)env_devices[camera]={device,context};
  pthread_mutex_unlock(&env_lock);
  __android_log_print(ANDROID_LOG_INFO,"OcEnvGate","SESSION id=%lu camera=%u mode=%x streams=%u device=%p",context.session,camera,context.mode,config?config->count:0,device);
  if(config&&config->streams&&config->count<=16)for(unsigned i=0;i<config->count;++i){auto*s=config->streams[i];if(s)__android_log_print(ANDROID_LOG_INFO,"OcEnvGate","STREAM session=%lu index=%u direction=%d width=%u height=%u format=%d",context.session,i,s->direction,s->width,s->height,s->format);}
 }
 errno=saved;return context;
}
static EnvContext env_for_device(const void*device){
 EnvContext result{0,~0u,0};if(!env_audit_enabled.load())return result;
 int saved=errno;pthread_mutex_lock(&env_lock);
 for(const auto&row:env_devices)if(row.device==device){result=row.context;break;}
 pthread_mutex_unlock(&env_lock);errno=saved;return result;
}
static int env_property_result(const char*key,const char*value,int result,const void*caller,bool modified){
 if(!env_audit_enabled.load()||!env_interesting_key(key))return result;
 int saved=errno;Dl_info owner{};
 if(!dladdr(caller,&owner)||!owner.dli_fname||!owner.dli_fbase){errno=saved;return result;}
 uintptr_t offset=reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(owner.dli_fbase);
 uint64_t hash=1469598103934665603ULL;
 for(const char*p=owner.dli_fname;*p;++p)hash=(hash^static_cast<unsigned char>(*p))*1099511628211ULL;
 for(const char*p=key;*p;++p)hash=(hash^static_cast<unsigned char>(*p))*1099511628211ULL;
 if(value)for(const char*p=value;*p;++p)hash=(hash^static_cast<unsigned char>(*p))*1099511628211ULL;
 hash^=offset^env_context.session^(static_cast<uint64_t>(modified)<<63);
 bool emit=false,overflow=false;pthread_mutex_lock(&env_lock);
 env_audit_queries.fetch_add(1);bool seen=false;
 for(unsigned i=0;i<env_used;++i)if(env_seen[i]==hash){seen=true;break;}
 if(seen)++env_repeated;
 else if(env_used<256){env_seen[env_used++]=hash;emit=true;}
 else {overflow=++env_overflow==1;}
 pthread_mutex_unlock(&env_lock);
 if(emit)__android_log_print(ANDROID_LOG_INFO,"OcEnvGate","PROPERTY session=%lu camera=%u mode=%x caller=%s rva=%lx key=%s value=%s result=%d compatibility_override=%d",env_context.session,env_context.camera,env_context.mode,owner.dli_fname,static_cast<unsigned long>(offset),key,value?value:"NULL",result,modified);
 if(overflow)__android_log_print(ANDROID_LOG_WARN,"OcEnvGate","COVERAGE_LIMIT unique_limit=256 more_events_unobserved; COUNTS continues next configure");
 errno=saved;return result;
}
