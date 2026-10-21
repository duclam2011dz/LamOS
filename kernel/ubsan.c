/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
// These names are mandated by the GCC instrumentation ABI.
// NOLINTBEGIN(bugprone-reserved-identifier)
struct SourceLocation {
    const char *file;
    uint32_t line, column;
};
__attribute__((no_sanitize("undefined"))) static _Noreturn void report(const char *kind,
                                                                       const void *data) {
    const struct SourceLocation *loc = data;
    print("UBSAN:%s %s:%u:%u\n", kind, loc->file, (uint64_t)loc->line, (uint64_t)loc->column);
    PANIC("Undefined behavior");
}
#define HANDLER(name)                                                                              \
    __attribute__((no_sanitize("undefined"))) void __ubsan_handle_##name##_abort(void *data,       \
                                                                                 ...) {            \
        report(#name, data);                                                                       \
    }
HANDLER(add_overflow)
HANDLER(sub_overflow)
HANDLER(mul_overflow)
HANDLER(divrem_overflow)
HANDLER(negate_overflow)
HANDLER(shift_out_of_bounds)
HANDLER(out_of_bounds)
HANDLER(type_mismatch_v1)
HANDLER(pointer_overflow)
HANDLER(load_invalid_value)
HANDLER(vla_bound_not_positive)
HANDLER(nonnull_arg)
HANDLER(nonnull_return_v1)
HANDLER(invalid_builtin)
void __ubsan_handle_builtin_unreachable(void *data) {
    report("unreachable", data);
}
void __ubsan_handle_missing_return(void *data) {
    report("missing_return", data);
}

// NOLINTEND(bugprone-reserved-identifier)
