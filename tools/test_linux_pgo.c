/* Toolchain qualification only; this file never enters the game. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t next_value(uint32_t *state) {
    uint32_t x=*state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    return *state=x;
}

__attribute__((noinline))
static uint32_t visit(const uint32_t *values,unsigned count,uint32_t salt) {
    uint32_t sum=salt;
    for(unsigned i=0;i<count;++i) {
        uint32_t x=values[i];
        if((x&31u)==0) sum^=x>>3;
        else if(x&0x80000000u) sum+=(x^salt)*13u;
        else sum=(sum<<1)^(x+0x9e3779b9u);
    }
    return sum;
}

int main(int argc,char **argv) {
    uint32_t seed=argc>1?(uint32_t)strtoul(argv[1],0,10):0x81356ac7u;
    uint32_t values[4096];
    for(unsigned i=0;i<4096;++i) values[i]=next_value(&seed);
    uint32_t sum=0;
    for(unsigned i=0;i<128;++i) {
        values[i*17u%4096u]^=next_value(&seed);
        sum^=visit(values,4096,sum+i);
    }
    printf("LINUX_PGO_PROBE checksum=%08x\n",sum);
    return 0;
}
