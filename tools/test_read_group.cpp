#include "sonic_read_test_fixture.hpp"
#include "sonic_read_group.hpp"

namespace {
struct Operation { unsigned source, destination; bool fp, indexed; };
constexpr std::array operations = {
    Operation{3,4,false,false}, Operation{4,6,true,false},
    Operation{4,3,false,false}, Operation{3,5,true,true}
};

// Original per-instruction attempt, read, fault and safepoint envelopes. The
// candidate changes only the all-admitted path; every rejected group runs here.
void run(Fixture& f, bool candidate, unsigned entry = 0) {
    auto& cpu=f.cpu;
    NativeAotRegisterFile<0x0000FFFFu,0x00000038u> registers(cpu);
    auto guard=cpu.memory.direct_linear_memory_guard(false);
    const auto translate=[&](std::uint32_t address,std::uint32_t& direct) noexcept {
        const auto segment=address>>29u;
        if(segment==4u || segment==5u) {
            if(!cpu.privileged_mode_inline()) return false;
            direct=address;
        } else {
            if((cpu.mmucr&1u)!=0u || segment>=7u || (!cpu.privileged_mode_inline() && address>=0x80000000u)) return false;
            direct=canonical_physical_address_inline(address)|0x80000000u;
        }
        return true;
    };
    const auto can_read=[&](std::uint32_t address) {
        std::uint32_t direct=0,offset=0;
        return translate(address,direct) && direct_linear_guard_offset(guard,direct,4,offset);
    };
    const auto original_read=[&](const GuestInstructionOrigin& origin,std::uint32_t address) {
        std::uint32_t value=0,direct=0;
        if(translate(address,direct) && direct_linear_guard_read_u32(guard,direct,value)) return value;
        ++f.fallbacks;
        registers.flush_release();
        const auto result=guest_read_u32_at(cpu,origin,address);
        registers.reload_acquire();
        return result;
    };
    if(candidate && entry==0) {
        sonic::memory::ReadGroup32 group(cpu,guard,true);
        std::uint32_t a=0,b=0,c=0,d=0;
        if(group.read(registers[3],a) && group.read(a,b) && group.read(a,c) &&
           group.read(c+registers[0],d) && group.commit(4)) {
            registers[4]=a;cpu.fr[6]=b;registers[3]=c;cpu.fr[5]=d;
            sonic::memory::complete_read_group(cpu,f.runtime_pc+6,4);
            f.preflight=true;
            return;
        }
    }
    for(unsigned index=entry;index<operations.size();++index) {
        const auto op=operations[index];
        const auto source=source_pc+index*2u, runtime=f.runtime_pc+index*2u;
        const auto address=[&] {return registers[op.source]+(op.indexed?registers[0]:0u);};
        const bool admitted=can_read(address()) && (!op.fp || !(cpu.fpscr&fpscr_sz_mask) || can_read(address()+4u));
        if(!admitted) {registers.flush_release();flush_pending_guest_cycles(cpu,f.services);registers.reload_acquire();}
        const auto epoch=admitted?0:cpu.memory.mmio_boundary_epoch();
        ExplicitGuestInstructionAttempt attempt(cpu,runtime,2);
        if(op.fp && (cpu.sr&sr_fd_mask)) {registers.flush_release();raise_fpu_disabled(cpu,runtime);return;}
        if(op.fp && (cpu.fpscr&fpscr_pr_mask) && (cpu.fpscr&fpscr_sz_mask)) {registers.flush_release();raise_illegal_instruction(cpu,runtime);return;}
        try {
            const GuestInstructionOrigin origin{source,runtime,true};
            const auto at=address();
            if(op.fp && (cpu.fpscr&fpscr_sz_mask)) {
                const auto low=original_read(origin,at), high=original_read(origin,at+4u);
                write_fpu_pair_bits(cpu,op.destination,(std::uint64_t(high)<<32)|low);
            } else if(op.fp) cpu.fr[op.destination]=original_read(origin,at);
            else registers[op.destination]=original_read(origin,at);
            attempt.complete();
        } catch(const MemoryAccessError& e) {
            if(op.fp) registers.flush_release();
            f.fault=e.reason();const auto o=e.instruction();
            f.error={unsigned(e.reason()),unsigned(e.operation()),e.address(),unsigned(e.width()),o.source_pc,o.runtime_pc,o.valid};
            const auto opcode=(op.fp?0xF000u:0x6000u)|(op.destination<<8)|(op.source<<4)|(op.fp?(op.indexed?6:8):2);
            enter_memory_exception_with_provenance(cpu,e,runtime,opcode);return;
        }
        if(!admitted && cpu.memory.mmio_boundary_epoch()!=epoch) {
            registers.flush_release();cpu.pc=runtime+2;
            if(finalize_guest_block(cpu,f.services,1024,runtime,0,false,false,false).interrupt.has_value())return;
            registers.reload_acquire();
        }
    }
}

unsigned cases=0;
void compare(const char* name,const Action& arrange={},unsigned entry=0,std::optional<bool> expected={}) {
    try {
        Fixture original,candidate;
        for(auto* f:{&original,&candidate}) {
            f->cpu.r[0]=4;
            f->ram->write_u32(0x100,0x8C000200);
            f->ram->write_u32(0x200,0x8C000300);
            f->ram->write_u32(0x204,0x12345678);
            f->ram->write_u32(0x300,0x01234567);
            f->ram->write_u32(0x304,0xF2345678);
            if(arrange)arrange(*f);
            f->cpu.memory.reset_performance_counters();
        }
        run(original,false,entry);run(candidate,true,entry);
        require(state(original.cpu)==state(candidate.cpu),"CPU state mismatch");
        require(provenance(original.cpu)==provenance(candidate.cpu),"fault provenance mismatch");
        require(original.cpu.address_space->snapshot()==candidate.cpu.address_space->snapshot(),"MMU state mismatch");
        require(counts(original.cpu.memory)==counts(candidate.cpu.memory),"access counters mismatch");
        require(original.log==candidate.log && !candidate.log.overflow,"observer order mismatch");
        require(original.error==candidate.error && original.fault==candidate.fault,"fault mismatch");
        require(original.fallbacks==candidate.fallbacks,"fallback reads mismatch");
        require(original.services.clock==candidate.services.clock && original.services.polls==candidate.services.polls,"scheduler mismatch");
        require(original.cpu.memory.mmio_boundary_epoch()==candidate.cpu.memory.mmio_boundary_epoch(),"MMIO boundary mismatch");
        require(original.cpu.memory.mmio_access_epoch()==candidate.cpu.memory.mmio_access_epoch(),"MMIO access mismatch");
        require(std::equal(original.ram->bytes().begin(),original.ram->bytes().end(),candidate.ram->bytes().begin()),"RAM mismatch");
        if(expected)require(candidate.preflight==*expected,"group admission mismatch");
        ++cases;
    } catch(const std::exception& e) {throw std::runtime_error(std::string(name)+": "+e.what());}
}
}

