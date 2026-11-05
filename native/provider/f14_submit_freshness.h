#pragma once
#include <stdint.h>

// Host-qualified bookkeeping only. Not included by the live selector.
// The eventual adapter must supply an attested CHI frame captured at the
// original copier entry, not infer it from a recycled metadata pointer.
namespace f14submit {
struct Copy {
    uint64_t epoch = 0, frame = 0, sequence = 0;
    uintptr_t handle = 0, source = 0;
    bool valid = false;
};
enum class State { linked, no_copy, invalid_record, epoch_changed, frame_changed, handle_changed };
struct Link { State state; Copy copy; };
class Witness {
    Copy pending{};
    uint64_t sequence = 0;
public:
    Copy take() { Copy copy=pending; pending={}; return copy; }
    // Clear before the original copy, including failures or unqualified sites.
    void begin_copy() { pending = {}; }
    void end_copy(uint64_t epoch, uint64_t frame, uintptr_t handle, uintptr_t source,
                  uint32_t original_result, bool qualified_site) {
        pending = {};
        if (!epoch || !handle || !source || original_result || !qualified_site) return;
        pending = Copy{epoch, frame, ++sequence, handle, source, true};
    }
    // Consume on EVERY intercepted submit, even unreadable/unqualified ones.
    // A second submission of a recycled handle must not inherit the witness.
    static Link match(Copy copy, uint64_t epoch, uint64_t frame, uintptr_t handle, bool valid_record) {
        if (!copy.valid) return {State::no_copy, copy};
        if (!valid_record) return {State::invalid_record, copy};
        if (!epoch || epoch != copy.epoch) return {State::epoch_changed, copy};
        if (frame != copy.frame) return {State::frame_changed, copy};
        if (!handle || handle != copy.handle) return {State::handle_changed, copy};
        return {State::linked, copy};
    }
    Link submit(uint64_t epoch, uint64_t frame, uintptr_t handle, bool valid_record) {
        return match(take(),epoch,frame,handle,valid_record);
    }
};
} // namespace f14submit
