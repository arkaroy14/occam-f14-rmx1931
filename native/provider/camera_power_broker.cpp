// Separate platform-Binder client. Only one pinned provider PID/UID and one
// reviewed profile may request a finite2000ms lease. No arbitrary node/API input.
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "f14_power_protocol.h"
#ifdef OCCAM_F14_MIGRATION_BROKER
#include <fcntl.h>
#include <sys/system_properties.h>
#include "f14_migration_lease.h"
struct MigrationIO {
 const char*path(int n){return n?"/proc/sys/kernel/sched_downmigrate":"/proc/sys/kernel/sched_upmigrate";}
 bool game_off(){char v[PROP_VALUE_MAX]{};return __system_property_get("perf_profile",v)>0&&!strcmp(v,"0");}
 bool read(int n,int*v){char b[80]{};int f=open(path(n),O_RDONLY|O_CLOEXEC);if(f<0)return false;
  int size=::read(f,b,sizeof(b)-1);close(f);char extra=0;
  return size>0&&sscanf(b,"%d %d %c",v,v+1,&extra)==2;}
 bool write(int n,const int*v){char b[40];int size=snprintf(b,sizeof(b),"%d %d\n",v[0],v[1]);
  int f=open(path(n),O_WRONLY|O_CLOEXEC);if(f<0)return false;
  int written=::write(f,b,size);close(f);int actual[2];
  bool ok=written==size&&read(n,actual)&&actual[0]==v[0]&&actual[1]==v[1];
  printf("MIGRATION_WRITE node=%d value=%d,%d verified=%d\n",n,v[0],v[1],ok);return ok;}
};
#endif
static constexpr const char*hint="OCCAM_F14_VIDEO_PREVIEW_60FPS";
#ifdef OCCAM_F14_RETAINED_POWER
static constexpr const char*marker="/data/vendor/camera/ocfresh_camera_power";
#else
static constexpr const char*marker="/data/vendor/camera/ocfresh_power_lease_trial";
#endif
static void*create(void*p){return p;}static void destroy(void*){}
static binder_status_t unused(AIBinder*,transaction_code_t,const AParcel*,AParcel*){return STATUS_UNKNOWN_TRANSACTION;}
static int64_t now_ms(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}
static bool supported(AIBinder*ext){
 AParcel*in=nullptr,*out=nullptr;AStatus*status=nullptr;bool value=false;
 auto rc=AIBinder_prepareTransaction(ext,&in);
 if(rc==STATUS_OK)rc=AParcel_writeString(in,hint,strlen(hint));
 if(rc==STATUS_OK)rc=AIBinder_transact(ext,4,&in,&out,0);
 if(rc==STATUS_OK)rc=AParcel_readStatusHeader(out,&status);
 bool ok=rc==STATUS_OK&&status&&AStatus_isOk(status);
 if(ok)rc=AParcel_readBool(out,&value);
 if(status)AStatus_delete(status);if(in)AParcel_delete(in);if(out)AParcel_delete(out);
 return ok&&rc==STATUS_OK&&value;
}
static int vote(AIBinder*ext,int ms){
 AParcel*in=nullptr,*out=nullptr;auto rc=AIBinder_prepareTransaction(ext,&in);
 if(rc==STATUS_OK)rc=AParcel_writeString(in,hint,strlen(hint));
 if(rc==STATUS_OK)rc=AParcel_writeInt32(in,ms);
 if(rc==STATUS_OK)rc=AIBinder_transact(ext,3,&in,&out,FLAG_ONEWAY);
 if(in)AParcel_delete(in);if(out)AParcel_delete(out);return rc;
}
int main(int argc,char**argv){
#ifdef OCCAM_F14_RETAINED_POWER
 if(argc==2&&!strcmp(argv[1],"--policy-status")){
  void*library=dlopen("libselinux.so",RTLD_NOW|RTLD_LOCAL);
  using CheckAccess=int(*)(const char*,const char*,const char*,const char*,void*);
  auto check=library?reinterpret_cast<CheckAccess>(dlsym(library,"selinux_check_access")):nullptr;
  if(!check){puts("POLICY_UNKNOWN");return 11;}
  errno=0;int result=check("u:r:hal_camera_default:s0","u:object_r:vendor_camera_data_file:s0","sock_file","write",nullptr);
  int error=errno;dlclose(library);
  if(result==0){puts("POLICY_ALLOWED");return 0;}
  if(error==EACCES){puts("POLICY_DENIED");return 10;}
  printf("POLICY_UNKNOWN errno=%d\n",error);return 11;
 }
#endif
#ifdef OCCAM_F14_MIGRATION_BROKER
 bool qualify=argc==2&&!strcmp(argv[1],"--qualify");
 if(!qualify&&argc!=3)return 2;
 pid_t expected_pid=qualify?2:atoi(argv[1]);uid_t expected_uid=qualify?1047:atoi(argv[2]);
#else
 if(argc!=3)return 2;pid_t expected_pid=atoi(argv[1]);uid_t expected_uid=atoi(argv[2]);
#endif
 if(expected_pid<2||expected_uid==0||access(marker,R_OK))return 3;
 setvbuf(stdout,nullptr,_IOLBF,0);
 using Check=AIBinder*(*)(const char*);auto check=reinterpret_cast<Check>(dlsym(RTLD_DEFAULT,"AServiceManager_checkService"));
 AIBinder*power=check?check("android.hardware.power.IPower/default"):nullptr,*ext=nullptr;
 auto rc=power?AIBinder_getExtension(power,&ext):STATUS_NAME_NOT_FOUND;
 if(power)AIBinder_decStrong(power);
 auto*cls=AIBinder_Class_define("google.hardware.power.extension.pixel.IPowerExt",create,destroy,unused);
 if(rc||!ext||!cls||!AIBinder_associateClass(ext,cls)||!supported(ext)){
  printf("BACKEND_UNAVAILABLE status=%d\n",rc);return 4;
 }
#ifdef OCCAM_F14_MIGRATION_BROKER
 MigrationIO migration_io;F14MigrationLease<MigrationIO> migration(migration_io);
 if(!migration.available()){puts("MIGRATION_BASELINE_REFUSED");AIBinder_decStrong(ext);return 9;}
 if(qualify){
  bool ok=migration.renew(now_ms());int result=ok?vote(ext,2000):-1;
  printf("QUALIFY migration=%d binder_status=%d finite_ms=2000\n",ok,result);
  int64_t deadline=now_ms()+2300;
  while(ok&&now_ms()<deadline){usleep(100000);ok=migration.tick(now_ms());}
  bool released=migration.release();vote(ext,-1);AIBinder_decStrong(ext);
  printf("QUALIFY_END restored=%d available=%d\n",released,migration.available());
  return ok&&result==0&&released&&migration.available()?0:10;
 }
#endif
 int server=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(server<0)return 5;
 sockaddr_un address{};address.sun_family=AF_UNIX;strcpy(address.sun_path,f14_power_socket);
 // Controller must ensure no socket from another owner exists. Never unlink one here.
 if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))||listen(server,4))return 6;
 if(chown(f14_power_socket,expected_uid,1006)||chmod(f14_power_socket,0660))return 7;
