#pragma once
#include <stdint.h>
struct F14PowerRequest {uint32_t magic,version,profile;int32_t duration_ms;};
struct F14PowerReply {uint32_t magic;int32_t status;};
static constexpr uint32_t f14_power_magic=0x4f435057;
#ifdef OCCAM_F14_RETAINED_POWER
static constexpr const char*f14_power_socket="/data/vendor/camera/ocfresh_camera_power.sock";
#else
static constexpr const char*f14_power_socket="/data/vendor/camera/ocfresh_power_lease.sock";
#endif
static bool f14_power_valid(const F14PowerRequest&r){return r.magic==f14_power_magic&&r.version==1&&r.profile==42&&
 (r.duration_ms==0||r.duration_ms==2000||r.duration_ms==-1);}
