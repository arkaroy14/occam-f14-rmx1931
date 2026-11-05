#include <dlfcn.h>
#include <errno.h>
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
/* Query only: no policy file parsing, loading, setenforce or root-manager IPC. */
static int policy_allowed(int include_dsp) {
 void *lib=dlopen("/system/lib64/libselinux.so",RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 32;}
 int (*check)(const char*,const char*,const char*,const char*,void*)=dlsym(lib,"selinux_check_access");

 int (*enforcing)(void)=dlsym(lib,"security_getenforce");
 if(!check||!enforcing||enforcing()!=1){dlclose(lib);return 32;}
 size_t start=include_dsp==2?8:0;
 size_t count=include_dsp==1?8:1;
 int rc=0;
 for(size_t i=start;i<start+count;i++){
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
int main(int argc,char **argv){
 if(geteuid()!=0)return 2;
 if(argc==3&&!strcmp(argv[1],"lsposed"))return scope(argv[2]);
 if(argc==3&&!strcmp(argv[1],"policy-check")){
  if(!strcmp(argv[2],"core"))return policy_allowed(0);
  if(!strcmp(argv[2],"core-dsp"))return policy_allowed(1);
  if(!strcmp(argv[2],"power"))return policy_allowed(2);
  return 2;
 }
 if(argc==2&&!strcmp(argv[1],"exec-allowed"))return policy_allowed(0);
 return 2;
}
