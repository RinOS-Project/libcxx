/*
 * RinOS C++26 <rcu>
 * Epoch-based read-copy update and deferred reclamation.
 */

#ifndef RINCXX_RCU_H
#define RINCXX_RCU_H

#include "rincxx.h"

#if defined(__cplusplus) && __cplusplus > 202002L && \
    (__cplusplus > 202302L || defined(RIN_ENABLE_CXX26_RCU))

#include "atomic.h"
#include "exception.h"
#include "memory.h"
#include "type_traits.h"
#include "utility.h"

#ifndef __cpp_lib_rcu
#define __cpp_lib_rcu 202306L
#endif

namespace std {

class rcu_domain;
inline rcu_domain& rcu_default_domain() noexcept;
inline void rcu_synchronize(rcu_domain& dom) noexcept;
inline void rcu_barrier(rcu_domain& dom) noexcept;

template<class T, class D = default_delete<T>>
class rcu_obj_base;

template<class T, class D = default_delete<T>>
void rcu_retire(T* pointer, D deleter = D(),
                rcu_domain& dom = rcu_default_domain());

namespace rcu_detail {

struct scheduled_evaluation {
    scheduled_evaluation* next;
    void (*invoke)(scheduled_evaluation*) noexcept;
    rcu_domain* domain;
    uint64_t ticket;
    bool queued;

    scheduled_evaluation() noexcept
        : next(nullptr), invoke(nullptr), domain(nullptr), ticket(0), queued(false) {}
};

struct local_region {
    rcu_domain* domain;
    uint64_t generation;
    size_t depth;
};

/* Reader registration never allocates in rcu_domain::lock(), which is
 * noexcept.  Threads beyond this small per-thread table use the domain's
 * generation-gated fallback counter. */
inline thread_local local_region local_regions[64] = {};

inline local_region* find_region(rcu_domain* domain) noexcept {
    for (local_region& region : local_regions)
        if (region.domain == domain) return &region;
    return nullptr;
}

inline local_region* find_free_region() noexcept {
    for (local_region& region : local_regions)
        if (!region.domain) return &region;
    return nullptr;
}

} // namespace rcu_detail

class rcu_domain {
    atomic<uint64_t> epoch_;
    atomic<size_t> readers_[2];
    atomic<size_t> fallback_readers_;
    atomic<bool> fallback_syncing_;
    atomic_flag synchronize_lock_;

    atomic_flag queue_lock_;
    rcu_detail::scheduled_evaluation* queue_head_;
    rcu_detail::scheduled_evaluation* queue_tail_;
    uint64_t last_ticket_;
    bool draining_;
    atomic<uint64_t> completed_ticket_;

    void spin_lock(atomic_flag& flag) noexcept {
        while (flag.test_and_set(memory_order_acquire)) {}
    }

    static void spin_unlock(atomic_flag& flag) noexcept {
        flag.clear(memory_order_release);
    }

    void release_reader(size_t index) noexcept {
        const size_t previous = readers_[index].fetch_sub(1u, memory_order_release);
        if (previous == 1u) readers_[index].notify_all();
    }

    void release_fallback_reader() noexcept {
        const size_t previous = fallback_readers_.fetch_sub(1u, memory_order_seq_cst);
        if (previous == 1u) fallback_readers_.notify_all();
    }

    void synchronize_impl() noexcept {
        spin_lock(synchronize_lock_);
        fallback_syncing_.store(true, memory_order_seq_cst);
        const uint64_t previous_generation = epoch_.fetch_add(1u, memory_order_seq_cst);
        const size_t reader_index = static_cast<size_t>(previous_generation & 1u);

        size_t readers = readers_[reader_index].load(memory_order_acquire);
        while (readers) {
            readers_[reader_index].wait(readers, memory_order_acquire);
            readers = readers_[reader_index].load(memory_order_acquire);
        }

        size_t fallback = fallback_readers_.load(memory_order_seq_cst);
        while (fallback) {
            fallback_readers_.wait(fallback, memory_order_acquire);
            fallback = fallback_readers_.load(memory_order_seq_cst);
        }

        fallback_syncing_.store(false, memory_order_seq_cst);
        fallback_syncing_.notify_all();
        spin_unlock(synchronize_lock_);
    }

