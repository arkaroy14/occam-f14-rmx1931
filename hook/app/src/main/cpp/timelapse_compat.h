#pragma once
#include <pthread.h>
#include <time.h>

// ROM helper SHA73b7d87c..., build ID53b24abc8356d621c67bc5580a6919ec.
// Only the verified default fastVideo profile. Original F14 APK is untouched.
using TimeLapseConfig = int32_t(*)(void*,double,double);
static TimeLapseConfig timelapse_original;
static std::atomic<bool> timelapse_enabled{false};
static bool timelapse_retained;
static std::atomic<unsigned> timelapse_install_state{0},timelapse_calls{0};
static std::atomic<unsigned> timelapse_seen{0};
static std::atomic<bool> timelapse_expired_reported{false};
static time_t timelapse_deadline;
static bool timelapse_take_budget(std::atomic<unsigned>&counter,unsigned limit){
    unsigned value=counter.load();
    while(value<limit){if(counter.compare_exchange_weak(value,value+1))return true;}
    return false;
}
// Only a linker-owned module's readable LOAD ranges, while dl_iterate_phdr
// holds its enumeration/lifetime guard. No new application syscall is needed.
static bool timelapse_module_read(const dl_phdr_info*info,uintptr_t rva,void*out,size_t bytes){
    if(!info||!out||bytes>128||rva>UINTPTR_MAX-bytes||info->dlpi_addr>UINTPTR_MAX-rva-bytes)return false;
    for(unsigned i=0;i<info->dlpi_phnum;++i){const auto&p=info->dlpi_phdr[i];
        if(p.p_type==PT_LOAD&&(p.p_flags&PF_R)&&rva>=p.p_vaddr
                &&rva-p.p_vaddr<=p.p_memsz&&bytes<=p.p_memsz-(rva-p.p_vaddr)){
            memcpy(out,reinterpret_cast<const void*>(info->dlpi_addr+rva),bytes);return true;
        }
    }return false;
}
static bool timelapse_live(){
    if(!timelapse_enabled.load())return false;
    if(timelapse_retained)return true;
    timespec now{};return clock_gettime(CLOCK_MONOTONIC,&now)==0&&now.tv_sec<=timelapse_deadline;
}
static int32_t timelapse_config(void*object,double fps,double capture){
    const int before=errno;
    // The original call owns validation and performs its own lock/unlock.
    errno=before;int32_t result=timelapse_original(object,fps,capture);const int after=errno;
    bool live=timelapse_live();
    if(timelapse_enabled.load()&&timelapse_take_budget(timelapse_seen,4))
        LOG("TIMELAPSE_CONFIG_SEEN pid=%d fps=%.3f capture=%.3f original_result=%d live=%d qualified_profile=%d",
            getpid(),fps,capture,result,live,fps==30.0&&capture==3.0);
    if(timelapse_enabled.load()&&!live&&!timelapse_expired_reported.exchange(true))
        LOG("TIMELAPSE_EXPIRED pid=%d original_behavior_retained=1",getpid());
    if(result==0&&object&&fps==30.0&&capture==3.0&&live){
        bool report=timelapse_take_budget(timelapse_calls,8);
        if(!timelapse_retained&&!report){errno=after;return result;}
        auto*bytes=static_cast<uint8_t*>(object);
        auto*mutex=reinterpret_cast<pthread_mutex_t*>(bytes+0x18);
        int locked=EBUSY;unsigned attempts=0;
        for(;attempts<3;++attempts){
            locked=pthread_mutex_trylock(mutex);
            if(locked!=EBUSY)break;
            if(attempts<2){timespec pause{0,1000000};nanosleep(&pause,nullptr);}
        }
        unsigned executing=255,previous=255,applied=0;double actual_fps=0,actual_capture=0;
        if(locked==0){
            executing=bytes[0xd8]&1;previous=bytes[0x180];
            memcpy(&actual_fps,bytes+0x170,8);memcpy(&actual_capture,bytes+0x178,8);
            // Same condition F14 setTimeLapseConfig uses, while original mutex
            // is held and recording has not started. No timestamp edits.
            if(!executing&&actual_fps==fps&&actual_capture==capture&&previous<=1){
                bytes[0x180]=(capture+capture<fps)?1:0;applied=1;
            }
            pthread_mutex_unlock(mutex);
        }
        if(report)LOG("TIMELAPSE_STOCK_RULE pid=%d fps=%.3f capture=%.3f original_result=%d lock=%d attempts=%u executing=%u snap_before=%u applied=%u actual_fps=%.3f actual_capture=%.3f original_framework_dropper=1",
            getpid(),fps,capture,result,locked,attempts<3?attempts+1:3,executing,previous,applied,actual_fps,actual_capture);
    }
    errno=after;return result;
}
static void timelapse_configure(const char*data,bool original_f14,bool retained=false){
    if(!original_f14||!data)return;
    char marker[PATH_MAX];snprintf(marker,sizeof(marker),"%s/files/ocfresh_timelapse_stock_snap",data);
    timespec now{};
    timelapse_retained=retained;
    bool enabled=retained||(access(marker,R_OK)==0&&clock_gettime(CLOCK_MONOTONIC,&now)==0);
    if(enabled&&!retained)timelapse_deadline=now.tv_sec+600;
    timelapse_enabled.store(enabled);
    LOG("TIMELAPSE_SETUP enabled=%d retained=%d max_configs=%u ttl_seconds=%u log_budget=8 default_profile_only=1",
        enabled,retained,retained?0:8,retained?0:600);
}
static bool timelapse_build_id(const dl_phdr_info*info){
    static constexpr uint8_t expected[]={0x53,0xb2,0x4a,0xbc,0x83,0x56,0xd6,0x21,0xc6,0x7b,0xc5,0x58,0x0a,0x69,0x19,0xec};
    for(unsigned i=0;i<info->dlpi_phnum;++i){
        const auto&p=info->dlpi_phdr[i];if(p.p_type!=PT_NOTE||p.p_memsz>4096)continue;
        uintptr_t base=p.p_vaddr;size_t off=0;
        for(unsigned n=0;n<16&&off+12<=p.p_memsz;++n){
            if(base>UINTPTR_MAX-off)return false;
            Elf64_Nhdr h{};if(!timelapse_module_read(info,base+off,&h,sizeof(h)))return false;
            uint64_t name=(uint64_t(h.n_namesz)+3)&~uint64_t(3),desc=(uint64_t(h.n_descsz)+3)&~uint64_t(3);
            if(name>128||desc>128||off+12+name+desc>p.p_memsz)return false;
            if(h.n_type==NT_GNU_BUILD_ID&&h.n_namesz==4&&h.n_descsz==16){
                char owner[4]{};uint8_t id[16]{};
                if(base>UINTPTR_MAX-off-12-name)return false;
                return timelapse_module_read(info,base+off+12,owner,4)&&memcmp(owner,"GNU",4)==0
                    &&timelapse_module_read(info,base+off+12+name,id,16)&&memcmp(id,expected,16)==0;
            }
            off+=12+name+desc;
        }
    }return false;
}
static int timelapse_find(dl_phdr_info*info,size_t,void*){
    static constexpr const char*name="/libstagefright_bufferqueue_helper.so";
    if(!info->dlpi_name)return 0;
    size_t length=strlen(info->dlpi_name),suffix=strlen(name);
    if(length<suffix||strcmp(info->dlpi_name+length-suffix,name))return 0;
    unsigned state=0;if(!timelapse_install_state.compare_exchange_strong(state,1))return 1;
    if(!timelapse_build_id(info)){
        timelapse_install_state.store(4);LOG("TIMELAPSE_REFUSED unknown_helper_build path=%s terminal=1",info->dlpi_name);return 1;
    }
    bool executable=false;
    for(unsigned i=0;i<info->dlpi_phnum;++i){const auto&p=info->dlpi_phdr[i];
        if(p.p_type==PT_LOAD&&(p.p_flags&PF_X)&&0x13f10>=p.p_vaddr&&0x13f30<=p.p_vaddr+p.p_memsz)executable=true;
    }
    static constexpr uint32_t expected[]={0xd10183ff,0x6d0223e9,0xa9037bfd,0xf90023f5,0xa9054ff4,0x9100c3fd,0x1e604028,0x1e604009};
    uintptr_t address=info->dlpi_addr+0x13f10;uint32_t code[8]{};
    if(!executable||!timelapse_module_read(info,0x13f10,code,sizeof(code))||memcmp(code,expected,sizeof(code))){
        timelapse_install_state.store(4);LOG("TIMELAPSE_REFUSED helper_entry_mismatch path=%s terminal=1",info->dlpi_name);return 1;
    }
    int result=hook_function(reinterpret_cast<void*>(address),reinterpret_cast<void*>(timelapse_config),reinterpret_cast<void**>(&timelapse_original));
    timelapse_install_state.store(result==0?2:3);
    LOG("TIMELAPSE_HOOK pid=%d result=%d path=%s rva=13f10 build_id=53b24abc8356d621c67bc5580a6919ec",getpid(),result,info->dlpi_name);
    return 1;
}
static void timelapse_try_install(){
    if(!active.load()||!timelapse_enabled.load()||timelapse_install_state.load())return;
    int saved=errno;dl_iterate_phdr(timelapse_find,nullptr);errno=saved;
}
static void timelapse_on_loaded(const char*,void*){timelapse_try_install();}
