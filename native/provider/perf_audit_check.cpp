// Host fixture for the real observation adapter. No provider/device contact.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <dlfcn.h>
#include <cassert>
#include <cstdarg>
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
static unsigned logs,reads,calls;
static bool marker;
static time_t seconds=100;
static int outcome;
static int args[16];
static int fake_log(int,const char*,const char*,...){++logs;errno=EBUSY;return 0;}
static int fake_access(const char*,int){errno=EBUSY;return marker?0:-1;}
static int fake_clock(clockid_t,timespec*t){t->tv_sec=seconds;t->tv_nsec=0;errno=EBUSY;return 0;}
static bool tuning_read(uintptr_t address,void*out,size_t n){
    ++reads;errno=EBUSY;if(address!=reinterpret_cast<uintptr_t>(args)||n>sizeof(args))return false;
    memcpy(out,args,n);return true;
}
using F14LoaderDlsym=void*(*)(void*,const char*,const void*);
#define __android_log_print fake_log
#define access fake_access
#define clock_gettime fake_clock
#include "f14_perf_audit.h"
#undef access
#undef clock_gettime
static int acquire(int h,int d,int*p,int n){
    ++calls;assert(errno==EDOM&&h==4&&d==500&&p==args&&(n==16||n==1000000));errno=EIO;return outcome;
}
static int release(int h){++calls;assert(errno==EDOM&&h==4);errno=EIO;return outcome;}
static int hint(int h,const char*p,int d,int t){
    ++calls;assert(errno==EDOM&&h==0x1331&&p==nullptr&&d==500&&t==-1);errno=EIO;return outcome;
}
static void reset(){perf_expired=false;perf_deadline=0;perf_events=0;calls=logs=reads=0;seconds=100;}
static void trio(int n=16){
    errno=EDOM;assert(perf_acquire_observe(4,500,args,n)==outcome&&errno==EIO);
    errno=EDOM;assert(perf_release_observe(4)==outcome&&errno==EIO);
    errno=EDOM;assert(perf_hint_observe(0x1331,nullptr,500,-1)==outcome&&errno==EIO);
}
int main(){
    perf_original_acquire=acquire;perf_original_release=release;perf_original_hint=hint;
    for(unsigned i=0;i<10000;++i){outcome=i%2?-1:233;trio();}
    assert(calls==30000&&logs==0&&reads==0);reset();marker=true;
    outcome=233;trio();assert(calls==3&&perf_events==3&&reads==1);
    marker=false;trio();assert(calls==6&&perf_events==3&&reads==1); // removal disables immediately
    marker=true;trio(1000000);assert(calls==9&&perf_events==6&&reads==1); // no unbounded read
    for(unsigned i=0;i<100;++i)trio();assert(perf_events==32&&perf_expired);
    unsigned before=reads;trio();assert(reads==before);reset();
    trio();seconds=341;trio();assert(perf_expired&&perf_events==3);
    reset();marker=false;seconds=1000;trio();assert(perf_deadline==0&&perf_events==0);
    assert(perf_audit_bind(nullptr,"perf_hint",nullptr,nullptr)==nullptr);
    printf("PASS: 30000 inactive forwards; exact args/results/errno; marker removal; count bound; event cap; TTL; no backend vote\n");
}
