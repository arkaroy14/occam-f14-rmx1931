// Standalone forwarding/publication fixture; never attaches to the provider.
#include <android/log.h>
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
static std::atomic<uintptr_t> tuning_chi_base{0},tuning_request_settings{0};
#ifdef OCCAM_METADATA_SUBMIT_LINK
static std::atomic<uint64_t> tuning_submit_epoch{1};
#endif
static std::atomic<unsigned> tuning_metadata_count{0};
struct TuningCallContext {bool observe;uintptr_t call;};
static thread_local TuningCallContext tuning_call_context{};
static bool tuning_live(){return false;}
static bool tuning_recent(){return false;}
static bool tuning_read(uintptr_t,void*,size_t){return false;}
#include "f14_metadata_ops_audit.h"
static unsigned failures,get_calls,set_calls,android_calls,entry_calls,ops_calls;
static int handle,source,payload;
static uint32_t expected_result;
static uint32_t original_get(void*h,uint32_t tag,void**out){
    ++get_calls;if(h!=&handle||tag!=0x123456||errno!=EDOM)++failures;
    *out=&payload;errno=EIO;return expected_result;
}
static uint32_t original_set(void*h,uint32_t tag,const void*d,uint32_t count){
    ++set_calls;if(h!=&handle||tag!=0x987654||d!=&payload||count!=7||errno!=EDOM)++failures;
    errno=EIO;return expected_result;
}
static uint32_t original_android(void*h,const void*s){
    ++android_calls;if(h!=&handle||s!=&source||errno!=EDOM)++failures;
    errno=EIO;return expected_result;
}
static constexpr uintptr_t fake_base=0x1000000;
static uintptr_t original_ops(void* table){
    ++ops_calls;
    if(table){auto**v=static_cast<void**>(table);for(unsigned i=0;i<37;++i)v[i]=reinterpret_cast<void*>(0x100+i);
        v[4]=reinterpret_cast<void*>(fake_base+0x4b4d20);v[8]=reinterpret_cast<void*>(fake_base+0x4b4e58);
        v[10]=reinterpret_cast<void*>(fake_base+0x4b51e8);v[13]=reinterpret_cast<void*>(fake_base+0x4b5f70);}
    errno=EINTR;return 0xdeadbeef;
}
static uintptr_t original_entry(void* table){
    ++entry_calls;
    if(table){auto**v=static_cast<void**>(table);for(unsigned i=0;i<20;++i)v[i]=reinterpret_cast<void*>(0x200+i);
        v[18]=reinterpret_cast<void*>(fake_base+0x4b2980);
#ifdef OCCAM_METADATA_SUBMIT_LINK
        v[13]=reinterpret_cast<void*>(fake_base+0x4b1cf8);
#endif
    }
    errno=EINTR;return 0xaabbccdd;
}
int main(){
    metadata_original_get.store(original_get);metadata_original_set.store(original_set);metadata_original_android.store(original_android);
    for(unsigned i=0;i<10000;++i){
        expected_result=i%2?4:0;void*out=nullptr;
        errno=EDOM;if(metadata_observe_get(&handle,0x123456,&out)!=expected_result||out!=&payload||errno!=EIO)++failures;
        errno=EDOM;if(metadata_observe_set(&handle,0x987654,&payload,7)!=expected_result||errno!=EIO)++failures;
        errno=EDOM;if(metadata_observe_android(&handle,&source)!=expected_result||errno!=EIO)++failures;
    }
    metadata_hal_base.store(fake_base);metadata_get_ops.store(original_ops);metadata_hal_entry.store(original_entry);
    void*ops[37]{};void*context[20]{};
    if(metadata_observe_ops(ops)!=0xdeadbeef||errno!=EINTR)++failures;
    for(unsigned i=0;i<37;++i){
        void*expected=
#ifdef OCCAM_METADATA_COPY_CONTRACT_ONLY
            i==4?reinterpret_cast<void*>(fake_base+0x4b4d20):i==8?reinterpret_cast<void*>(fake_base+0x4b4e58):
#else
            i==4?reinterpret_cast<void*>(metadata_observe_get):i==8?reinterpret_cast<void*>(metadata_observe_set):
#endif
            i==10?reinterpret_cast<void*>(
#ifdef OCCAM_METADATA_SUBMIT_LINK
                f14_copy_frame_entry
#else
                metadata_observe_android
#endif
            ):i==13?reinterpret_cast<void*>(fake_base+0x4b5f70):reinterpret_cast<void*>(0x100+i);
        if(ops[i]!=expected)++failures;
    }
    if(metadata_observe_entry(context)!=0xaabbccdd||errno!=EINTR)++failures;
    for(unsigned i=0;i<20;++i)if(context[i]!=(i==18?reinterpret_cast<void*>(metadata_observe_ops):
#ifdef OCCAM_METADATA_SUBMIT_LINK
        i==13?reinterpret_cast<void*>(submit_observe):
#endif
        reinterpret_cast<void*>(0x200+i)))++failures;
    // Null ChiEntry still runs the original vendor-tag side effects.
    if(metadata_observe_entry(nullptr)!=0xaabbccdd||entry_calls!=2||errno!=EINTR)++failures;
    if(get_calls!=10000||set_calls!=10000||android_calls!=10000||ops_calls!=1)++failures;
    printf("get=%u set=%u android=%u entry=%u ops=%u failures=%u args_out_return_errno=checked publication_slots=checked null_entry=checked\n",
        get_calls,set_calls,android_calls,entry_calls,ops_calls,failures);
    return failures?1:0;
}
