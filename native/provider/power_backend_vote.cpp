// One finite backend qualification vote; never called automatically at boot.
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
static void*create(void*p){return p;}
static void destroy(void*){}
static binder_status_t unexpected(AIBinder*,transaction_code_t,const AParcel*,AParcel*){return STATUS_UNKNOWN_TRANSACTION;}
int main(int argc,char**argv){
    bool restore=argc==3&&!strcmp(argv[1],"--restore")&&
        (!strcmp(argv[2],"INTERACTIVE")||!strcmp(argv[2],"DOUBLE_TAP_TO_WAKE"));
    if(!restore&&(argc!=2||strcmp(argv[1],"OCCAM_F14_VIDEO_UHD60"))){
        puts("Only the reviewed finite UHD60 trial or replay of observed system hints is admitted");return 2;
    }
    const char*name=restore?argv[2]:argv[1];
    using Check=AIBinder*(*)(const char*);
    auto check=reinterpret_cast<Check>(dlsym(RTLD_DEFAULT,"AServiceManager_checkService"));
    if(!check)return 3;
    auto*power=check("android.hardware.power.IPower/default");if(!power)return 4;
    AIBinder*ext=nullptr;auto rc=AIBinder_getExtension(power,&ext);AIBinder_decStrong(power);
    if(rc!=STATUS_OK||!ext)return 5;
    auto*cls=AIBinder_Class_define("google.hardware.power.extension.pixel.IPowerExt",create,destroy,unexpected);
    if(!AIBinder_associateClass(ext,cls))return 6;
    AParcel*in=nullptr,*out=nullptr;AStatus*status=nullptr;bool supported=false;
    rc=AIBinder_prepareTransaction(ext,&in);
    if(rc==STATUS_OK)rc=AParcel_writeString(in,name,strlen(name));
    if(rc==STATUS_OK)rc=AIBinder_transact(ext,restore?2:4,&in,&out,0); // support query, no vote yet
    if(rc==STATUS_OK)rc=AParcel_readStatusHeader(out,&status);
    if(rc==STATUS_OK&&status&&AStatus_isOk(status))rc=AParcel_readBool(out,&supported);
    if(status)AStatus_delete(status);if(in)AParcel_delete(in);if(out)AParcel_delete(out);
    in=out=nullptr;
    if(rc!=STATUS_OK||!supported){puts("REFUSED unsupported camera hint");return 7;}
    rc=AIBinder_prepareTransaction(ext,&in);
    if(rc==STATUS_OK)rc=AParcel_writeString(in,name,strlen(name));
    if(rc==STATUS_OK)rc=restore?AParcel_writeBool(in,true):AParcel_writeInt32(in,2000);
    if(rc==STATUS_OK)rc=AIBinder_transact(ext,restore?1:3,&in,&out,FLAG_ONEWAY);
    printf("%s hint=%s duration_ms=%d binder_status=%d actual_nodes_require_readback=1\n",
        restore?"REPLAY_OBSERVED_SYSTEM_MODE":"FINITE_VOTE",name,restore?0:2000,rc);
    if(in)AParcel_delete(in);if(out)AParcel_delete(out);AIBinder_decStrong(ext);
    return rc==STATUS_OK?0:8;
}
