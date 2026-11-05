// Host-only original-call/lock/field-gate fixture. Never attaches to Android.
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <elf.h>
#include <link.h>
#include <assert.h>
#include <vector>
using HookFunction=int(*)(void*,void*,void**);
static HookFunction hook_function;
static std::atomic<bool> active{false};
static unsigned config_logs=0,refusal_logs=0,hook_logs=0,rule_logs=0;
static void fixture_log(const char*format,...){
    if(!strncmp(format,"TIMELAPSE_CONFIG_SEEN",21))++config_logs;
    if(!strncmp(format,"TIMELAPSE_REFUSED",16))++refusal_logs;
    if(!strncmp(format,"TIMELAPSE_HOOK",13))++hook_logs;
    if(!strncmp(format,"TIMELAPSE_STOCK_RULE",20))++rule_logs;
}
#define LOG(...) fixture_log(__VA_ARGS__)
#include "timelapse_compat.h"
static unsigned calls=0;
static int32_t expected_result=0;
static int32_t original(void*object,double fps,double capture){
    assert(errno==EDOM);++calls;
    auto*p=static_cast<uint8_t*>(object);
    if(expected_result==0){memcpy(p+0x170,&fps,8);memcpy(p+0x178,&capture,8);p[0x180]=0;}
    errno=EIO;return expected_result;
}
static unsigned installs=0;
static void*expected_target;
static int fixture_install(void*target,void*replacement,void**saved){
    assert(target==expected_target&&replacement==reinterpret_cast<void*>(timelapse_config));
    *saved=reinterpret_cast<void*>(original);++installs;return 0;
}
int main(){
    alignas(16) uint8_t object[0x1c0]{};auto*mutex=reinterpret_cast<pthread_mutex_t*>(object+0x18);
    assert(pthread_mutex_init(mutex,nullptr)==0);timelapse_original=original;
    timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);timelapse_deadline=now.tv_sec+60;
    auto call=[&](double fps,double capture){errno=EDOM;auto r=timelapse_config(object,fps,capture);assert(r==expected_result&&errno==EIO);};
    call(30,3);assert(object[0x180]==0); // marker default off
    timelapse_enabled.store(true);call(30,3);assert(object[0x180]==1);
    call(30,480);assert(object[0x180]==0); // high speed untouched
    call(60,3);assert(object[0x180]==0); // unqualified profile untouched
    call(30,0);assert(object[0x180]==0); // hyper-lapse untouched
    object[0xd8]=1;call(30,3);assert(object[0x180]==0);object[0xd8]=0;
    expected_result=-38;object[0x180]=0;call(30,3);assert(object[0x180]==0);expected_result=0;
    assert(pthread_mutex_lock(mutex)==0);call(30,3);assert(object[0x180]==0);pthread_mutex_unlock(mutex);
    timelapse_deadline=now.tv_sec-1;call(30,3);assert(object[0x180]==0);
    assert(calls==9);
    timelapse_configure("/nonexistent",false,true);
    assert(!timelapse_retained); // exact original gate precedes retained opt-in
    timelapse_configure("/nonexistent",true,true);
    assert(timelapse_retained&&timelapse_live());
    for(unsigned i=0;i<25;++i){call(30,3);assert(object[0x180]==1);}
    assert(timelapse_calls.load()==8&&rule_logs==8); // behavior outlives finite log budget
    call(30,30);assert(object[0x180]==0); // ordinary video remains original
    call(30,480);assert(object[0x180]==0);
    object[0xd8]=1;call(30,3);assert(object[0x180]==0);object[0xd8]=0;
    expected_result=-38;call(30,3);assert(object[0x180]==0);expected_result=0;
    assert(pthread_mutex_lock(mutex)==0);call(30,3);assert(object[0x180]==0);pthread_mutex_unlock(mutex);
    timelapse_configure("/nonexistent",true,false);assert(!timelapse_live());
    assert(pthread_mutex_destroy(mutex)==0&&calls==39);
    assert(config_logs==4); // bounded even for nonqualifying arguments
    std::vector<uint8_t> image(0x14040);
    Elf64_Phdr phdrs[2]{};
    phdrs[0].p_type=PT_LOAD;phdrs[0].p_flags=PF_R|PF_X;phdrs[0].p_memsz=image.size();
    phdrs[1].p_type=PT_NOTE;phdrs[1].p_vaddr=32;phdrs[1].p_memsz=32;
    Elf64_Nhdr note{4,16,NT_GNU_BUILD_ID};memcpy(image.data()+32,&note,12);memcpy(image.data()+44,"GNU",4);
    uint8_t id[]={0x53,0xb2,0x4a,0xbc,0x83,0x56,0xd6,0x21,0xc6,0x7b,0xc5,0x58,0x0a,0x69,0x19,0xec};
    memcpy(image.data()+48,id,16);
    uint32_t code[]={0xd10183ff,0x6d0223e9,0xa9037bfd,0xf90023f5,0xa9054ff4,0x9100c3fd,0x1e604028,0x1e604009};
    memcpy(image.data()+0x13f10,code,32);
    dl_phdr_info info{};info.dlpi_addr=reinterpret_cast<uintptr_t>(image.data());info.dlpi_phdr=phdrs;info.dlpi_phnum=2;
    info.dlpi_name="/system/lib64/libstagefright_bufferqueue_helper.so";
    uint8_t output[128]{};
    assert(timelapse_build_id(&info)&&timelapse_module_read(&info,32,output,32));
    assert(!timelapse_module_read(&info,image.size()-4,output,8));
    assert(!timelapse_module_read(&info,UINTPTR_MAX-1,output,8));
    assert(!timelapse_module_read(&info,32,output,129));
    phdrs[0].p_flags=PF_X;assert(!timelapse_module_read(&info,32,output,16));phdrs[0].p_flags=PF_R|PF_X;
    hook_function=fixture_install;expected_target=image.data()+0x13f10;
    timelapse_install_state.store(0);assert(timelapse_find(&info,0,nullptr)==1&&installs==1&&hook_logs==1);
    assert(timelapse_find(&info,0,nullptr)==1&&installs==1);
    timelapse_install_state.store(0);image[48]^=1;
    for(unsigned i=0;i<20;++i)timelapse_find(&info,0,nullptr);
    assert(refusal_logs==1&&timelapse_install_state.load()==4&&installs==1);
    timelapse_install_state.store(0);image[48]^=1;image[0x13f10]^=1;
    for(unsigned i=0;i<20;++i)timelapse_find(&info,0,nullptr);
    assert(refusal_logs==2&&timelapse_install_state.load()==4&&installs==1);
    puts("PASS original_args_result_errno default_off qualified_rule highspeed_other_profile_hyperlapse_untouched executing_error_bounded_mutex_retry_expired retained_beyond_trial_ttl_and_8configs ordinary_video_retained_untouched 39_original_calls config_logs4 rule_logs8 module_read_bounds_permissions build_id_prefix single_install terminal_refusal_logs2 no_process_vm_readv");
}
