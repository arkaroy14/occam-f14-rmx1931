#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
enum{ANDROID_LOG_INFO=4};
static int __android_log_print(int,const char*,const char*,...){return 0;}
#include "f14_isp_log_scope.h"
int main(){
 const char*key="persist.sys.assert.panic.camera",*owner="/vendor/lib64/hw/camera.qcom.so";
 assert(isp_log_site(key,owner,0x562288));
 assert(!isp_log_site(key,owner,0x562284)&&!isp_log_site(key,owner,0x56228c));
 assert(!isp_log_site("persist.sys.assert.panic",owner,0x562288));
 assert(!isp_log_site(key,"/vendor/lib64/hw/com.qti.chi.override.so",0x562288));
 assert(!isp_log_site(nullptr,owner,0x562288)&&!isp_log_site(key,nullptr,0x562288));
 char file[]="/tmp/ocisp-check-XXXXXX";int fd=mkstemp(file);assert(fd>=0);close(fd);
 auto write=[&](const char*text){FILE*f=fopen(file,"w");assert(f);fputs(text,f);fclose(f);chmod(file,0644);};
 write("pid=123 expires=1100\n");assert(isp_log_marker_live(file,getuid(),123,1000));
 assert(!isp_log_marker_live(file,getuid(),124,1000));
 assert(!isp_log_marker_live(file,getuid()+1,123,1000));
 assert(!isp_log_marker_live(file,getuid(),123,1200));
 assert(!isp_log_marker_live(file,getuid(),123,900));
 assert(!isp_log_marker_live(file,getuid(),123,0));
 chmod(file,0666);assert(!isp_log_marker_live(file,getuid(),123,1000));
 write("pid=123 expires=1100\nTRAILING");assert(!isp_log_marker_live(file,getuid(),123,1000));
 write("pid=123 expires=1100\n");char link[128];snprintf(link,sizeof(link),"%s.link",file);assert(symlink(file,link)==0);
 assert(!isp_log_marker_live(link,getuid(),123,1000));unlink(link);unlink(file);
 char value[92]="unchanged";
 for(unsigned i=0;i<10000;++i){
  assert(!isp_log_override("other.property",value,nullptr));assert(!strcmp(value,"unchanged"));
 }
 assert(!isp_log_marker_live(file,getuid(),123,1000));
}
