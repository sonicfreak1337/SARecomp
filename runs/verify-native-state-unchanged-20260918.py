from pathlib import Path
import hashlib,json
root=Path('runs/native-state-transfers-20260918')
report={}
for kind,source in [('render','render-hierarchy'),('contact','movement-contact')]:
 generated=root/('unchanged-'+kind); existing=Path('build-linux/generated')/source
 rows=[]
 for file in sorted(generated.iterdir()):
  if file.suffix not in ('.inc','.json'):continue
  old=existing/file.name
  if not old.exists():raise ValueError('Missing built artifact '+str(old))
  equal=file.read_bytes()==old.read_bytes()
  rows.append({'file':file.name,'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'identical':equal})
  if not equal:raise ValueError('Existing group changed '+file.name)
 report[kind]=rows
(root/'unchanged-groups.json').write_text(json.dumps(report,indent=2)+'\n')
print('SONIC_EXISTING_GROUPS_IDENTICAL',[(k,len(v)) for k,v in report.items()])
