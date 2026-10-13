#!/usr/bin/env python3
"""Create/reuse a permanent card URL and write it to one explicitly selected tag."""
import argparse,json,os,re,subprocess,sys,time
from pathlib import Path
from urllib.request import Request,urlopen
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--uid',required=True);p.add_argument('--card',help='Reuse a card ID already created in the owner dashboard');p.add_argument('--port');a=p.parse_args()
if not re.fullmatch(r'(?:[0-9A-F]{8}|[0-9A-F]{14}|[0-9A-F]{20})',a.uid):p.error('Use the exact uppercase UID from the reader log.')
if a.card and not re.fullmatch(r'[a-f0-9]{32}',a.card):p.error('Card IDs contain 32 lowercase hexadecimal characters.')
c=json.loads((ROOT/'.local/pet-cloud-credentials.json').read_text());inventory=ROOT/'.local/nfc-inventory.json';items=json.loads(inventory.read_text()) if inventory.exists() else {}
if a.uid in items and a.card and a.card!=items[a.uid]['id']:p.error('This sticker already has a different permanent card address. Review the inventory first.')
def api(path,method='GET'):
 req=Request(c['base']+path,method=method,data=b'{}' if method=='POST' else None,headers={'X-PET-Admin':c['admin_key'],'Content-Type':'application/json','User-Agent':'PET/1.0'})
 with urlopen(req,timeout=20) as r:return json.load(r)
def save():
 temporary=inventory.with_suffix('.tmp');fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
 with os.fdopen(fd,'w') as f:json.dump(items,f,indent=2);f.flush();os.fsync(f.fileno())
 temporary.replace(inventory)
if a.uid not in items:
 card=api('/api/card/'+a.card) if a.card else api('/api/cards','POST')
 items[a.uid]={'id':card['id'],'url':c['base']+'/c/'+card['id'],'verified':False};save()
entry=items[a.uid];api('/api/card/'+entry['id'])
print('Hold only sticker '+a.uid+' against PET until verification finishes.',flush=True)
log=ROOT/'.local'/('nfc-provision-'+a.uid+'-'+str(time.time_ns())+'.log')
cmd=[sys.executable,str(ROOT/'scripts/rfid-monitor.py'),'--uid',a.uid,'--write-url',entry['url'],'--seconds','35','--log',str(log)]
if a.port:cmd+=['--port',a.port]
subprocess.run(cmd,check=True,cwd=ROOT)
if ('NDEF URL VERIFIED UID='+a.uid+' URL='+entry['url']) not in log.read_text(errors='replace'):raise SystemExit('Write not confirmed. Keep the same inventory entry and retry; no new card address is needed.')
entry['verified']=True;entry['verified_at']=int(time.time());save();print('Verified permanent NFC address: '+entry['url'])
