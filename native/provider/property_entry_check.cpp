// Standalone ABI check for OUR veneer. Does not attach to the camera provider.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "property_entry_veneer.h"
static unsigned calls, failures;
static const char key[]="veneer-test",fallback[]="default";
static char value[64];
extern "C" char veneer_fixture_return;
extern "C" int oc_f14_property_get(const char*k,char*v,const char*f,const F14PropertyEntry*s){
    ++calls;
    if(k!=key||v!=value||f!=fallback||errno!=EDOM||s->entry_sp%16
        ||s->x19_x30[10]!=s->entry_sp||s->x19_x30[11]!=reinterpret_cast<uintptr_t>(&veneer_fixture_return))++failures;
    for(unsigned i=0;i<10;++i)if(s->x19_x30[i]!=0x119+i)++failures;
    strcpy(v,"unchanged-args");errno=EAGAIN;return -73;
}
extern "C" __attribute__((naked)) int property_get(const char*,char*,const char*){
    __asm__ volatile(OCFRESH_PROPERTY_VENEER);
}
extern "C" int veneer_fixture(const char*,char*,const char*);
__asm__(
".text\n.global veneer_fixture\n.type veneer_fixture,%function\nveneer_fixture:\n"
"sub sp,sp,#96\n"
"stp x19,x20,[sp,#0]\nstp x21,x22,[sp,#16]\n"
"stp x23,x24,[sp,#32]\nstp x25,x26,[sp,#48]\n"
"stp x27,x28,[sp,#64]\nstp x29,x30,[sp,#80]\nmov x29,sp\n"
"mov x19,#0x119\nmov x20,#0x11a\nmov x21,#0x11b\nmov x22,#0x11c\n"
"mov x23,#0x11d\nmov x24,#0x11e\nmov x25,#0x11f\nmov x26,#0x120\n"
"mov x27,#0x121\nmov x28,#0x122\nbl property_get\n"
".global veneer_fixture_return\nveneer_fixture_return:\n"
"mov x9,sp\ncmp x29,x9\nb.ne 1f\n"
"cmp x19,#0x119\nb.ne 1f\ncmp x20,#0x11a\nb.ne 1f\n"
"cmp x21,#0x11b\nb.ne 1f\ncmp x22,#0x11c\nb.ne 1f\n"
"cmp x23,#0x11d\nb.ne 1f\ncmp x24,#0x11e\nb.ne 1f\n"
"cmp x25,#0x11f\nb.ne 1f\ncmp x26,#0x120\nb.ne 1f\n"
"cmp x27,#0x121\nb.ne 1f\ncmp x28,#0x122\nb.ne 1f\nb 2f\n"
"1: mov w0,#99\n2: ldp x19,x20,[sp,#0]\nldp x21,x22,[sp,#16]\n"
"ldp x23,x24,[sp,#32]\nldp x25,x26,[sp,#48]\n"
"ldp x27,x28,[sp,#64]\nldp x29,x30,[sp,#80]\nadd sp,sp,#96\nret\n"
".size veneer_fixture,.-veneer_fixture\n");
int main(){
    for(unsigned i=0;i<100000;++i){
        errno=EDOM;
        if(veneer_fixture(key,value,fallback)!=-73||errno!=EAGAIN||strcmp(value,"unchanged-args"))++failures;
    }
    printf("calls=%u failures=%u registers=10 fp_lr_sp=checked args_return_errno=checked\n",calls,failures);
    return failures?1:0;
}
