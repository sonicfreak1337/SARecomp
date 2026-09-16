"""Authenticate the PAL animation closure; emit identities, never a translated kernel."""
import argparse
import hashlib
from pathlib import Path
import prepare_collision_candidates as shared

RANGES=(
 (0x8C057B00,0x52C,'283b799c5d6ebce1f6b5f5cb2fa1fe45f7f9c7e5c5351bf76a041883b5fd97e4'),
 (0x8C055C8E,0xC,'cb3f7010259f483bcc5f63904940db2f93fe210d81f53480c6c97d7d176ce703'),
 (0x8C639BB0,0x90,'7fa76130c1dab3c4a236f32abe71d20b8af90b84a871a9889ffa22e0584ef735'),
 (0x8C639AD8,0x40,'a3ff7b35d7be9f1ac1dce0209af71beca602344d478d8cec77901ccf7598cc62'),
 (0x8C638ED4,0x2A,'18337194101a0938c69e1e5cc47540e06728773b96fe6ee50174b513a5eb76ce'),
 (0x8C63A744,0x74,'648a2b9b600846afa0e129d8cd4560364c9f6245990921521bcc800d67d46b10'),
 (0x8C63A52C,0xB0,'77c4e4eccc68448a68b938cb6235017ea342bc24f296be4290fbf106033afe99'),
 (0x8C63A7B8,0x12,'99c11afa0f3bd41376d06cfefafb5287f6ec1364e1d3e97fc228c6701860a3bc'),
 (0x8C63A5DC,0x46,'540e05c372efcba07ba4d3e75fc581760458a2628988e0c73598f42dc353bba5'),
 (0x8C639C34,0x86,'edcfcf7558f0a71ee34a6baedb0245b3c7905e65e7ef8cbcd08e09ab16a5c4dd'),
)
POSE_RANGES=(
 (0x8C0417C8,0x1A4,'8f18f0b75f6afb9cf3bfceabafe5bf4dbcccf605e25ec7dba2b629aa9a9179ff'),
 (0x8C041B84,0x10,'622c6967c61ed41b7554b18a3b6715976707de6386bd7f1751fbfc748cae08c8'),
 *RANGES[-3:],
 (0x8C639F38,0x86,'5a3a684da7cdece15ec227b4070f73024c4a0d6d297d44af4f71d38082f35607'),
)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--ram',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--interpreter',type=Path,help='Optional self-contained Linux test reference')
 a=p.parse_args();ram=a.ram.read_bytes()
 if hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:raise ValueError('PAL RAM identity')
 for address,size,sha in (*RANGES,*POSE_RANGES):
  if hashlib.sha256(ram[address-shared.BASE:address-shared.BASE+size]).hexdigest()!=sha:
   raise ValueError(f'Animation closure identity: {address:08X}')
 # Include the RTS delay slot, independently fixed to NOP.
 if ram[0x638EFE:0x638F00]!=b'\x09\x00':raise ValueError('Matrix store return slot')
 spans=sorted((address,size+(2 if address==0x8C638ED4 else 0)) for address,size,_ in RANGES)
 merged=[]
 for address,size in spans:
  if merged and address<=sum(merged[-1]):
   start,length=merged[-1];merged[-1]=(start,max(start+length,address+size)-start)
  else:merged.append((address,size))
 spans=merged
 a.output.mkdir(parents=True,exist_ok=True)
 if a.interpreter:
  # Same separately qualified FTRC oracle correction as motion sampling.
  # Keep Linux component tests independent of a Windows build directory.
  oracle=a.interpreter.read_bytes()
  if hashlib.sha256(oracle).hexdigest()!='f853af46cc865e0c7bd1b5c57be1addad9b00a5f045e6791c440927c0ec64918':
   raise ValueError('Test interpreter identity changed')
  before=b'case Kind::Ftrc: fpu_truncate_to_fpul(cpu, n); return {};'
  after=b'case Kind::Ftrc: fpu_truncate_to_fpul(cpu, m); return {};'
  if oracle.count(before)!=1:raise ValueError('FTRC oracle adaptation mismatch')
  path=a.output/'animation-reference-interpreter.cpp';adapted=oracle.replace(before,after)
  if not path.exists() or path.read_bytes()!=adapted:path.write_bytes(adapted)
 path=a.output/'animation-identities.inc';text=shared.emit_identities(ram,spans)
 if not path.exists() or path.read_text()!=text:path.write_text(text,encoding='ascii',newline='\n')
 path=a.output/'pose-blend-identities.inc';text=shared.emit_identities(ram,sorted((address,size) for address,size,_ in POSE_RANGES))
 if not path.exists() or path.read_text()!=text:path.write_text(text,encoding='ascii',newline='\n')
 print('SONIC_ANIMATION_HIERARCHY_IDENTITIES_READY ranges='+str(len(spans)))
if __name__=='__main__':main()
