//
// Created by 甘尧 on 2019-09-24.
//

#pragma once

#include <cstdint>
#include <mutex>
#include <array>
#include <atomic>
#include "compiler.h"
#include <boost/smart_ptr/intrusive_ptr.hpp>

using u8 = std::uint8_t;   ///< 8-bit unsigned byte
using u16 = std::uint16_t; ///< 16-bit unsigned short
using u32 = std::uint32_t; ///< 32-bit unsigned word
using u64 = std::uint64_t; ///< 64-bit unsigned int
using u128 = std::array<u64, 2>;

using s8 = std::int8_t;   ///< 8-bit signed byte
using s16 = std::int16_t; ///< 16-bit signed short
using s32 = std::int32_t; ///< 32-bit signed word
using s64 = std::int64_t; ///< 64-bit signed int

using f32 = float;  ///< 32-bit floating point
using f64 = double; ///< 64-bit floating point
using f128 = u128; ///< 64-bit floating point

using VAddr = std::size_t;    ///< Represents a pointer in the userspace virtual address space.

using LockGuard = std::lock_guard<std::mutex>;

struct Base {
};

#define CONCAT2(x, y) DO_CONCAT2(x, y)
#define DO_CONCAT2(x, y) x##y

#define FORCE_INLINE inline __attribute__((always_inline))

#define BitField32(name, offset) u32 name: offset;
#define BitField16(name, offset) u16 name: offset;

template<typename U, typename T>
U ForceCast(T *x) {
    return (U) (uintptr_t) x;
}

template<typename U, typename T>
U ForceCast(T &x) {
    return *(U *) &x;
}

template<typename T>
struct Identity {
    using type = T;
};


template<typename T>
constexpr T RoundDown(T x, typename Identity<T>::type n) {
    return (x & -n);
}

template<typename T>
constexpr T RoundUp(T x, typename std::remove_reference<T>::type n) {
    return RoundDown(x + n - 1, n);
}


template <typename T>
constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return static_cast<T>(value + (size - value % size) % size);
}

template <typename T>
constexpr T AlignDown(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return static_cast<T>(value - value % size);
}


namespace pointer {

    template<typename T = void *>
    constexpr intptr_t as_intptr(T in) {
        return reinterpret_cast<intptr_t>(in);
    }

    template<typename R, typename T>
    constexpr R as_pointer(T in) {
        return reinterpret_cast<R>(in);
    }


};  // end namespace pinter

template<typename T>
constexpr bool TestBit(T val, size_t pos) {
    return (val & (static_cast<T>(1) << pos)) != 0;
}

template<size_t pos, typename T>
constexpr bool TestBit(T val) {
    return (val & (static_cast<T>(1) << (pos))) != 0;
}

template<typename T>
constexpr T Bitmask(T size) {
    return (size == sizeof(T) * 8)? std::numeric_limits<T>::max() : (static_cast<T>(1) << size) - 1;
}

template<size_t size, typename T>
constexpr T Bitmask() {
    return (static_cast<T>(1) << (size)) - 1;
}

template<>
constexpr uint32_t Bitmask<32, uint32_t>() {
    return 0xFFFFFFFF;
}

template<>
constexpr uint64_t Bitmask<64, uint64_t>() {
    return 0xFFFFFFFFFFFFFFFF;
}

template<typename T>
constexpr T BitRange(T val, T start, T end) {
    return (val >> start) & Bitmask(end - start + 1);
}

template<size_t start, size_t end, typename T>
constexpr T BitRange(T val) {
    return (val >> start) & Bitmask<end - start + 1, T>();
}

template<size_t bit, typename T>
constexpr T Bit(T val) {
    return (val >> bit) & Bitmask<1, T>();
}

template <typename T>
constexpr T SignExtendX(u8 size, T val) {
    T mask = (T(2) << (size - 1)) - T(1);
    val &= mask;
    T sign_bits = -((val >> (size - 1)) << size);
    val |= sign_bits;
    return val;
}

template <typename T, u8 size>
constexpr T SignExtend(T val) {
    T mask = (T(2) << (size - 1)) - T(1);
    val &= mask;
    T sign_bits = -((val >> (size - 1)) << size);
    val |= sign_bits;
    return val;
}

constexpr u64 TruncateToUintN(unsigned n, u64 x) {
    return static_cast<u64>(x) & ((UINT64_C(1) << n) - 1);
}

template<unsigned size>
constexpr u32 TruncateUTo(u64 val) {
    return static_cast<u32>(TruncateToUintN(size, val));
}

template<unsigned size>
constexpr u32 TruncateSTo(s64 val) {
    return static_cast<u32>(TruncateToUintN(size, val));
}

template<typename T>
constexpr T ConstLog2(T num) {
    return (num == 1)? 0 : 1 + ConstLog2(num >> 1);
}

constexpr u64 LowestSetBit(u64 value) { return value & -value; }

