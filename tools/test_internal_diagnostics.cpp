#include "sonic_internal_diagnostics.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const auto root=std::filesystem::absolute(argv[1]);
    if(std::filesystem::exists(root))return 2;
    std::filesystem::create_directories(root);
    const auto program=root/"game";
    const auto policy=root/sonic::diagnostics::internal_policy_name;
    const auto require=[](bool ok){if(!ok)throw std::runtime_error("diagnostic-policy-test");};
    const auto write=[&](std::string_view value){std::ofstream out(policy,std::ios::binary);out<<value;};
    using sonic::diagnostics::read_internal_policy;
    require(!read_internal_policy(program,nullptr));
    write(sonic::diagnostics::internal_policy_on);
    require(read_internal_policy(program,nullptr));
    require(!read_internal_policy(program,"0"));
    write(sonic::diagnostics::internal_policy_off);
    require(!read_internal_policy(program,nullptr));
    require(read_internal_policy(program,"1"));
    for(const auto invalid:{"on", "SARECOMP-DIAGNOSTICS-2\non\n", "SARECOMP-DIAGNOSTICS-1\non\nextra"}){
        write(invalid);require(!read_internal_policy(program,nullptr));
    }
    require(!read_internal_policy(program,"invalid"));
    std::filesystem::remove(policy);std::filesystem::remove(root);
    std::cout<<"SONIC_INTERNAL_POLICY_TESTS passed=9\n";
}