    void maybe_drain() noexcept {
        if (rcu_detail::find_region(this) ||
            fallback_readers_.load(memory_order_seq_cst) != 0u)
            return;

        spin_lock(queue_lock_);
        if (draining_ || !queue_head_) {
            spin_unlock(queue_lock_);
            return;
        }
        draining_ = true;
        spin_unlock(queue_lock_);

        for (;;) {
            spin_lock(queue_lock_);
            rcu_detail::scheduled_evaluation* boundary = queue_tail_;
            if (!boundary) {
                draining_ = false;
                spin_unlock(queue_lock_);
                completed_ticket_.notify_all();
                return;
            }
            spin_unlock(queue_lock_);

            synchronize_impl();

            spin_lock(queue_lock_);
            rcu_detail::scheduled_evaluation* batch = queue_head_;
            queue_head_ = boundary->next;
            if (!queue_head_) queue_tail_ = nullptr;
            boundary->next = nullptr;
            spin_unlock(queue_lock_);

            while (batch) {
                rcu_detail::scheduled_evaluation* current = batch;
                batch = current == boundary ? nullptr : current->next;
                const uint64_t ticket = current->ticket;
                current->next = nullptr;
                current->invoke(current);
                completed_ticket_.store(ticket, memory_order_release);
                completed_ticket_.notify_all();
            }
        }
    }

    void schedule(rcu_detail::scheduled_evaluation* evaluation) noexcept {
        spin_lock(queue_lock_);
        if (evaluation->queued || !evaluation->invoke) {
            spin_unlock(queue_lock_);
            terminate();
        }
        if (last_ticket_ == static_cast<uint64_t>(-1)) {
            spin_unlock(queue_lock_);
            terminate();
        }
        evaluation->queued = true;
        evaluation->domain = this;
        evaluation->ticket = ++last_ticket_;
        evaluation->next = nullptr;
        if (queue_tail_) queue_tail_->next = evaluation;
        else queue_head_ = evaluation;
        queue_tail_ = evaluation;
        spin_unlock(queue_lock_);
        maybe_drain();
    }

    void barrier_impl() noexcept {
        spin_lock(queue_lock_);
        const uint64_t target = last_ticket_;
        spin_unlock(queue_lock_);
        if (!target) return;

        maybe_drain();
        uint64_t completed = completed_ticket_.load(memory_order_acquire);
        while (completed < target) {
            completed_ticket_.wait(completed, memory_order_acquire);
            completed = completed_ticket_.load(memory_order_acquire);
        }
    }

    friend void rcu_synchronize(rcu_domain&) noexcept;
    friend void rcu_barrier(rcu_domain&) noexcept;
    template<class T, class D> friend class rcu_obj_base;
    template<class T, class D> friend void rcu_retire(T*, D, rcu_domain&);

public:
    rcu_domain() noexcept
        : epoch_(0), readers_{atomic<size_t>(0), atomic<size_t>(0)},
          fallback_readers_(0), fallback_syncing_(false),
          synchronize_lock_(), queue_lock_(), queue_head_(nullptr),
          queue_tail_(nullptr), last_ticket_(0), draining_(false),
          completed_ticket_(0) {}

    rcu_domain(const rcu_domain&) = delete;
    rcu_domain& operator=(const rcu_domain&) = delete;
    ~rcu_domain() { barrier_impl(); }