template <typename V>
constexpr bool IsPowerOf2(V value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

int CountLeadingZerosFallBack(uint64_t value, int width);

template <typename V>
constexpr int CountLeadingZeros(V value, int width = (sizeof(V) * 8)) {
#if COMPILER_HAS_BUILTIN_CLZ
    if (width == 32) {
    return (value == 0) ? 32 : __builtin_clz(static_cast<unsigned>(value));
  } else if (width == 64) {
    return (value == 0) ? 64 : __builtin_clzll(value);
  }
#endif
    return CountLeadingZerosFallBack(value, width);
}

constexpr u64 RotateRight(u64 value,
                            unsigned int rotate,
                            unsigned int width) {
    assert((width > 0) && (width <= 64));
    u64 width_mask = ~UINT64_C(0) >> (64 - width);
    rotate &= 63;
    if (rotate > 0) {
        value &= width_mask;
        value = (value << (width - rotate)) | (value >> rotate);
    }
    return value & width_mask;
}


//share ptr
template<typename T>
using SharedPtr = std::shared_ptr<T>;

class BaseObject : public std::enable_shared_from_this<BaseObject> {
};

template<typename T>
inline SharedPtr<T> DynamicObjectCast(SharedPtr<BaseObject> object) {
    if (object != nullptr) {
        return std::static_pointer_cast<T>(object);
    }
    return nullptr;
}

template<typename T>
std::shared_ptr<T> SharedFrom(T *raw) {
    if (raw == nullptr)
        return nullptr;
    return std::static_pointer_cast<T>(raw->shared_from_this());
}

using ObjectRef = SharedPtr<BaseObject>;

#define ARG_LIST(...) __VA_ARGS__

#define OFFSET_OF(t, d) __builtin_offsetof(t, d)


template<typename Function, typename Tuple, std::size_t... Index>
decltype(auto) invoke_impl(Function&& func, Tuple&& t, std::index_sequence<Index...>)
{
    return func(std::get<Index>(std::forward<Tuple>(t))...);
}

template<typename Function, typename Tuple>
decltype(auto) invoke(Function&& func, Tuple&& t)
{
    constexpr auto size = std::tuple_size<typename std::decay<Tuple>::type>::value;
    return invoke_impl(std::forward<Function>(func), std::forward<Tuple>(t), std::make_index_sequence<size>{});
}

template<typename Array, std::size_t... Index>
decltype(auto) array2tuple_impl(const Array& a, std::index_sequence<Index...>)
{
    return std::make_tuple(a[Index]...);
}

template<typename T, std::size_t N>
decltype(auto) array2tuple(const std::array<T, N>& a)
{
    return array2tuple_impl(a, std::make_index_sequence<N>{});
}

/// Used to provide information about an arbitrary function.
template <typename Function>
struct FunctionInfo : public FunctionInfo<decltype(&Function::operator())>
{
};

/**
 * Partial specialization for function types.
 *
 * This is used as the supporting base for all other specializations.
 */
template <typename R, typename... Args>
struct FunctionInfo<R(Args...)>
{
    using return_type = R;
    static constexpr size_t args_count = sizeof...(Args);

    template <size_t ParameterIndex>
    struct Parameter
    {
        using type = std::tuple_element_t<ParameterIndex, std::tuple<Args...>>;
    };

    using equivalent_function_type = R(Args...);
};

/// Partial specialization for function pointers
template <typename R, typename... Args>
struct FunctionInfo<R(*)(Args...)> : public FunctionInfo<R(Args...)>
{
};

/// Partial specialization for member function pointers.
template <typename C, typename R, typename... Args>
struct FunctionInfo<R(C::*)(Args...)> : public FunctionInfo<R(Args...)>
{
    using class_type = C;
};

/// Partial specialization for const member function pointers.
template <typename C, typename R, typename... Args>
struct FunctionInfo<R(C::*)(Args...) const> : public FunctionInfo<R(Args...)>
{
    using class_type = C;
};

/**
 * Helper template for retrieving the type of a function parameter.
 *
 * @tparam Function       An arbitrary function type.
 * @tparam ParameterIndex Zero-based index indicating which parameter to get the type of.
 */
template <typename Function, size_t ParameterIndex>
using parameter_type_t = typename FunctionInfo<Function>::template Parameter<ParameterIndex>::type;

/**
 * Helper template for retrieving the return type of a function.
 *
 * @tparam Function The function type to get the return type of.
 */
template <typename Function>
using return_type_t = typename FunctionInfo<Function>::return_type;

/**
 * Helper template for retrieving the class type of a member function.
 *
 * @tparam Function The function type to get the return type of.
 */
template <typename Function>
using class_type_t = typename FunctionInfo<Function>::class_type;

/**
 * Helper template for retrieving the equivalent function type of a member function or functor.
 *
 * @tparam Function The function type to get the return type of.
 */
template <typename Function>
using equivalent_function_type_t = typename FunctionInfo<Function>::equivalent_function_type;

void ClearCachePlatform(VAddr start, VAddr size);

class NonCopyable {
protected:
    constexpr NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

class SpinMutex {
    std::atomic<int> flag = ATOMIC_VAR_INIT(-1);
public:
    SpinMutex() = default;
    void Lock() {
        int expected = -1;
        int tid = gettid();
        while(!flag.compare_exchange_strong(expected, tid)) {
            expected = -1;
            sched_yield();
        }
    }

    void Unlock() {
        flag.store(-1);
    }

    bool TryLock() {
        return flag == -1;
    }

    bool LockedBySelf() {
        return flag == gettid();
    }
};

class SpinLockGuard {
public:
    SpinLockGuard(SpinMutex &mutex);
    ~SpinLockGuard();
    void Lock();
    void Unlock();
private:
    SpinLockGuard(SpinLockGuard const&) = delete;
    SpinLockGuard& operator=(SpinLockGuard const&) = delete;
    SpinMutex &mutex_;
};