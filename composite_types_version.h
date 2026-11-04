/* C++26 composite vocabulary type feature-test macros. */
#ifndef RINCXX_COMPOSITE_TYPES_VERSION_H
#define RINCXX_COMPOSITE_TYPES_VERSION_H

#if defined(__cplusplus) && __cplusplus > 202002L && \
    (__cplusplus > 202302L || \
     defined(RIN_ENABLE_CXX26_COMPOSITE_VOCABULARY_TYPES))
#define __cpp_lib_indirect 202502L
#define __cpp_lib_polymorphic 202502L
#endif

#endif /* RINCXX_COMPOSITE_TYPES_VERSION_H */
