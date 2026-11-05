#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
// Only this original logging function sees a different debug-property result.
// No global property is set and no image/metadata API result is replaced.
static constexpr const char* isp_log_marker="/data/vendor/camera/ocfresh_isp_logging";
static bool isp_log_site(const char*key,const char*owner,uintptr_t offset){
 return key&&owner&&!strcmp(key,"persist.sys.assert.panic.camera")
     &&!strcmp(owner,"/vendor/lib64/hw/camera.qcom.so")&&offset==0x562288;
}
static bool isp_log_marker_live(const char*path,uid_t owner,pid_t pid,time_t now){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;
 struct stat st{};char text[96]={};
 bool valid=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==owner
     &&!(st.st_mode&0022)&&st.st_size>0&&st.st_size<static_cast<off_t>(sizeof(text));
 ssize_t count=valid?read(fd,text,sizeof(text)-1):-1;close(fd);
 unsigned target=0;long long expiry=0;int consumed=0;
 return valid&&count==st.st_size&&sscanf(text,"pid=%u expires=%lld\n%n",&target,&expiry,&consumed)==2
     &&consumed==count&&target==static_cast<unsigned>(pid)&&now>0&&expiry>=now&&expiry-now<=180;
}
static bool isp_log_override(const char*key,char*value,const void*caller){
 if(!key||!value||strcmp(key,"persist.sys.assert.panic.camera"))return false;
 Dl_info info{};
 if(!dladdr(caller,&info)||!info.dli_fbase||!isp_log_site(key,info.dli_fname,
        reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(info.dli_fbase)))return false;
 if(!isp_log_marker_live(isp_log_marker,0,getpid(),time(nullptr)))return false;
 memcpy(value,"true",5);
 static std::atomic<unsigned> reports{0};
 if(reports.fetch_add(1)<8)__android_log_print(ANDROID_LOG_INFO,"OcISPLog",
     "ORIGINAL_LOG_REFRESH exact_HAL562288 debug_query_only info10088 verbose0 ttl<=180 global_properties_unchanged=1");
 return true;
}
