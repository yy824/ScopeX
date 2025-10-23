#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <cassert>
#include <new>
#include <type_traits>
#include <atomic>

namespace concurrency {

// ------------- Single Producer Single Consumer Ring Buffer -------------
template <typename T>
class SpscRing {
public:
    explicit SpscRing(std::size_t capacity_pow2) : capacity_(capacity_pow2),
        mask_(capacity_pow2 - 1),
        buffer_(static_cast<T*>(::operator new[](sizeof(T) * capacity_pow2, std::align_val_t{alignof(T)})))
    {
        // ensure capacity is power of 2
        assert(capacity_pow2 && (capacity_pow2 & mask_)==0);
    }

    ~SpscRing() {
        T tmp;
        while(pop(tmp)) {};
        ::operator delete[](buffer_, std::align_val_t{alignof(T)});
    }

    SpscRing(const SpscRing&) = delete; // forbid to copy
    SpscRing& operator=(const SpscRing&) = delete; // forbid to copy

    // -- Producer --
    bool push(const T& val) noexcept(std::is_nothrow_copy_constructible<T>::value)
    {
        return emplace_impl(val);
    }

    bool push(T&& val) noexcept(std::is_nothrow_move_constructible<T>::value)
    {
        return emplace_impl(std::move(val));
    }

    template <class... args_t>
    bool emplace(args_t&&... args) noexcept(std::is_nothrow_constructible<T, args_t...>::value)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        // update head from consumer if it is necessary
        const std::size_t head = head_cache_for_producer();
        if ((tail - head) == capacity_) {return false;}

        ::new(static_cast<void*>(addr(tail))) T(std::forward<args_t>(args)...);
        tail_.store(next_index(tail),  std::memory_order_release);
        return true;
    }

    // -- Consumer --
    bool pop(T& out) noexcept
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);

        // update tail from producer only if it is necessary
        const std::size_t tail = tail_cache_for_consumer();

        // empty if head == tail
        if(head == tail) {return false;}

        T* pointer = addr(head);
        out = std::move(*pointer);

        pointer->~T();
        head_.store(next_index(head), std::memory_order_release);
        return true;
    }

    std::size_t try_pop_n(T* out, std::size_t max_n) noexcept
    {
        std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_cache_for_consumer();
        std::size_t num = 0;

        while(num < max_n && head != tail)
        {
            T* pointer = addr(head);
            out[num++] = std::move(*pointer);
            pointer->~T();
            head = next_index(head);
        }

        if(num != 0) {head_.store(head, std::memory_order_release);}
        return num;
    }

    std::size_t approx_size() const noexcept
    {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        return (tail + capacity_ - head) & mask_;
    }

    std::size_t capacity() const noexcept { return capacity_;}

private:
    T* const buffer_;
    const std::size_t capacity_;
    const std::size_t mask_;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};

    // cache: buffer for producer/consumer to reduce frequency of atomic load
    alignas(64) std::size_t head_cached_for_producer_{0};
    alignas(64) std::size_t tail_cached_for_consumer_{0};

    template<class U>
    bool emplace_impl(U&& val)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        // make sure head is the latest one (it could be already consumed)
        const std::size_t head = head_cache_for_producer();
        if ((tail - head) == capacity_) {return false;}// full

        // adding new data at tail.
        ::new(static_cast<void*>(addr(tail))) T(std::forward<U>(val));
        tail_.store(next_index(tail), std::memory_order_release);
        return true;
    }

    // reduce buffered head for producer(reduce frequency of acquire)
    std::size_t head_cache_for_producer() noexcept
    {
        // read previous stored index of consumer
        std::size_t head = head_cached_for_producer_;

        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        // if it is going to be full
        if((tail-head) >= capacity_)
        {
            head = head_.load(std::memory_order_acquire);
            head_cached_for_producer_ = head;
        }
        return head;
    }

    std::size_t tail_cache_for_consumer() noexcept
    {
        // read previous stored index of producer
        std::size_t tail = tail_cached_for_consumer_;
        const std::size_t head = head_.load(std::memory_order_relaxed);

        // only head == tail is critical
        if(head == tail) 
        {
            tail = tail_.load(std::memory_order_acquire);
            tail_cached_for_consumer_ = tail;
        }
        return tail;
    }

    static constexpr std::size_t next_index(std::size_t index) noexcept
    {
        return index + 1;
    }

    T* addr(std::size_t index) noexcept { return buffer_ + (index & mask_);}
};

} //namespace concurrency