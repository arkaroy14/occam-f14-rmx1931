#include "f14_submit_freshness.h"
#include <assert.h>
#include <stdio.h>
#include <thread>
using namespace f14submit;
static thread_local Witness thread_witness;
static void note(Witness& w, uint64_t frame=17, uint64_t epoch=1,
                 uintptr_t handle=0x1000, uint32_t result=0, bool qualified=true) {
    w.begin_copy(); w.end_copy(epoch,frame,handle,0x2000,result,qualified);
}
int main() {
    Witness w;
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    note(w); auto first=w.submit(1,17,0x1000,true);
    assert(first.state==State::linked && first.copy.sequence==1 && first.copy.source==0x2000);
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    note(w); assert(w.submit(1,18,0x1000,true).state==State::frame_changed);
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    note(w); assert(w.submit(2,17,0x1000,true).state==State::epoch_changed);
    note(w); assert(w.submit(1,17,0x3000,true).state==State::handle_changed);
    note(w); assert(w.submit(1,17,0x1000,false).state==State::invalid_record);
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    note(w); note(w,17,1,0x1000,4);
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    note(w); note(w,17,1,0x1000,0,false);
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    note(w,0); assert(w.submit(1,0,0x1000,true).state==State::linked);
    note(w,0x100000011ull);
    assert(w.submit(1,17,0x1000,true).state==State::frame_changed);
    note(w,0x100000011ull);
    assert(w.submit(1,0x100000011ull,0x1000,true).state==State::linked);
    note(w); w.begin_copy(); // Nested submit during an in-progress original copy
    assert(w.submit(1,17,0x1000,true).state==State::no_copy);
    w.end_copy(1,17,0x1000,0x2000,0,true);
    assert(w.submit(1,17,0x1000,true).state==State::linked);
    note(thread_witness);
    std::thread other([] {
        assert(thread_witness.submit(1,17,0x1000,true).state==State::no_copy);
        note(thread_witness,18);
        assert(thread_witness.submit(1,18,0x1000,true).state==State::linked);
    }); other.join();
    assert(thread_witness.submit(1,17,0x1000,true).state==State::linked);
    // This class cannot detect abandoned loops that reuse frame AND handle;
    // the adapter must retain that qualification/lifetime limit explicitly.
    puts("PASS consume_once frame64 epoch handle unreadable failed_unqualified_copy reentry_clear thread_local_isolation; adapter_and_live_freshness_unqualified");
}
