// Executes the actual lease header against a deterministic Binder/clock backend.
#include <atomic>
#include <cstdint>
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <vector>
#define OCCAM_POWER_LEASE_FIXTURE
#define ANDROID_LOG_INFO 4
static int64_t tick=1000;static bool marker=true,support=true;static int writes;
static int last_duration;static std::vector<int> votes;
static std::atomic<uintptr_t> tuning_chi_base{0x100000};
static uint32_t native_profile=42;
struct F14HighConfig {uint32_t count;const void*streams;uint32_t mode;};
static bool tuning_read(uintptr_t p,void*out,size_t n){assert(p==0x100000+0x2188e8+0x16a0&&n==4);memcpy(out,&native_profile,4);errno=EBUSY;return true;}
static int logprint(int,const char*,const char*,...){errno=EBUSY;return 0;}
static int access_fake(const char*,int){errno=EBUSY;return marker?0:-1;}
static int clock_fake(clockid_t,timespec*t){t->tv_sec=tick/1000;t->tv_nsec=(tick%1000)*1000000;return 0;}
static int thread_create(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*){errno=EBUSY;return 0;}
static int thread_detach(pthread_t){return 0;}
static int sleep_fake(const timespec*,timespec*);
static int power_lease_transport(int ms){if(ms==0)return support?0:-1;++writes;votes.push_back(ms);return 0;}
#define access access_fake
#define clock_gettime clock_fake
#define nanosleep sleep_fake
#define pthread_create thread_create
#define pthread_detach thread_detach
#define __android_log_print logprint
#include "f14_power_lease.h"
static int sleep_fake(const timespec*,timespec*){
 tick+=200;
 if(tick<=3000)power_lease_request(reinterpret_cast<void*>(1),0);
 if(tick>=4000)marker=false;
 return 0;
}
static void reset(){tick=1000;marker=true;support=true;native_profile=42;power_lease_device=0;power_lease_frame_ms=0;power_lease_deadline_ms=0;power_lease_thread_started=false;votes.clear();writes=0;}
int main(){
 F14HighConfig config{3,reinterpret_cast<void*>(2),0x8021};bool enabled=true;
 for(unsigned i=0;i<10000;++i){
  reset();marker=false;errno=EDOM;power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);
  assert(errno==EDOM&&power_lease_device==0&&power_lease_deadline_ms==0);
 }
 reset();power_lease_configured(1,reinterpret_cast<void*>(1),&config,0,&enabled);assert(power_lease_device==0);
 reset();power_lease_configured(0,reinterpret_cast<void*>(1),&config,1,&enabled);assert(power_lease_device==0);
 reset();enabled=false;power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);assert(power_lease_device==0);enabled=true;
 reset();native_profile=44;power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);assert(power_lease_device==0);
 reset();errno=EDOM;power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);assert(power_lease_device==1&&errno==EDOM);
 power_lease_request(reinterpret_cast<void*>(2),0);assert(power_lease_frame_ms==0);
 power_lease_request(reinterpret_cast<void*>(1),1);assert(power_lease_frame_ms==0);
 power_lease_request(reinterpret_cast<void*>(1),0);assert(power_lease_frame_ms==1000);
 power_lease_worker(nullptr);assert((votes==std::vector<int>{2000,2000,2000,-1}));
 reset();power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);power_lease_request(reinterpret_cast<void*>(1),0);
 support=false;power_lease_worker(nullptr);assert(votes.empty()&&writes==0);
 reset();power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);power_lease_request(reinterpret_cast<void*>(1),0);
 F14HighConfig photo{3,reinterpret_cast<void*>(2),0x8001};power_lease_configured(0,reinterpret_cast<void*>(1),&photo,0,&enabled);assert(power_lease_device==0&&power_lease_frame_ms==0);
 F14PowerRequest request{f14_power_magic,1,42,2000};assert(f14_power_valid(request));
 request.duration_ms=0;assert(f14_power_valid(request));request.duration_ms=-1;assert(f14_power_valid(request));
 request.duration_ms=2001;assert(!f14_power_valid(request));request.duration_ms=2000;request.profile=43;assert(!f14_power_valid(request));
 request.profile=42;request.version=2;assert(!f14_power_valid(request));request.version=1;request.magic=0;assert(!f14_power_valid(request));
#ifdef OCCAM_F14_RETAINED_POWER
 reset();power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);
 tick=301000;power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);
 assert(power_lease_device==1&&power_lease_deadline_ms==INT64_MAX);
 marker=false;power_lease_worker(nullptr);assert(power_lease_device==0&&!power_lease_thread_started);
 marker=true;power_lease_configured(0,reinterpret_cast<void*>(1),&config,0,&enabled);assert(power_lease_device==1&&power_lease_thread_started);
#endif
 puts("PASS: 10000 marker-off configurations; exact camera/mode/profile/success gating; errno; request identity; timed renew/release; unsupported backend; bounded protocol; reconfiguration invalidation");
}
