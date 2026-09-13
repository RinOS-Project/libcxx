/*
 * RinOS libcxx - cxxabi.h
 * C++ ABI support (minimal implementation)
 *
 * This header provides the C++ ABI functions required for
 * RTTI, exception handling, and other runtime features.
 */

#ifndef _CXXABI_H
#define _CXXABI_H

#include "typeinfo.h"

namespace __cxxabiv1 {

/* ═══════════════════════════════════════════════════════════════
 * Type Info Classes
 * ═══════════════════════════════════════════════════════════════*/

/* Fundamental type info */
class __fundamental_type_info : public std::type_info {
public:
    virtual ~__fundamental_type_info();
};

/* Array type info */
class __array_type_info : public std::type_info {
public:
    virtual ~__array_type_info();
};

/* Function type info */
class __function_type_info : public std::type_info {
public:
    virtual ~__function_type_info();
};

/* Enum type info */
class __enum_type_info : public std::type_info {
public:
    virtual ~__enum_type_info();
};

/* Class type info base */
class __class_type_info : public std::type_info {
public:
    virtual ~__class_type_info();
};

/* Single inheritance class type info */
class __si_class_type_info : public __class_type_info {
public:
    const __class_type_info* __base_type;
    virtual ~__si_class_type_info();
};

/* Virtual/multiple inheritance base class info */
struct __base_class_type_info {
    const __class_type_info* __base_type;
    long __offset_flags;

    enum __offset_flags_masks {
        __virtual_mask = 0x1,
        __public_mask = 0x2,
        __offset_shift = 8
    };
};

/* Multiple inheritance class type info */
class __vmi_class_type_info : public __class_type_info {
public:
    unsigned int __flags;
    unsigned int __base_count;
    __base_class_type_info __base_info[1];

    enum __flags_masks {
        __non_diamond_repeat_mask = 0x1,
        __diamond_shaped_mask = 0x2
    };

    virtual ~__vmi_class_type_info();
};

/* Pointer type info */
class __pbase_type_info : public std::type_info {
public:
    unsigned int __flags;
    const std::type_info* __pointee;

    enum __masks {
        __const_mask = 0x1,
        __volatile_mask = 0x2,
        __restrict_mask = 0x4,
        __incomplete_mask = 0x8,
        __incomplete_class_mask = 0x10,
        __transaction_safe_mask = 0x20,
        __noexcept_mask = 0x40
    };

    virtual ~__pbase_type_info();
};

class __pointer_type_info : public __pbase_type_info {
public:
    virtual ~__pointer_type_info();
};

class __pointer_to_member_type_info : public __pbase_type_info {
public:
    const __class_type_info* __context;
    virtual ~__pointer_to_member_type_info();
};

/* ═══════════════════════════════════════════════════════════════
 * Exception Handling Support
 * ═══════════════════════════════════════════════════════════════*/

struct __cxa_exception;
struct __cxa_eh_globals;

extern "C" {

/* Exception throwing - use void* for type_info to match C ABI */
void __cxa_throw(void* thrown_exception,
                 void* tinfo,
                 void (*dest)(void*));

/* Exception catching */
void* __cxa_begin_catch(void* exc_obj);
void __cxa_end_catch();

/* Exception allocation */
void* __cxa_allocate_exception(size_t thrown_size) noexcept;
void __cxa_free_exception(void* thrown_exception) noexcept;

/* Rethrowing */
[[noreturn]] void __cxa_rethrow();

/* Exception globals */
__cxa_eh_globals* __cxa_get_globals();
__cxa_eh_globals* __cxa_get_globals_fast();

/* ═══════════════════════════════════════════════════════════════
 * Guard Variables (for static initialization)
 * ═══════════════════════════════════════════════════════════════*/

int __cxa_guard_acquire(long long* guard_object);
void __cxa_guard_release(long long* guard_object);
void __cxa_guard_abort(long long* guard_object);

/* ═══════════════════════════════════════════════════════════════
 * Virtual Function Support
 * ═══════════════════════════════════════════════════════════════*/

void __cxa_pure_virtual();
void __cxa_deleted_virtual();

/* ═══════════════════════════════════════════════════════════════
 * atexit Support
 * ═══════════════════════════════════════════════════════════════*/

int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle);
void __cxa_finalize(void* dso_handle);

/* DSO handle */
extern void* __dso_handle;

/* ═══════════════════════════════════════════════════════════════
 * Dynamic Cast Support
 * ═══════════════════════════════════════════════════════════════*/

void* __dynamic_cast(const void* src_ptr,
                     const __class_type_info* src_type,
                     const __class_type_info* dst_type,
                     ptrdiff_t src2dst);

/* ═══════════════════════════════════════════════════════════════
 * Demangling
 * ═══════════════════════════════════════════════════════════════*/

char* __cxa_demangle(const char* mangled_name,
                     char* output_buffer,
                     size_t* length,
                     int* status);

} /* extern "C" */

} /* namespace __cxxabiv1 */

/* Standard namespace alias */
namespace abi = __cxxabiv1;

#endif /* _CXXABI_H */
