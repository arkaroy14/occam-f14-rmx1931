#pragma once
// Our own property_get export only. No donor instruction is replaced.
// AAPCS64: SP stays aligned; x0-x2 reach the handler unchanged. Callee-saved
// registers, the original LR and entry SP are copied before any C++ prologue.
#define OCFRESH_PROPERTY_VENEER \
    "sub sp, sp, #112\n" \
    "stp x19, x20, [sp, #0]\n" \
    "stp x21, x22, [sp, #16]\n" \
    "stp x23, x24, [sp, #32]\n" \
    "stp x25, x26, [sp, #48]\n" \
    "stp x27, x28, [sp, #64]\n" \
    "stp x29, x30, [sp, #80]\n" \
    "add x9, sp, #112\n" \
    "str x9, [sp, #96]\n" \
    "str xzr, [sp, #104]\n" \
    "mov x3, sp\n" \
    "bl oc_f14_property_get\n" \
    "ldp x19, x20, [sp, #0]\n" \
    "ldp x21, x22, [sp, #16]\n" \
    "ldp x23, x24, [sp, #32]\n" \
    "ldp x25, x26, [sp, #48]\n" \
    "ldp x27, x28, [sp, #64]\n" \
    "ldp x29, x30, [sp, #80]\n" \
    "add sp, sp, #112\n" \
    "ret\n"

struct F14PropertyEntry {
    uintptr_t x19_x30[12];
    uintptr_t entry_sp, reserved;
};
static_assert(sizeof(F14PropertyEntry)==112, "property entry veneer layout");
