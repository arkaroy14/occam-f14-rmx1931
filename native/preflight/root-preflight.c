#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
static int scope(const char *expected) {
 void *lib=dlopen("/system/lib64/libsqlite.so",RTLD_NOW|RTLD_LOCAL);if(!lib)return 24;
 int (*open_db)(const char*,sqlite3**,int,const char*)=dlsym(lib,"sqlite3_open_v2");
 int (*close_db)(sqlite3*)=dlsym(lib,"sqlite3_close");
 int (*prepare)(sqlite3*,const char*,int,sqlite3_stmt**,const char**)=dlsym(lib,"sqlite3_prepare_v2");
 int (*step)(sqlite3_stmt*)=dlsym(lib,"sqlite3_step");
 int (*column)(sqlite3_stmt*,int)=dlsym(lib,"sqlite3_column_int");
 const unsigned char *(*text)(sqlite3_stmt*,int)=dlsym(lib,"sqlite3_column_text");
 int (*finalize)(sqlite3_stmt*)=dlsym(lib,"sqlite3_finalize");
 int (*busy)(sqlite3*,int)=dlsym(lib,"sqlite3_busy_timeout");
 if(!open_db||!close_db||!prepare||!step||!column||!text||!finalize||!busy)return 24;
 sqlite3 *db=0;sqlite3_stmt *stmt=0;
 if(open_db("/data/adb/lspd/config/modules_config.db",&db,1,0)!=0){if(db)close_db(db);return 24;}
 busy(db,2000);
 const char *sql="SELECT m.enabled,m.apk_path,EXISTS(SELECT 1 FROM scope s WHERE s.mid=m.mid AND s.app_pkg_name='com.oppo.camera' AND s.user_id=0) FROM modules m WHERE m.module_pkg_name='com.arkaroy.occamfresh'";
 int rc=24;
 if(prepare(db,sql,-1,&stmt,0)==0){
  int result=step(stmt);
  if(result==101)rc=21;
  else if(result==100){
   const unsigned char *path=text(stmt,1);
   if(column(stmt,0)!=1)rc=22;
   else if(!path||strcmp((const char*)path,expected))rc=25;
   else rc=column(stmt,2)==1?0:23;
  }
 }
 if(stmt)finalize(stmt);close_db(db);dlclose(lib);return rc;
}
struct permission_query {const char *source,*target,*klass,*permission;};
static const struct permission_query policy_queries[]={
 {"u:r:hal_camera_default:s0","u:object_r:hal_camera_default_exec:s0","file","execute_no_trans"},
 {"u:r:priv_app:s0","u:object_r:vendor_xdsp_device:s0","chr_file","getattr"},
 {"u:r:priv_app:s0","u:object_r:vendor_xdsp_device:s0","chr_file","open"},
 {"u:r:priv_app:s0","u:object_r:vendor_xdsp_device:s0","chr_file","read"},
 {"u:r:priv_app:s0","u:object_r:vendor_xdsp_device:s0","chr_file","ioctl"},
 {"u:r:priv_app:s0","u:object_r:adsprpcd_file:s0","file","getattr"},
 {"u:r:priv_app:s0","u:object_r:adsprpcd_file:s0","file","open"},
 {"u:r:priv_app:s0","u:object_r:adsprpcd_file:s0","file","read"},
 {"u:r:hal_camera_default:s0","u:object_r:vendor_camera_data_file:s0","sock_file","write"}
};
/* Query only: no policy file parsing, loading, setenforce or root-manager IPC.
   OC_SELINUX_LIB overrides the library path for host C fixtures only; unset it
   is untouched production behavior. */
static void *selinux_lib(void) {
#ifdef OCCAM_HOST_FIXTURES
 const char *path=getenv("OC_SELINUX_LIB");
 return dlopen(path&&path[0]?path:"/system/lib64/libselinux.so",RTLD_NOW|RTLD_LOCAL);
#else
 return dlopen("/system/lib64/libselinux.so",RTLD_NOW|RTLD_LOCAL);
#endif
}
/* mask bits follow policy_queries[]. Read-back: 0 allow, 31 a completed
   denial, 32 the query or the enforcing state could not be established. */