int main() {
    try {
        for(auto alias:{0x0C000100u,0x8C000100u,0xAC000100u,0x0C010100u})
            compare("RAM aliases",[=](auto& f){f.cpu.r[3]=alias;},0,true);
        for(unsigned fault_index=0;fault_index<3;++fault_index)for(auto address:{0x8C020000u,0x8C000101u,0xFFFFFFFFu})
            compare("fault after partial progress",[=](auto& f){
                if(fault_index==0)f.cpu.r[3]=address;
                else f.ram->write_u32(fault_index==1?0x100:0x200,address);
            },0,false);
        for(bool fd:{false,true})for(bool pr:{false,true})for(bool sz:{false,true})for(bool bank:{false,true})
            compare("FP modes",[=](auto& f){f.cpu.write_sr(sr_md_mask|(fd?sr_fd_mask:0));f.cpu.write_fpscr((pr?fpscr_pr_mask:0)|(sz?fpscr_sz_mask:0)|(bank?fpscr_fr_mask:0));},0,!fd&&!sz);
        for(bool privileged:{false,true})for(bool mmu:{false,true})for(auto address:{0x0C000100u,0x8C000100u})
            compare("MMU privilege",[=](auto& f){f.cpu.write_sr(privileged?sr_md_mask:0);f.cpu.mmucr=mmu;f.cpu.address_space->set_mode(mmu?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);f.cpu.address_space->write_mmucr(f.cpu.mmucr);f.cpu.r[3]=address;});
        compare("read watcher",[](auto& f){f.watch(MemoryWatchpointAccess::Read);},0,false);
        compare("write-only watcher",[](auto& f){f.watch(MemoryWatchpointAccess::Write);},0,false);
        compare("trace",[](auto& f){f.trace();},0,false);
        compare("sink",[](auto& f){f.sink();},0,false);
        compare("MMIO tracking",[](auto& f){f.cpu.memory.set_mmio_access_tracking(true);},0,false);
        compare("reference lookup",[](auto& f){f.cpu.memory.set_lookup_mode(MemoryLookupMode::Reference);},0,false);
        compare("relocated metadata",[](auto& f){f.runtime_pc=0x8C12947C;f.cpu.active_block_virtual_start=0x8C129400;},0,true);
        compare("block metadata fallback",[](auto& f){f.cpu.active_block_size=0;},0,true);
        for(bool rejected:{false,true})
            compare("MMIO middle",[=](auto& f){f.mmio(rejected);f.cpu.r[3]=0x8C000100;f.ram->write_u32(0x100,0xB0000000);},0,false);
        compare("flush changes subsequent address and FPU",[](auto& f){
            f.cpu.r[3]=0x8C020000;
            f.services.on_flush=[&f]{f.cpu.r[3]=0x8C000100;f.cpu.write_fpscr(fpscr_sz_mask);};
        },0,false);
        for(unsigned entry=1;entry<4;++entry)compare("resume inside group",{},entry,false);
        compare("unsigned indexed wrap",[](auto& f){f.ram->write_u32(0x200,0xFFFFFFFC);f.cpu.r[0]=0x8C000308;},0,true);
        std::cout<<"SONIC_READ_GROUP_OK cases="<<cases<<" partial_faults=exact observers=ordered counters=exact resume=original\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
