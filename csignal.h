/*
 * RinOS C++ <csignal>
 * C++ wrapper for signal.h
 */

#ifndef RINCXX_CSIGNAL_H
#define RINCXX_CSIGNAL_H

#include <signal.h>

#ifdef __cplusplus

namespace std {
    using ::sig_atomic_t;
    using ::signal;
    using ::raise;
}

#endif /* __cplusplus */
#endif /* RINCXX_CSIGNAL_H */