    void lock() noexcept {
        if (rcu_detail::local_region* region = rcu_detail::find_region(this)) {
            ++region->depth;
            return;
        }

        rcu_detail::local_region* region = rcu_detail::find_free_region();
        if (!region) {
            for (;;) {
                while (fallback_syncing_.load(memory_order_acquire))
                    fallback_syncing_.wait(true, memory_order_acquire);
                fallback_readers_.fetch_add(1u, memory_order_seq_cst);
                if (!fallback_syncing_.load(memory_order_seq_cst)) return;
                release_fallback_reader();
            }
        }

        for (;;) {
            const uint64_t generation = epoch_.load(memory_order_seq_cst);
            const size_t index = static_cast<size_t>(generation & 1u);
            readers_[index].fetch_add(1u, memory_order_seq_cst);
            if (epoch_.load(memory_order_seq_cst) == generation) {
                region->domain = this;
                region->generation = generation;
                region->depth = 1;
                return;
            }
            release_reader(index);
        }
    }

    bool try_lock() noexcept {
        lock();
        return true;
    }

    void unlock() noexcept {
        rcu_detail::local_region* region = rcu_detail::find_region(this);
        if (region) {
            if (!region->depth) terminate();
            if (--region->depth) return;
            const size_t index = static_cast<size_t>(region->generation & 1u);
            region->domain = nullptr;
            region->generation = 0;
            release_reader(index);
        } else {
            if (!fallback_readers_.load(memory_order_seq_cst)) terminate();
            release_fallback_reader();
        }
        maybe_drain();
    }
};

inline rcu_domain& rcu_default_domain() noexcept {
    static rcu_domain domain;
    return domain;
}

inline void rcu_synchronize(rcu_domain& domain) noexcept {
    domain.synchronize_impl();
}

inline void rcu_barrier(rcu_domain& domain) noexcept {
    domain.barrier_impl();
}

namespace rcu_detail {

template<class T, class D>
struct allocated_evaluation : scheduled_evaluation {
    T* pointer;
    D deleter;

    allocated_evaluation(T* value, D&& destroyer)
        : scheduled_evaluation(), pointer(value), deleter(std::move(destroyer)) {
        this->invoke = &run;
    }

    static void run(scheduled_evaluation* base) noexcept {
        allocated_evaluation* evaluation =
            static_cast<allocated_evaluation*>(base);
        evaluation->deleter(evaluation->pointer);
        delete evaluation;
    }
};

template<class T>
struct is_protectable : integral_constant<bool,
    is_class<typename remove_cv<T>::type>::value &&
    is_base_of<scheduled_evaluation, typename remove_cv<T>::type>::value> {};

} // namespace rcu_detail

template<class T, class D>
class rcu_obj_base : public rcu_detail::scheduled_evaluation {
    static_assert(is_class<T>::value, "rcu_obj_base requires a class type");
    D deleter_;

    static void reclaim_object(rcu_detail::scheduled_evaluation* evaluation) noexcept {
        using base_type = rcu_obj_base<T, D>;
        base_type* base = static_cast<base_type*>(evaluation);
        T* object = static_cast<T*>(base);
        base->deleter_(object);
    }

public:
    void retire(D deleter = D(), rcu_domain& domain = rcu_default_domain()) noexcept {
        if (this->queued) terminate();
        deleter_ = std::move(deleter);
        this->invoke = &reclaim_object;
        domain.schedule(static_cast<rcu_detail::scheduled_evaluation*>(this));
    }

protected:
    rcu_obj_base() = default;
    rcu_obj_base(const rcu_obj_base&) = default;
    rcu_obj_base(rcu_obj_base&&) = default;
    rcu_obj_base& operator=(const rcu_obj_base&) = default;
    rcu_obj_base& operator=(rcu_obj_base&&) = default;
    ~rcu_obj_base() = default;
};

template<class T, class D>
void rcu_retire(T* pointer, D deleter, rcu_domain& domain) {
    static_assert(is_move_constructible<D>::value,
                  "rcu_retire requires a move-constructible deleter");
    auto* evaluation = new rcu_detail::allocated_evaluation<T, D>(
        pointer, std::move(deleter));
    domain.schedule(evaluation);
}

} // namespace std

#endif /* C++26 RCU */
#endif /* RINCXX_RCU_H */
