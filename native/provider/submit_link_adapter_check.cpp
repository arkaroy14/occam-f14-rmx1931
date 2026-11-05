// Host-only synthetic callback forwarding / bounded-reader fixture.
#include <android/log.h>
#include <atomic>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <vector>
#include <string>
static std::vector<std::string> logs;
extern "C" int __android_log_print(int,const char*,const char*format,...){
    char line[1800];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);
    logs.emplace_back(line);errno=ERANGE;return 0;
}
using HalSetAndroid=uint32_t(*)(void*,const void*);
static std::atomic<HalSetAndroid> metadata_original_android{nullptr};
static std::atomic<uintptr_t> tuning_chi_base{0x10000000};
static std::atomic<uint64_t> tuning_submit_epoch{1};
static bool live=false,recent=false;
static bool tuning_live(){return live;}
static bool tuning_recent(){return live&&recent;}
struct Region {uintptr_t address;size_t bytes;};
static std::vector<Region> regions;
static unsigned reads=0;
static bool tuning_read(uintptr_t p,void*out,size_t n){
    ++reads;errno=EAGAIN;
    if(n>128||p+n<p)return false;
    for(auto r:regions)if(p>=r.address&&p-r.address<=r.bytes&&n<=r.bytes-(p-r.address)){
        memcpy(out,reinterpret_cast<const void*>(p),n);return true;
    }
    errno=EFAULT;return false;
}
#include "f14_submit_link_adapter.h"
static unsigned copies=0,submits=0;
static uint32_t copy_result=0,submit_result=0;
static void*expected_request;
static uint32_t original_copy(void*h,const void*s){
    assert(h==reinterpret_cast<void*>(0x6000)&&s==reinterpret_cast<void*>(0x7000)&&errno==EDOM);
    ++copies;errno=EIO;return copy_result;
}
static uint32_t original_submit(void*c,const void*r){
    assert(c==reinterpret_cast<void*>(0x8000)&&r==expected_request&&errno==EDOM);
    ++submits;errno=EIO;return submit_result;
}
static void reset(){
    submit_witness.begin_copy();submit_copy_budget.store(0);submit_scan_budget.store(0);submit_log_budget.store(0);
    submit_copy_limit_reported.store(false);submit_scan_limit_reported.store(false);submit_log_limit_reported.store(false);
    tuning_submit_epoch.store(1);copy_result=submit_result=0;logs.clear();reads=0;
}
static void copy(uint64_t frame=17,uintptr_t caller=0x100c9748){
    errno=EDOM;auto rc=f14_copy_frame_dispatch(reinterpret_cast<void*>(0x6000),reinterpret_cast<void*>(0x7000),frame,caller);
    assert(rc==copy_result&&errno==EIO);
}
static void submit(SubmitHeader*request){
    expected_request=request;errno=EDOM;auto rc=submit_observe(reinterpret_cast<void*>(0x8000),request);
    assert(rc==submit_result&&errno==EIO);
}
static bool status(const char*value){return !logs.empty()&&logs.back().find(value)!=std::string::npos;}
int main(){
    metadata_original_android.store(original_copy);submit_original.store(original_submit);
    SubmitRecord record{};record.frame=17;record.input_count=1;record.input=0x4000;record.input_meta=0x6000;
    record.pipeline=0x5000;record.output_meta=0x9000;
    SubmitHeader header{0x3000,1,0,reinterpret_cast<uintptr_t>(&record)};
    regions={{reinterpret_cast<uintptr_t>(&header),sizeof(header)},{reinterpret_cast<uintptr_t>(&record),sizeof(record)}};
    auto saved_header=header;auto saved_record=record;
    reset();copy();submit(&header);assert(reads==0&&logs.empty());
    live=recent=true;reset();copy();submit(&header);assert(status("copy_status=copy_frame_handle_match"));
    submit(&header);assert(status("copy_status=no_copy_witness"));
    assert(!memcmp(&header,&saved_header,sizeof(header))&&!memcmp(&record,&saved_record,sizeof(record)));
    reset();copy(0x100000011ull);submit(&header);assert(status("copy_status=frame_changed"));
    reset();copy();record.input_meta=0x9000;submit(&header);assert(status("copy_status=handle_changed"));record=saved_record;
    reset();copy();tuning_submit_epoch.store(2);submit(&header);assert(status("copy_status=epoch_changed"));
    reset();copy_result=4;copy();submit(&header);assert(status("copy_status=no_copy_witness"));
    reset();copy(17,0x100c974c);submit(&header);assert(status("copy_status=no_copy_witness"));
    reset();copy();header.count=2;submit(&header);header=saved_header;submit(&header);assert(status("copy_status=no_copy_witness"));
    reset();copy();auto saved_regions=regions;regions.clear();submit(&header);regions=saved_regions;submit(&header);assert(status("copy_status=no_copy_witness"));
    reset();copy();submit_result=4;submit(&header);assert(status("result=4"));
    reset();copy();recent=false;submit(&header);assert(status("copy_status=copy_frame_handle_match"));recent=true;
    reset();copy();live=false;submit(&header);live=true;submit(&header);assert(status("copy_status=no_copy_witness"));
    reset();for(unsigned i=0;i<70;++i)submit(&header);
    assert(submit_scan_budget.load()==64&&reads==128&&submit_log_budget.load()==4);
    unsigned cap_logs=0;for(const auto&l:logs)cap_logs+=l.find("SCAN_LIMIT")!=std::string::npos;assert(cap_logs==1);
    reset();for(unsigned i=0;i<6;++i)copy();assert(submit_copy_budget.load()==4);
    submit(&header);assert(status("copy_status=no_copy_witness"));
    cap_logs=0;for(const auto&l:logs)cap_logs+=l.find("COPY_LIMIT")!=std::string::npos;assert(cap_logs==1);
    // Forwarding with diagnostics off, including errors, thousands of calls.
    reset();live=false;unsigned c0=copies,s0=submits;
    for(unsigned i=0;i<10000;++i){copy_result=submit_result=i%2?4:0;copy();submit(&header);}
    assert(copies==c0+10000&&submits==s0+10000&&reads==0&&logs.empty());
    puts("PASS actual_adapter args_result_errno original_once inactive10000_pairs no_request_edits matching_frame64_handle_epoch consume_invalid_unreadable_failed_site bounded128reads finite_logs; ARM64_veneer_execution_unqualified");
}
