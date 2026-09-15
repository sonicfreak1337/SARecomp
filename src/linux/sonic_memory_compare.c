// The Zig compiler runtime compares buffers byte by byte. Use bounded word
// loads for short ranges and glibc's CPU-selected comparison for larger ones.
// Hidden definitions are essential: do not interpose on shared libraries or
// export an unversioned name that could bind our own versioned import again.
#include <features.h>
#include <stddef.h>
#include <stdint.h>
#if !defined(__linux__) || !defined(__x86_64__) || !defined(__GLIBC__)
#error This bridge requires the existing x86-64 glibc Linux target.
#endif

extern int sonic_glibc_memcmp(const void *, const void *, size_t);
__asm__(".symver sonic_glibc_memcmp,memcmp@GLIBC_2.2.5");

#define SONIC_LOCAL __attribute__((visibility("hidden")))
static __attribute__((always_inline)) inline int small_compare(
        const unsigned char *left,const unsigned char *right,size_t size,int ordered) {
    while(size>=8) {
        uint64_t a,b;
        // Constant-size builtins become unaligned loads, without aliasing UB,
        // a libc call, alignment assumptions or reading beyond either range.
        __builtin_memcpy(&a,left,8);__builtin_memcpy(&b,right,8);
        if(a!=b) {
            if(!ordered)return 1;
            unsigned shift=(unsigned)__builtin_ctzll(a^b)&~7u;
            return (int)((a>>shift)&255u)-(int)((b>>shift)&255u);
        }
        left+=8;right+=8;size-=8;
    }
    for(size_t i=0;i<size;++i)
        if(left[i]!=right[i])return ordered?(int)left[i]-(int)right[i]:1;
    return 0;
}
// Preserve the compiler runtime's zero-length behavior, including null
// pointers. For nonzero lengths the ordinary C memory contracts apply.
SONIC_LOCAL int memcmp(const void *left, const void *right, size_t size) {
    if(size<128)return small_compare(left,right,size,1);
    return sonic_glibc_memcmp(left,right,size);
}
SONIC_LOCAL int bcmp(const void *left, const void *right, size_t size) {
    // Keep the old runtime's precise 0/1 result, not just its zero test.
    if(size<128)return small_compare(left,right,size,0);
    return sonic_glibc_memcmp(left,right,size)!=0;
}
