#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// The full game needs the compiler's 128-bit arithmetic archive; that archive
// also introduces the weak memory routines under comparison. Force the same
// archive into this small control executable, which would otherwise use libc.
extern unsigned __int128 __udivti3(unsigned __int128,unsigned __int128);

static void require(int ok, const char *message) {
    if (!ok) { fprintf(stderr,"LINUX_MEMORY_FAILURE %s\n",message); exit(1); }
}
static unsigned char pattern(size_t i) { return (unsigned char)((i*37u)^(i>>3)); }
static void initialize(unsigned char *p,size_t n) {
    for(size_t i=0;i<n;++i)p[i]=pattern(i);
}
static unsigned char *guarded(size_t page) {
    unsigned char *p=mmap(NULL,page*3,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    require(p!=MAP_FAILED,"mmap");
    require(!mprotect(p+page,page,PROT_READ|PROT_WRITE),"mprotect");
    return p+page;
}
static void semantics(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE), cases=0;
    require(page>=4096,"page size");
    unsigned char *a=guarded(page),*b=guarded(page),*expected=malloc(page);
    require(expected!=NULL,"allocation");
    // The retained runtime accepts null for zero bytes. Call through plain
    // pointers so libc headers' nonnull annotations cannot erase this check.
    void *(*volatile copy_zero)(void *,const void *,size_t)=memcpy;
    void *(*volatile move_zero)(void *,const void *,size_t)=memmove;
    void *(*volatile fill_zero)(void *,int,size_t)=memset;
    int (*volatile compare_zero)(const void *,const void *,size_t)=memcmp;
    int (*volatile equal_zero)(const void *,const void *,size_t)=bcmp;
    require(copy_zero(NULL,NULL,0)==NULL && move_zero(NULL,NULL,0)==NULL &&
            fill_zero(NULL,0,0)==NULL && !compare_zero(NULL,NULL,0) && !equal_zero(NULL,NULL,0),"zero length");
    for(size_t n=0;n<=page;n+=(n<512?1:127)) {
        for(unsigned placement=0;placement<4;++placement) {
            size_t left=(placement&1)?page-n:0,right=(placement&2)?page-n:0;
            initialize(a,page);initialize(b,page);
            for(size_t i=0;i<n;++i)b[right+i]=0xED;
            require(memcpy(b+right,a+left,n)==b+right,"memcpy return");
            for(size_t i=0;i<page;++i)
                require(b[i]==(i>=right && i<right+n?pattern(left+i-right):pattern(i)),"memcpy bytes/bounds");
            require(!memcmp(a+left,b+right,n) && !bcmp(a+left,b+right,n),"equal ranges");
            if(n) {
                b[right+n-1]^=1;
                int diff=(int)a[left+n-1]-(int)b[right+n-1],cmp=memcmp(a+left,b+right,n);
                require((cmp>0)-(cmp<0)==(diff>0)-(diff<0) && bcmp(a+left,b+right,n)==1,"unequal ranges");
            }
            const int values[]={-257,-1,0,1,255,256,0x12345678};
            for(size_t v=0;v<sizeof(values)/sizeof(*values);++v) {
                initialize(b,page);
                require(memset(b+right,values[v],n)==b+right,"memset return");
                for(size_t i=0;i<page;++i)
                    require(b[i]==(i>=right && i<right+n?(unsigned char)values[v]:pattern(i)),"memset bytes/bounds");
            }
            ++cases;
        }
    }
    for(size_t n=0;n<=1024;n+=13)
        for(int shift=-63;shift<=63;shift+=7) {
            size_t source=128,destination=(size_t)((int)source+shift);
            initialize(a,page);initialize(expected,page);
            for(size_t i=0;i<n;++i)expected[destination+i]=pattern(source+i);
            require(memmove(a+destination,a+source,n)==a+destination,"memmove return");
            for(size_t i=0;i<page;++i)require(a[i]==expected[i],"overlapping memmove/bounds");
            ++cases;
        }
    for(unsigned left=0;left<256;++left)
        for(unsigned right=0;right<256;++right) {
            a[0]=(unsigned char)left;b[0]=(unsigned char)right;
            int cmp=memcmp(a,b,1),diff=(int)left-(int)right;
            require((cmp>0)-(cmp<0)==(diff>0)-(diff<0) && bcmp(a,b,1)==(left!=right),"unsigned comparison");
            ++cases;
        }
    for(size_t n=1;n<=page;n+=17) {
        initialize(a,page);initialize(b,page);
        b[n/3]^=128;b[n-1]^=1;
        int expected_sign=0;
        for(size_t i=0;i<n;++i)if(a[i]!=b[i]){expected_sign=a[i]>b[i]?1:-1;break;}
        int result=memcmp(a,b,n);
        require((result>0)-(result<0)==expected_sign && bcmp(a,b,n)==1,"first differing byte");
        ++cases;
    }
    free(expected);munmap(a-page,page*3);munmap(b-page,page*3);
    Dl_info info={0};void *selected=dlvsym(RTLD_DEFAULT,"memcmp","GLIBC_2.2.5");
    require(selected && dladdr(selected,&info),"versioned libc provider");
    printf("LINUX_MEMORY_OK cases=%zu libc=%s\n",cases,info.dli_fname);
}
static uint64_t cpu_ns(void) {
    struct timespec now;require(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&now),"thread clock");
    return (uint64_t)now.tv_sec*1000000000u+(uint64_t)now.tv_nsec;
}
static void benchmark(void) {
    const size_t sizes[]={16,64,256,4096,65536};
    // Plain volatile function pointers prevent libc pure/builtin annotations
    // from hoisting an unchanged comparison out of the timed loop.
    void *(*volatile copy)(void *,const void *,size_t)=memcpy;
    void *(*volatile move)(void *,const void *,size_t)=memmove;
    void *(*volatile fill)(void *,int,size_t)=memset;
    int (*volatile compare)(const void *,const void *,size_t)=memcmp;
    int (*volatile equal)(const void *,const void *,size_t)=bcmp;
    unsigned char *a=malloc(65536+64),*b=malloc(65536+64);
    require(a&&b,"benchmark allocation");unsigned checksum=0;
    for(size_t s=0;s<sizeof(sizes)/sizeof(*sizes);++s) {
        size_t n=sizes[s],loops=8388608u/n;
        if(loops>200000)loops=200000;
        initialize(a,65536+64);initialize(b,65536+64);
        for(unsigned kind=0;kind<5;++kind) {
            memcpy(b+7,a+3,n);
            uint64_t start=cpu_ns();
            for(size_t i=0;i<loops;++i) {
                switch(kind) {
                case 0: copy(b+7,a+3,n);checksum+=b[7+i%n];break;
                case 1: fill(b+7,(int)i,n);checksum+=b[7+i%n];break;
                case 2: checksum+=(unsigned)equal(a+3,b+7,n);break;
                case 3: checksum+=(unsigned)compare(a+3,b+7,n);break;
                case 4: move(b+7,b+3,n);checksum+=b[7+i%n];break;
                }
            }
            printf("LINUX_MEMORY_BENCH kind=%u bytes=%zu loops=%zu cpu_ns=%llu\n",kind,n,loops,
                   (unsigned long long)(cpu_ns()-start));
        }
    }
    printf("LINUX_MEMORY_CHECKSUM %u\n",checksum);free(a);free(b);
}
int main(int argc,char **argv) {
    require(__udivti3(81,9)==9,"compiler runtime link");
    if(argc==2 && !strcmp(argv[1],"--benchmark"))benchmark();else semantics();
    return 0;
}
