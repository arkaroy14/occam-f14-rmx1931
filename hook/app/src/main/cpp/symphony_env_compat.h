#pragma once
static int (*symphony_original_putenv)(char*);
static std::atomic<unsigned> symphony_env_reports{0};
static char symphony_valid_assignment[]="SYMPHONY_INIT_TEST=SYMPHONY_YES";
static bool symphony_env_match(const char* path,uintptr_t rva,const char* text){
    return path&&text&&rva==0x588c4
        &&(!strcmp(path,"/system/priv-app/OppoCamera/lib/arm64/libsymphony-cpu.so")
        ||!strcmp(path,"/system/priv-app/OppoCamera/fresh_native/darksight_rom/lib64/libsymphony-cpu.so")
        ||!strcmp(path,"/system/priv-app/OppoCamera/fresh_native/anc_snpe/lib64/libsymphony-cpu.so"))
        &&!strcmp(text,"SYMPHONY_INIT_TEST=SYMPHONY_YE");
}
static int symphony_putenv(char* text){
    int saved=errno;Dl_info owner{};
    const void* caller=__builtin_return_address(0);
    bool fix=dladdr(caller,&owner)&&owner.dli_fbase
        &&symphony_env_match(owner.dli_fname,reinterpret_cast<uintptr_t>(caller)
              -reinterpret_cast<uintptr_t>(owner.dli_fbase),text);
    errno=saved;
    // putenv retains its pointer: use writable process-lifetime storage, not
    // a stack temporary. No write beyond the original 31-byte SDK buffer.
    int rc=symphony_original_putenv(fix?symphony_valid_assignment:text);int after=errno;
    if(fix&&symphony_env_reports.fetch_add(1)<16)
        LOG("SYMPHONY_ENV_APPLY caller=588c4 original_truncated=1 stable_storage=1 result=%d",rc);
    errno=after;return rc;
}
