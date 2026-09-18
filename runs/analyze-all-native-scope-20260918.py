from pathlib import Path
import json
p=Path('runs/all-native-profile-gamma-20260918/stacks-resolved.json')
s=json.loads(p.read_text()); n=s['total_samples']
for title,roots in [('actors',{'8C0CBD40','8C0FDC20'}),('display',{'8C0CFA0E','8C0FC0A0','8C0FC240'}),('player',{'8C0CBD40'})]:
 rows=[r for r in s['samples'] if set(r['owners'])&roots]
 native=[r for r in rows if r['native']]
 print(title,len(rows),round(100*len(rows)/n,2),'with any native descendant',len(native),'without',len(rows)-len(native))
 print([(x['parent'],x['child'],x['samples']) for x in s['guest_edges'] if x['parent'] in roots][:24])
