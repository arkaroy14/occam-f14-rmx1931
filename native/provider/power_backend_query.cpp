// Read-only Binder support queries. No setMode/setBoost transactions exist here.
#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
static void*create(void*p){return p;}
static void destroy(void*){}
static binder_status_t unexpected(AIBinder*,transaction_code_t,const AParcel*,AParcel*){return STATUS_UNKNOWN_TRANSACTION;}
static bool attach(AIBinder*b,const char*descriptor){
    auto*c=AIBinder_Class_define(descriptor,create,destroy,unexpected);
    return c&&AIBinder_associateClass(b,c);
}
static void query(AIBinder*b,const char*kind,const char*name,int value,bool text,bool boost){
    AParcel*in=nullptr,*out=nullptr;AStatus*status=nullptr;bool supported=false;
    binder_status_t rc=AIBinder_prepareTransaction(b,&in);
    if(rc==STATUS_OK)rc=text?AParcel_writeString(in,name,strlen(name)):AParcel_writeInt32(in,value);
    if(rc==STATUS_OK)rc=AIBinder_transact(b,boost?4:2,&in,&out,0);
    if(rc==STATUS_OK)rc=AParcel_readStatusHeader(out,&status);
    bool ok=rc==STATUS_OK&&status&&AStatus_isOk(status);
    if(ok)rc=AParcel_readBool(out,&supported);
    printf("QUERY interface=%s kind=%s name=%s transaction=%d exception=%d supported=%d read_only=1\n",
        kind,boost?"boost":"mode",name,rc,status?AStatus_getExceptionCode(status):-999,ok&&rc==STATUS_OK?supported:-1);
    if(status)AStatus_delete(status);if(in)AParcel_delete(in);if(out)AParcel_delete(out);
}
int main(){
    using Check=AIBinder*(*)(const char*);
    auto check=reinterpret_cast<Check>(dlsym(RTLD_DEFAULT,"AServiceManager_checkService"));
    if(!check){puts("NO_SERVICE_MANAGER_API");return 2;}
    AIBinder*power=check("android.hardware.power.IPower/default");
    if(!power){puts("NO_POWER_SERVICE");return 3;}
    if(!attach(power,"android.hardware.power.IPower")){puts("POWER_DESCRIPTOR_MISMATCH");return 4;}
    for(int i=11;i<=14;++i)query(power,"IPower","CAMERA_STREAMING",i,false,false);
    query(power,"IPower","CAMERA_LAUNCH",4,false,true);
    query(power,"IPower","CAMERA_SHOT",5,false,true);
    AIBinder*ext=nullptr;auto rc=AIBinder_getExtension(power,&ext);
    printf("EXTENSION transaction=%d present=%d\n",rc,ext!=nullptr);
    if(rc==STATUS_OK&&ext){
        if(attach(ext,"google.hardware.power.extension.pixel.IPowerExt")){
            for(const char*name:{"LAUNCH","CAMERA_STREAMING_HIGH","ORMS_ACTION_CAMERA_VIDEO_UHD60","OCCAM_F14_VIDEO60"}){
                query(ext,"IPowerExt",name,0,true,false);query(ext,"IPowerExt",name,0,true,true);
            }
        }else puts("EXTENSION_DESCRIPTOR_MISMATCH");
        AIBinder_decStrong(ext);
    }
    AIBinder_decStrong(power);return 0;
}
