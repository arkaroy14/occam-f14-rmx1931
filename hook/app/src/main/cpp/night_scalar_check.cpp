// Host-only ASan/UBSan check of the actual observer wrappers, never in APK.
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <dlfcn.h>
static std::atomic<bool> active{true};
static bool highres_copy_enabled;
static int(*hook_function)(void*,void*,void**);
alignas(8) static unsigned char raw_info[0xb8],image_info[80];
static int32_t crop[4]={1,2,3,4},pre[4]={1313,2,4608,3456};
static unsigned invalid_reads,info_logs,process_logs,pre_logs,add_calls,process_calls,pre_calls;
static bool readable(const void*p,size_t n){
 if((p==raw_info&&n<=sizeof(raw_info))||(p==image_info&&n<=sizeof(image_info))
       ||(p==crop&&n<=sizeof(crop))||(p==pre&&n<=sizeof(pre)))return true;
 ++invalid_reads;return false;
}
static void log_line(const char* format,...){
 if(!strncmp(format,"NIGHT_V3_RAW_INFO call=",23))++info_logs;
 if(!strncmp(format,"NIGHT_V3_PROCESS_BEGIN",22))++process_logs;
 if(!strncmp(format,"NIGHT_PREPROCESS_BEGIN",22))++pre_logs;
 errno=ERANGE;
}
#define LOG(...) log_line(__VA_ARGS__)
#include "night_observer.h"
static uintptr_t add_original(uintptr_t h,uintptr_t i,uintptr_t f){
 assert(h==99&&i==(uintptr_t)raw_info&&f==(uintptr_t)image_info);
 ++add_calls;errno=EDOM;return UINT64_C(0xabcdef0100000007);
}
static uintptr_t process_original(uintptr_t a0,uintptr_t a1,uintptr_t a2,uintptr_t a3,
        uintptr_t a4,uintptr_t a5,uintptr_t a6,uintptr_t a7){
 assert(a0==10&&a1==11&&a2==0x12345000&&a3==13&&a4==14&&a5==(uintptr_t)crop&&a6==16&&a7==17);
 ++process_calls;errno=EIO;return UINT64_C(0x12345678fffffff9);
}
static int64_t pre_original(int32_t*p,const char*c){assert(p==pre&&c==nullptr);++pre_calls;errno=EAGAIN;return -33;}
int main(){
 // Exact SDK image-layout guards, but no pixels are allocated or readable.
 uint32_t fields[][2]={{4,0},{8,10},{20,4608},{24,3456},{64,9216}};
 for(auto&f:fields)memcpy(image_info+f[0],&f[1],4);
 night_scalar_enabled=true;original_night_add=add_original;original_night_process=process_original;
 original_night_preprocess=pre_original;
 for(unsigned i=0;i<25;++i){errno=0;auto r=audit_standard_night_add(99,(uintptr_t)raw_info,(uintptr_t)image_info);
  assert(r==UINT64_C(0xabcdef0100000007)&&errno==EDOM);}
 for(unsigned i=0;i<5;++i){errno=0;auto r=audit_standard_night_process(10,11,0x12345000,13,14,(uintptr_t)crop,16,17);
  assert(r==UINT64_C(0x12345678fffffff9)&&errno==EIO&&night_depth==0);}
 for(unsigned i=0;i<5;++i){errno=0;assert(audit_night_preprocess(pre,nullptr)==-33&&errno==EAGAIN&&night_depth==0);}
 assert(add_calls==25&&process_calls==5&&pre_calls==5&&invalid_reads==0);
 assert(info_logs==20&&process_logs==2&&pre_logs==2);
 night_scalar_enabled=false;night_observer_enabled=false;
 for(unsigned i=0;i<10000;++i)assert(audit_night_symbol((void*)0x1234,"ARC_SN_Process",nullptr)==(void*)0x1234);
}
