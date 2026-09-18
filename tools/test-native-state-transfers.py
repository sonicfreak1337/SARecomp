"""Check bounded state-transfer authoring and emit its standalone C++ oracle test.

This tests branch/delay/PR semantics, not gameplay or an enabled actor feature.
"""
import argparse
import importlib.util
from pathlib import Path
import struct

spec=importlib.util.spec_from_file_location('state_author',Path(__file__).with_name('prepare-render-hierarchy.py'))
author=importlib.util.module_from_spec(spec);spec.loader.exec_module(author)
BASE=author.shared.BASE
ENTRY=BASE+0x1000
TARGETS=(ENTRY-16,ENTRY+0x40,ENTRY+0x80)

def image(op,slot):
    ram=bytearray(0x1000000)
    struct.pack_into('<HH',ram,ENTRY-BASE,op,slot)
    return ram

def reject(op,slot,transfers):
    author.TRANSFERS=transfers
    try:author.inspect(image(op,slot),ENTRY,ENTRY,ENTRY+4)
    except ValueError:return
    raise AssertionError('Unsafe transfer declaration was accepted')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    reject(0x0023,0x7001,{})  # Missing original target evidence.
    reject(0x0003,0x0009,{ENTRY:{ENTRY:TARGETS}})  # BSRF writes PR: different operation.
    reject(0xA01E,0x0009,{ENTRY:{ENTRY:(ENTRY+0x80,)}})  # Stale direct branch.
    reject(0x0023,0x000B,{ENTRY:{ENTRY:TARGETS}})  # Nested transfer in delay slot.
    reject(0x0023,0x0009,{ENTRY:{ENTRY:(ENTRY+1,)}})
    reject(0x0023,0x0009,{ENTRY:{ENTRY:(TARGETS[0],TARGETS[0])}})
    reject(0x0023,0x0009,{ENTRY:{ENTRY:TARGETS,ENTRY+8:TARGETS}})
    source=[r'''
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
struct Cpu {std::array<std::uint32_t,16> r{};std::uint32_t pc{},pr{};};
struct RestartPoint {std::uint32_t pc;};
struct Resume {};
enum class CodeWriteSource {Cpu};
struct Access {Cpu& cpu;void restart(std::uint32_t pc){cpu.pc=pc;throw Resume{};}};
#define HIERARCHY_SITE(pc) ((void)0)
unsigned transferred{},backedges{};
unsigned stores{};std::uint32_t written_address{},written_value{},write_origin{};
void check(bool ok){if(!ok)throw std::runtime_error("state transfer mismatch");}
''']
    fixtures=((0x0023,0x7001,0,TARGETS),  # Target must precede changed index.
              (0x0723,0x717F,7,TARGETS),
              (0xA01E,0x7101,0,(ENTRY+0x40,)),
              (0x0023,0x6122,0,TARGETS),  # Delay load can fault.
              (0x0723,0x0124,7,TARGETS))  # Indexed byte state write.
    for i,(op,slot,reg,targets) in enumerate(fixtures):
        ram=image(op,slot);author.TRANSFERS={ENTRY:{ENTRY:targets}}
        ins,delays,calls=author.inspect(ram,ENTRY,ENTRY,ENTRY+4)
        assert set(ins)=={ENTRY} and delays=={ENTRY+2}
        assert len(calls)==1 and calls[0]['transfer']
        source.append(f'void operation{i}(Cpu& cpu) {{\nAccess a{{cpu}};')
        source.append('const auto backedge=[](std::uint32_t){++backedges;};')
        source.append('const auto load=[&](RestartPoint at,std::uint32_t p){if(p&3u)a.restart(at.pc);return 0x12345678u;};')
        source.append('const auto store8=[](RestartPoint at,std::uint32_t p,std::uint8_t v,CodeWriteSource){++stores;written_address=p;written_value=v;write_origin=at.pc;};')
        source.append('const auto transfer=[&](std::uint32_t target,std::uint32_t site){check(site==0x8C001000u);cpu.pc=target;++transferred;};')
        source.append(author.emit_body(ram,ENTRY,ins)+'}\n')
    source.append(r'''
int main(){unsigned cases=0;
 const std::array<void(*)(Cpu&),5> functions{operation0,operation1,operation2,operation3,operation4};
 for(unsigned kind=0;kind<functions.size();++kind)
 for(std::uint32_t target:{0x8C000FF0u,0x8C001040u,0x8C001080u,0x8C001042u,0x8C001000u})
 for(unsigned fault=0;fault<2;++fault){
  Cpu cpu;for(unsigned i=0;i<16;++i)cpu.r[i]=0xCA000000u+i;
  cpu.pr=0x8CABCDEFu;cpu.pc=0x8C001000u;
  cpu.r[kind==1 || kind==4?7:0]=target-0x8C001004u;cpu.r[2]=fault?3:4;
  Cpu expected=cpu;transferred=backedges=stores=0;
  const auto destination=kind==2?0x8C001040u:target;
  const bool admitted=kind==2 || destination==0x8C000FF0u || destination==0x8C001040u || destination==0x8C001080u;
  const bool completes=admitted && !(kind==3 && fault);
  if(completes){
   if(kind==0)++expected.r[0];
   if(kind==1)expected.r[1]+=127u;
   if(kind==2)++expected.r[1];
   if(kind==3)expected.r[1]=0x12345678u;
   expected.pc=destination;
  }
  bool resumed=false;try{functions[kind](cpu);}catch(const Resume&){resumed=true;}
  check(resumed!=completes);check(transferred==unsigned(completes));
  check(stores==unsigned(kind==4 && completes));
  if(stores)check(written_address==expected.r[0]+expected.r[1] && written_value==std::uint8_t(expected.r[2]) && write_origin==0x8C001000u);
  check(cpu.r==expected.r && cpu.pc==expected.pc && cpu.pr==expected.pr);++cases;
 }
 std::cout<<"SONIC_NATIVE_STATE_TRANSFER_PASS cases="<<cases<<"\n";
}
''')
    author.TRANSFERS={}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text('\n'.join(source)+'\n',newline='\n')
    print('SONIC_NATIVE_STATE_AUTHOR_PASS rejects=7 fixtures=5')

if __name__=='__main__':main()
