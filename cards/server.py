"""Local card assignment bench. Production hosting and NFC writing are separate."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
import argparse
import json
import secrets
import os
from store import CardStore

ROOT=Path(__file__).resolve().parent

def make_handler(store, admin_key, public_base=''):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def respond(self,status,content,kind='application/json'):
            if not isinstance(content,bytes): content=json.dumps(content).encode()
            self.send_response(status)
            self.send_header('Content-Type',kind)
            self.send_header('Content-Length',str(len(content)))
            self.send_header('Cache-Control','no-store')
            self.send_header('X-Content-Type-Options','nosniff')
            self.send_header('Referrer-Policy','no-referrer')
            self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
            self.end_headers();self.wfile.write(content)
        def authorized(self):
            return secrets.compare_digest(self.headers.get('X-PET-Admin',''),admin_key)
        def do_GET(self):
            path=urlsplit(self.path).path
            if path in ('/','/admin'):
                return self.respond(200,(ROOT/'admin.html').read_bytes(),'text/html; charset=utf-8')
            if path.startswith('/c/'):
                return self.respond(200,(ROOT/'card.html').read_bytes(),'text/html; charset=utf-8') if store.get(path[3:]) else self.respond(404,{'error':'Card not found'})
            if path=='/api/cards':
                if not self.authorized(): return self.respond(403,{'error':'Owner access required'})
                return self.respond(200,{'cards':store.all(),'publicBase':public_base,'physicalBadgeConnected':False})
            if path.startswith('/api/card/'):
                card=store.get(path[len('/api/card/'):])
                return self.respond(200,card) if card else self.respond(404,{'error':'Card not found'})
            return self.respond(404,{'error':'Not found'})
        def do_POST(self):
            if not self.authorized(): return self.respond(403,{'error':'Owner access required'})
            path=urlsplit(self.path).path
            try:
                length=int(self.headers.get('Content-Length','0'))
                if length<0 or length>4096: return self.respond(413,{'error':'Request too large'})
                raw=self.rfile.read(length)
                if path=='/api/cards': return self.respond(201,store.create())
                if path.startswith('/api/card/'):
                    j=json.loads(raw)
                    if not isinstance(j,dict) or any(not isinstance(j.get(k),str) for k in ('label','note','url')): raise ValueError('Enter a label, note and HTTPS URL.')
                    if 'connection_note' in j and not isinstance(j['connection_note'],str): raise ValueError('How we connected must be text.')
                    return self.respond(200,store.assign(path[len('/api/card/'):],j['label'],j['note'],j['url'],j.get('connection_note')))
            except (ValueError,KeyError) as e: return self.respond(400,{'error':str(e)})
            return self.respond(404,{'error':'Not found'})
    return Handler

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--port',type=int,default=8767)
    parser.add_argument('--public-base',default='',help='Final HTTPS host for permanent NFC links; not a deployment command')
    args=parser.parse_args()
    base=urlsplit(args.public_base)
    if args.public_base and (base.scheme!='https' or not base.hostname or base.username or base.password or base.query or base.fragment):parser.error('Public base must be an HTTPS address without credentials, query or fragment')
    data=ROOT.parent/'.local';data.mkdir(exist_ok=True)
    keyfile=data/'card-owner.key'
    if not keyfile.exists():
        fd=os.open(keyfile,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
        with os.fdopen(fd,'w') as f:f.write(secrets.token_urlsafe(32))
    key=keyfile.read_text().strip()
    owner_url=data/'card-owner-url.txt'
    fd=os.open(owner_url,os.O_CREAT|os.O_TRUNC|os.O_WRONLY,0o600)
    os.fchmod(fd,0o600)
    with os.fdopen(fd,'w') as f:f.write(f'http://127.0.0.1:{args.port}/admin#{key}\n')
    server=ThreadingHTTPServer(('127.0.0.1',args.port),make_handler(CardStore(data/'cards.sqlite3'),key,args.public_base.rstrip('/')))
    print(f'Card bench on 127.0.0.1:{args.port}. Private owner URL saved in .local/card-owner-url.txt.',flush=True)
    server.serve_forever()
