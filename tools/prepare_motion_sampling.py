"""Author complete original motion owners; SDK register-only leaves stay explicit."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import prepare_collision_candidates as shared

OWNERS=(
 ('position',0x8C0400A0,0x58,'e030d045a21abfba6378dbed0b98214f6897dfb0c755aedb7e3158e61a94e819'),
 ('scale',0x8C0400F8,0x58,'428bb930950b18bfd5d35b714a63b8c6d1ddcb4fd7107170935aa6ed12e203a3'),
 ('rotate-zyx',0x8C040150,0x58,'2b3e88bb227cafbe94a2c676b80c6b68f976992ead98a923219afd38e770d4b7'),
 ('rotate-yxz',0x8C0401A8,0x58,'989583a5b58daa5bd5b50eea6b8a326414246a6fa259b28dfedfe7b8d7eb1222'),
 ('key-index',0x8C03FEB8,0x3A,'30acd547629a38163256792165f65b9f8821244c4337702f622a56899f38e7bf'),
 ('float-key',0x8C03FF2C,0x64,'a9bdcdb5f3f0e6571835202d8f792386551b98b2e3e87fcf11b7822a37d0c4a7'),
 ('angle-key',0x8C03FF90,0x80,'28e9dbccd008f14942cbb0b238b24de6d8864295257c59d69cab2823b5746a73'),
)
SDK=(
 (0x8C63A7B8,0x12,'99c11afa0f3bd41376d06cfefafb5287f6ec1364e1d3e97fc228c6701860a3bc'),
 (0x8C63A5DC,0x46,'540e05c372efcba07ba4d3e75fc581760458a2628988e0c73598f42dc353bba5'),
 (0x8C639C34,0x86,'edcfcf7558f0a71ee34a6baedb0245b3c7905e65e7ef8cbcd08e09ab16a5c4dd'),
 (0x8C639F38,0x86,'5a3a684da7cdece15ec227b4070f73024c4a0d6d297d44af4f71d38082f35607'),
)
RANGES=sorted([(a,n) for _,a,n,_ in OWNERS]+[(a,n) for a,n,_ in SDK]+
              [(0x8C04007C,4),(0x8C040384,0x14)])

def emit(pc,op,ram):
 n,m=(op>>8)&15,(op>>4)&15
 a,b=f'cpu.r[{n}]',f'cpu.r[{m}]'
 if op&0xF00F==0x000E:return f'{a}=load(cpu.r[0]+{b});'
 if op&0xF00F==0x3002:return f'set_t({a}>={b});'
 if op&0xF00F==0x3006:return f'set_t({a}>{b});'
 if op&0xF00F==0x3008:return f'{a}-={b};'
 if op&0xF0FF==0x4000:return f'set_t(({a}&0x80000000u)!=0u);{a}<<=1u;'
 if op&0xF0FF==0x4001:return f'set_t(({a}&1u)!=0u);{a}>>=1u;'
 if op&0xF0FF==0x4008:return f'{a}<<=2u;'
 if op&0xF0FF==0xF07D:return f'fpu_reciprocal_square_root(cpu,{n}u);'
 return shared.emit_simple(pc,op,ram)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--ram',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--interpreter',type=Path,required=True)
 args=p.parse_args();ram=args.ram.read_bytes()
 if len(ram)!=0x1000000 or hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:raise ValueError('Original RAM mismatch')
 for a,n,sha in [(a,n,sha) for _,a,n,sha in OWNERS]+list(SDK):
  if hashlib.sha256(ram[a-shared.BASE:a-shared.BASE+n]).hexdigest()!=sha:raise ValueError(f'Source mismatch {a:08X}')
 args.output.mkdir(parents=True,exist_ok=True);reports=[]
 # The pinned test interpreter uses decoded destination n for FTRC, but the
 # decoder represents FRn as source m and FPUL as destination 0. Correct only
 # this separately generated oracle; never edit the pinned SDK or game pack.
 oracle=args.interpreter.read_bytes()
 if hashlib.sha256(oracle).hexdigest()!='f853af46cc865e0c7bd1b5c57be1addad9b00a5f045e6791c440927c0ec64918':
  raise ValueError('Test interpreter source changed; review FTRC adaptation')
 before=b'case Kind::Ftrc: fpu_truncate_to_fpul(cpu, n); return {};'
 after=b'case Kind::Ftrc: fpu_truncate_to_fpul(cpu, m); return {};'
 if oracle.count(before)!=1:raise ValueError('FTRC oracle adaptation mismatch')
 (args.output/'motion-reference-interpreter.cpp').write_bytes(oracle.replace(before,after))
 for name,a,n,sha in OWNERS:
  instructions,delays,calls=shared.inspect(ram,a,a+n)
  used=dict(instructions);used.update({pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0] for pc in delays})
  literals=set()
  for pc,op in used.items():
   if op>>12==0xD:literals.add((((pc+4)&~3)+(op&255)*4,4))
   if op>>12==9:literals.add((pc+4+(op&255)*2,2))
   if op>>8==0xC7:literals.add((((pc+4)&~3)+(op&255)*4,4))
  for address,size in literals:
   if not any(base<=address and address+size<=base+length for base,length in RANGES):
    raise ValueError(f'Uncovered literal {address:08X}')
  body=shared.emit_body(ram,instructions,emit).replace('PAL TOUCH-POLY owner',f'PAL motion {name} owner')
  (args.output/(name+'-body.inc')).write_text(body,encoding='ascii',newline='\n')
  reports.append({'name':name,'entry':f'{a:08X}','size':n,'sha256':sha,
    'instructions':len(instructions),'delays':len(delays),'calls':calls})
 (args.output/'motion-identities.inc').write_text(shared.emit_identities(ram,RANGES),encoding='ascii',newline='\n')
 (args.output/'motion-inventory.json').write_text(json.dumps({'schema':'sarecomp-motion-sampling-v1',
  'ram_sha256':shared.RAM_SHA,'owners':reports,'source_spans':[{'address':f'{a:08X}','size':n,
  'sha256':hashlib.sha256(ram[a-shared.BASE:a-shared.BASE+n]).hexdigest()} for a,n in RANGES]},indent=2)+'\n',encoding='ascii',newline='\n')
 print('SONIC_MOTION_SAMPLING_READY owners='+str(len(reports)))
if __name__=='__main__':main()
