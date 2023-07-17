#pragma once 

// MSVC++ with unknown standard lib
#ifndef RPP_MSVC
#  define RPP_MSVC _MSC_VER
#endif

// MSVC++ with VC standard libs
#ifndef RPP_MSVC_WIN
#  define RPP_MSVC_WIN (_WIN32 && _MSC_VER)
#endif

// GCC with unknown standard lib
#ifndef RPP_GCC
#  define RPP_GCC (__GNUC__ && !__clang__)
#endif

// Clang using GCC/LLVM standard libs
#ifndef RPP_CLANG_LLVM
#  define RPP_CLANG_LLVM (__GNUC__ && __clang__)
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI //__declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

#ifndef RPP_EXTERNC
#  ifdef __cplusplus
#    define RPP_EXTERNC extern "C"
#  else
#    define RPP_EXTERNC
#  endif
#endif

#ifndef RPPCAPI
#  define RPPCAPI RPP_EXTERNC RPPAPI
#endif

#if __cplusplus
#  if _MSC_VER
#    define RPP_HAS_CXX17 (_MSVC_LANG > 201402)
#  else
#    define RPP_HAS_CXX17 (__cplusplus >= 201703L)
#  endif
#endif

#if __cplusplus
#  if _MSC_VER
#    define RPP_HAS_CXX20 (_MSVC_LANG >= 202002L)
#  else
#    define RPP_HAS_CXX20 (__cplusplus >= 202002L)
#  endif
#endif

#if __cplusplus
#  if _MSC_VER
#    define RPP_HAS_CXX23 (_MSVC_LANG > 202002L)
#  else
#    define RPP_HAS_CXX23 (__cplusplus > 202002L)
#  endif
#endif

#if __cplusplus
#  if RPP_HAS_CXX17
#    define RPP_INLINE_STATIC inline static
#  else
#    define RPP_INLINE_STATIC static
#  endif
#endif

#if __cplusplus
#  if __cpp_if_constexpr
#    define RPP_CXX17_IF_CONSTEXPR if constexpr
#  else
#    define RPP_CXX17_IF_CONSTEXPR if
#  endif
#endif

//// @note Some functions get inlined too aggressively, leading to some serious code bloat
////       Need to hint the compiler to take it easy ^_^'
#ifndef NOINLINE
#  if _MSC_VER
#    define NOINLINE __declspec(noinline)
#  else
#    define NOINLINE __attribute__((noinline))
#  endif
#endif

//// @note Some strong hints that some functions are merely wrappers, so should be forced inline
#ifndef FINLINE
#  if _MSC_VER
#    define FINLINE __forceinline
#  elif __APPLE__
#    define FINLINE inline __attribute__((always_inline))
#  else
#    define FINLINE __attribute__((always_inline))
#  endif
#endif

#if INTPTR_MAX == INT64_MAX
#  define RPP_64BIT 1
#endif

#ifdef _LIBCPP_STD_VER
#  define _HAS_STD_BYTE (_LIBCPP_STD_VER > 16)
#elif !defined(_HAS_STD_BYTE)
#  define _HAS_STD_BYTE 0
#endif

#ifndef NOCOPY_NOMOVE
#define NOCOPY_NOMOVE(T) \
    T(T&& fwd)             = delete; \
    T& operator=(T&& fwd)  = delete; \
    T(const T&)            = delete; \
    T& operator=(const T&) = delete;
#endif

#ifndef NODISCARD
#  if __GNUC__ <= 6
#    define NODISCARD __attribute__((warn_unused_result))
#  elif __cplusplus >= 201500
#    define NODISCARD [[nodiscard]]
#  elif _MSC_VER > 1916
#    ifdef __has_cpp_attribute
#      if __has_cpp_attribute(nodiscard)
#        define NODISCARD [[nodiscard]]
#      endif
#    endif
#    ifndef NODISCARD
#      define NODISCARD
#    endif
#  else
#    define NODISCARD
#  endif
#endif

#ifdef __cplusplus
namespace rpp
{
    #ifndef RPP_BASIC_INTEGER_TYPEDEFS
    #define RPP_BASIC_INTEGER_TYPEDEFS
        using byte   = unsigned char;
        using ushort = unsigned short;
        using uint   = unsigned int;
        using ulong  = unsigned long;
        using int64  = long long;
        using uint64 = unsigned long long;
    #endif
}
#endif
