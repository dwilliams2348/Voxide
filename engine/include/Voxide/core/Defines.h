#include "Voxide/core/Types.h"

#if defined(__clang__) || defined(__GNUC__)
#define VX_LIKELY(x) __builtin_expect(!!(x), 1)
#define VX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define VX_LIKELY(x) (x)
#define VX_UNLIKELY(x) (x)
#endif

#if defined(_MSC_VER)
#define VX_FOREC_INLINE __forceinline
#define VX_NO_INLINE __declspec(noinline)
#else
#define VX_FOREC_INLINE inline __attribute__((always_inline))
#define VX_NO_INLINE __attribute__((noinline))
#endif

#if defined(_MSC_VER)
#define VX_RESTRICT __restrict
#else
#define VX_RESTRICT __restrict__
#endif

#define VX_ALIGNAS(x) alignas(x)

inline constexpr usize kCacheLine = 64;
