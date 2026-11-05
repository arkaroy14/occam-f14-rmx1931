// Opt-in observation only. The loader's dlerror is deliberately NOT consumed.
#pragma once
static std::atomic<bool> environment_native_enabled{false};
static std::atomic<unsigned> environment_native_events{0};
static void environment_event(const char*kind,const char*name,const void*caller,uintptr_t result,const void*engine=nullptr){
 if(!environment_native_enabled.load())return;
 int saved=errno;unsigned event=environment_native_events.fetch_add(1);
 if(event<2000){
  Dl_info owner{};dladdr(caller,&owner);
  uintptr_t offset=owner.dli_fbase?reinterpret_cast<uintptr_t>(caller)-reinterpret_cast<uintptr_t>(owner.dli_fbase):0;
  __android_log_print(ANDROID_LOG_INFO,"OcEnvNative","EVENT n=%u kind=%s name=%s result=%lx engine=%p caller=%s rva=%lx errno_after=%d",event,kind,name?name:"NULL",static_cast<unsigned long>(result),engine,owner.dli_fname?owner.dli_fname:"unknown",static_cast<unsigned long>(offset),saved);
 }else if(event==2000)__android_log_print(ANDROID_LOG_WARN,"OcEnvNative","COVERAGE_LIMIT limit=2000 additional_native_events_unobserved; use shorter audit");
 errno=saved;
}
static bool environment_camera_caller(const void*caller){
 if(!environment_native_enabled.load())return false;
 int saved=errno;Dl_info source{};
 bool result=dladdr(caller,&source)&&source.dli_fname&&!strncmp(source.dli_fname,application_directory,strlen(application_directory));
 errno=saved;return result;
}
extern "C" JNIEXPORT void JNICALL
Java_com_arkaroy_occamfresh_NativePaths_setEnvironmentAudit(JNIEnv*,jclass,jboolean enabled){
 environment_native_enabled.store(enabled==JNI_TRUE);
 __android_log_print(ANDROID_LOG_INFO,"OcEnvNative","STATE enabled=%d events=%u limit=2000 loader_dlerror_untouched; SDKcoverage=Superphoto_only",enabled==JNI_TRUE,environment_native_events.load());
}