static int policy_allowed_mask(unsigned mask) {
 void *lib=selinux_lib();if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 32;}
 int (*check)(const char*,const char*,const char*,const char*,void*)=dlsym(lib,"selinux_check_access");
 int (*enforcing)(void)=dlsym(lib,"security_getenforce");
 if(!check||!enforcing||enforcing()!=1){dlclose(lib);return 32;}
 int rc=0;
 for(size_t i=0;i<sizeof(policy_queries)/sizeof(policy_queries[0]);i++){
  if((mask&(1u<<i))==0)continue;
  const struct permission_query *q=&policy_queries[i];
  errno=0;
  if(check(q->source,q->target,q->klass,q->permission,0)!=0){
   if(errno!=EACCES&&errno!=EPERM){
    fprintf(stderr,"Camera permission query failed: errno=%d\n",errno);rc=32;break;
   }
   fprintf(stderr,"Missing camera permission: %s %s %s %s\n",q->source,q->target,q->klass,q->permission);rc=31;
  }
 }
 if(enforcing()!=1)rc=32;
 dlclose(lib);return rc;
}
static int policy_allowed(int include_dsp) {
 if(include_dsp==2)return policy_allowed_mask(1u<<8);
 if(include_dsp==1)return policy_allowed_mask(0xFFu);
 return policy_allowed_mask(1u);
}
/* One allow-line group. Returns 1 when every query completed (allow or
   EACCES/EPERM denial). An unexpected errno marks the group unverifiable
   and the scan continues so later groups are still classified. */
static int group_answerable(int (*check)(const char*,const char*,const char*,const char*,void*),size_t begin,size_t end) {
 int ok=1;
 for(size_t i=begin;i<end;i++){
  const struct permission_query *q=&policy_queries[i];
  errno=0;
  if(check(q->source,q->target,q->klass,q->permission,0)!=0){
   if(errno==EACCES||errno==EPERM)continue;
   fprintf(stderr,"Unverifiable policy query: %s %s %s %s errno=%d\n",
    q->source,q->target,q->klass,q->permission,errno);
   ok=0;
  }
 }
 return ok;
}
/* Install-time capability probe. Read-only: no policy load or grant.
   Queries 0..7 are the packaged rule (exec, vendor_xdsp, adsprpcd).
   Query 8 (power) is excluded. stdout is three lines, exec=/xdsp=/adsprpcd=,
   each ok or bad. Exit:
     0  probe ran and every required group completed (0 or 31)
     33 probe ran, exec completed, at least one DSP group did not
     32 probe failed (dlopen, missing API, not enforcing) OR exec itself
        is unverifiable. 31 is never an exit code here: a denial is ok. */
static int policy_capability(void) {
 void *lib=selinux_lib();if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 32;}
 int (*check)(const char*,const char*,const char*,const char*,void*)=dlsym(lib,"selinux_check_access");
 int (*enforcing)(void)=dlsym(lib,"security_getenforce");
 if(!check||!enforcing||enforcing()!=1){dlclose(lib);return 32;}
 int exec_ok=group_answerable(check,0,1);
 int xdsp_ok=group_answerable(check,1,5);
 int ads_ok=group_answerable(check,5,8);
 int still=enforcing()==1;
 dlclose(lib);
 if(!still){fprintf(stderr,"Not enforcing at capability check\n");return 32;}
 printf("exec=%s\nxdsp=%s\nadsprpcd=%s\n",exec_ok?"ok":"bad",xdsp_ok?"ok":"bad",ads_ok?"ok":"bad");
 if(!exec_ok)return 32;
 if(xdsp_ok&&ads_ok)return 0;
 return 33;
}
int main(int argc,char **argv){
 if(geteuid()!=0)return 2;
 if(argc==3&&!strcmp(argv[1],"lsposed"))return scope(argv[2]);
 if(argc==3&&!strcmp(argv[1],"policy-check")){
  if(!strcmp(argv[2],"core"))return policy_allowed(0);
  if(!strcmp(argv[2],"core-dsp"))return policy_allowed(1);
  if(!strcmp(argv[2],"core-xdsp"))return policy_allowed_mask(0x1Fu);
  if(!strcmp(argv[2],"core-adsprpcd"))return policy_allowed_mask(0xE1u);
  if(!strcmp(argv[2],"power"))return policy_allowed(2);
  return 2;
 }
 if(argc==2&&!strcmp(argv[1],"policy-capability"))return policy_capability();
 if(argc==2&&!strcmp(argv[1],"exec-allowed"))return policy_allowed(0);
 return 2;
}
