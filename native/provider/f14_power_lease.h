#pragma once
// A real backend compatibility fallback for one exact stock F14 video session.
// It does NOT enable caller/OEM gates or change any camera request/pixel/return.
// Original CHI profile42 maps to ORMS_ACTION_CAMERA_VIDEO_PREVIEW_60FPS.
#ifndef OCCAM_POWER_LEASE_FIXTURE
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include "f14_power_protocol.h"
#ifdef OCCAM_F14_RETAINED_POWER
static constexpr const char*power_lease_marker="/data/vendor/camera/ocfresh_camera_power";
#else
static constexpr const char*power_lease_marker="/data/vendor/camera/ocfresh_power_lease_trial";
#endif
static constexpr const char*power_lease_hint="OCCAM_F14_VIDEO_PREVIEW_60FPS";
static std::atomic<uintptr_t> power_lease_device{0};
static std::atomic<int64_t> power_lease_frame_ms{0},power_lease_deadline_ms{0};
static std::atomic<bool> power_lease_thread_started{false};
static int64_t power_lease_now(){timespec t{};return clock_gettime(CLOCK_MONOTONIC,&t)==0?t.tv_sec*1000LL+t.tv_nsec/1000000:0;}
#ifndef OCCAM_POWER_LEASE_FIXTURE
static int power_lease_transport(int duration){
 int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0)return -errno;
 timeval limit{0,100000};setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&limit,sizeof(limit));
 setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&limit,sizeof(limit));
 sockaddr_un address{};address.sun_family=AF_UNIX;strcpy(address.sun_path,f14_power_socket);
 F14PowerRequest request{f14_power_magic,1,42,duration};F14PowerReply reply{};int result=-13;
 if(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0&&
    send(fd,&request,sizeof(request),MSG_NOSIGNAL)==sizeof(request)&&
    recv(fd,&reply,sizeof(reply),MSG_WAITALL)==sizeof(reply)&&reply.magic==f14_power_magic)result=reply.status;
 else result=-errno;
 close(fd);return result;
}
#endif
static void*power_lease_worker(void*){
 // Binder work is kept off the synchronous camera request thread. Every vote
 // expires in2000ms even if this process crashes or the marker is removed.
 int rc=power_lease_transport(0);bool available=rc==0;
#ifdef OCCAM_F14_RETAINED_POWER
 __android_log_print(ANDROID_LOG_INFO,"OcPowerLease","BACKEND supported=%d transport_status=%d finite_ms=2000 original_profile=42 cpu_migration_subset=1 manual_activation=1",available,rc);
#else
 __android_log_print(ANDROID_LOG_INFO,"OcPowerLease","BACKEND supported=%d transport_status=%d finite_ms=2000 original_profile=42 cpu_subset_only=1 separate_power_client=1",available,rc);
#endif
 if(!available){power_lease_device.store(0);power_lease_thread_started.store(false);return nullptr;}
 bool held=false;unsigned reports=0;int64_t last_vote=0;
 for(;;){
  int64_t now=power_lease_now(),deadline=power_lease_deadline_ms.load(),frame=power_lease_frame_ms.load();
  bool enabled=now>0&&deadline>0&&now<=deadline&&access(power_lease_marker,R_OK)==0;
  bool live=enabled&&power_lease_device.load()!=0&&frame>0&&now>=frame&&now-frame<750;
  if(live&&(!held||now-last_vote>=1000)){
   rc=power_lease_transport(2000);held=rc==0;last_vote=now;
   if(reports++<8)__android_log_print(ANDROID_LOG_INFO,"OcPowerLease","LEASE_REFRESH status=%d last_frame_age_ms=%lld duration_ms=2000",rc,static_cast<long long>(now-frame));
  }else if(held&&!live){
   rc=power_lease_transport(-1);held=false;
   __android_log_print(ANDROID_LOG_INFO,"OcPowerLease","LEASE_RELEASE status=%d active_session=%d expired_or_disabled=%d",rc,power_lease_device.load()!=0,!enabled);
  }
  if(!enabled)break;
  timespec delay{0,200000000};nanosleep(&delay,nullptr);
 }
 if(held)power_lease_transport(-1);
#ifdef OCCAM_F14_RETAINED_POWER
 power_lease_device.store(0);power_lease_frame_ms.store(0);power_lease_thread_started.store(false);
#endif
 return nullptr;
}
static void power_lease_configured(uint32_t id,const void*device,const F14HighConfig*config,uint64_t result,const bool*enabled){
 int saved=errno;
 // Reconfiguration always invalidates the previous device; failed or unrelated
 // configurations cannot inherit a vote. Only this trial's0x8021rear session.
 power_lease_device.store(0);power_lease_frame_ms.store(0);
 bool qualified=id==0&&device&&config&&config->mode==0x8021&&result==0&&enabled&&*enabled
     &&config->count>0&&config->count<=8&&access(power_lease_marker,R_OK)==0;
 uint32_t profile=0;uintptr_t base=tuning_chi_base.load();
 if(qualified)qualified=base&&tuning_read(base+0x2188e8+0x16a0,&profile,sizeof(profile))&&profile==42;
 if(qualified){
  int64_t now=power_lease_now(),empty=0;
  if(now>0){
#ifdef OCCAM_F14_RETAINED_POWER
   power_lease_deadline_ms.store(INT64_MAX);
#else
   power_lease_deadline_ms.compare_exchange_strong(empty,now+120000);
#endif
   if(now<=power_lease_deadline_ms.load()){
    power_lease_device.store(reinterpret_cast<uintptr_t>(device));
    if(!power_lease_thread_started.exchange(true)){
     pthread_t thread{};int rc=pthread_create(&thread,nullptr,power_lease_worker,nullptr);
     if(!rc)pthread_detach(thread);else {power_lease_device.store(0);power_lease_thread_started.store(false);}
    }
   }
  }
 }
 if(config&&config->mode==0x8021)__android_log_print(ANDROID_LOG_INFO,"OcPowerLease","SESSION camera=%u original_result=%llu override=%d native_profile=%u qualified=%d no_identity_override=1",
    id,static_cast<unsigned long long>(result),enabled?*enabled:-1,profile,qualified);
 errno=saved;
}
static void power_lease_request(const void*device,uint64_t result){
 int saved=errno;
 if(result==0&&device&&power_lease_device.load()==reinterpret_cast<uintptr_t>(device))power_lease_frame_ms.store(power_lease_now());
 errno=saved;
}
