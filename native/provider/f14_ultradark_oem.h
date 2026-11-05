#pragma once
// Exact original F14 stack contracts; one property response per qualified Fill.
static thread_local uintptr_t ud_oem_fill_fp=0;
static std::atomic<bool> ud_oem_session{false};
static bool ud_oem_tuple(uintptr_t caller,uint32_t usecase,uint32_t sensor,uint32_t feature){
    return caller==0xc9fa8&&usecase==13&&sensor==2&&feature==20;
}
static void ud_oem_prepare(const F14PropertyEntry* e){
    uintptr_t base=tuning_chi_base.load();
    if(!base||e->x19_x30[11]!=base+0xea928)return;
    ud_oem_fill_fp=0;
    if(!ud_oem_session.load())return;
    uintptr_t fp=e->x19_x30[10],pair[2]{};uint32_t feature=0;
    if(fp!=e->entry_sp+0xf0||!tuning_read(fp,pair,sizeof(pair))
        ||!tuning_read(e->x19_x30[4],&feature,4))return;
    if(ud_oem_tuple(pair[1]-base,static_cast<uint32_t>(e->x19_x30[5]),
        static_cast<uint32_t>(e->x19_x30[0]),feature))ud_oem_fill_fp=fp;
}
static bool ud_oem_query(const F14PropertyEntry* e,uintptr_t offset){
    if(offset!=0xe9984)return false;
    uintptr_t expected=ud_oem_fill_fp;ud_oem_fill_fp=0;
    if(!expected||!ud_oem_session.load())return false;
    uintptr_t pair[2]{};
    return e->x19_x30[10]==e->entry_sp+0x70
        &&tuning_read(e->x19_x30[10],pair,sizeof(pair))
        &&pair[0]==expected&&pair[1]==tuning_chi_base.load()+0xea970;
}