#ifdef OCCAM_F14_RETAINED_POWER
 printf("READY provider_pid=%d uid=%u profile=42 hint=%s max_ms=2000 manual_session=1\n",expected_pid,expected_uid,hint);
 int64_t deadline=INT64_MAX,last_vote=0;
#else
 printf("READY provider_pid=%d uid=%u profile=42 hint=%s max_ms=2000 lifetime_ms=180000\n",expected_pid,expected_uid,hint);
 int64_t deadline=now_ms()+180000,last_vote=0;
#endif
 unsigned count=0;bool held=false;
#ifdef OCCAM_F14_MIGRATION_BROKER
 bool fatal_migration=false;
#endif
 while(now_ms()<deadline&&access(marker,R_OK)==0){
#ifdef OCCAM_F14_MIGRATION_BROKER
  if(!migration.tick(now_ms())){puts("MIGRATION_EXPIRY_FAILED");break;}
#endif
  pollfd ready{server,POLLIN,0};if(poll(&ready,1,200)<=0)continue;
  int client=accept4(server,nullptr,nullptr,SOCK_CLOEXEC);if(client<0)continue;
  timeval limit{0,100000};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&limit,sizeof(limit));
  setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&limit,sizeof(limit));
  ucred peer{};socklen_t length=sizeof(peer);F14PowerRequest request{};F14PowerReply reply{f14_power_magic,-13};
  bool authorized=getsockopt(client,SOL_SOCKET,SO_PEERCRED,&peer,&length)==0&&peer.pid==expected_pid&&peer.uid==expected_uid;
  if(authorized&&recv(client,&request,sizeof(request),MSG_WAITALL)==sizeof(request)&&f14_power_valid(request)){
   int64_t t=now_ms();
   if(request.duration_ms==0)reply.status=0;
   else if(request.duration_ms==-1){reply.status=vote(ext,-1);held=false;
#ifdef OCCAM_F14_MIGRATION_BROKER
    if(!migration.release()){reply.status=-5;fatal_migration=true;}
#endif
   }
   else if(last_vote&&t-last_vote<800)reply.status=-11;
   else {
#ifdef OCCAM_F14_MIGRATION_BROKER
    if(!migration.renew(t)){vote(ext,-1);held=false;reply.status=-5;fatal_migration=true;}
    else
#endif
    {reply.status=vote(ext,2000);last_vote=t;held=reply.status==0;
#ifdef OCCAM_F14_MIGRATION_BROKER
     if(!held){migration.release();fatal_migration=true;}
#endif
    }
   }
   if(count++<150)printf("REQUEST pid=%d profile=42 duration=%d status=%d\n",peer.pid,request.duration_ms,reply.status);
  }else printf("REFUSED peer_pid=%d peer_uid=%u authorized=%d\n",peer.pid,peer.uid,authorized);
  send(client,&reply,sizeof(reply),MSG_NOSIGNAL);close(client);
#ifdef OCCAM_F14_MIGRATION_BROKER
  if(fatal_migration){puts("MIGRATION_FATAL_STOP independent_guard_required=1");break;}
#endif
 }
#ifdef OCCAM_F14_MIGRATION_BROKER
 bool restored=migration.release()&&migration.at_baseline();printf("MIGRATION_EXIT restored=%d\n",restored);
#endif
 if(held)vote(ext,-1);close(server);unlink(f14_power_socket);AIBinder_decStrong(ext);
 puts("EXIT outstanding_hint_ended=1");return 0;
}
