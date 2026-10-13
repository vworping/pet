from pathlib import Path
import sys,tempfile,threading,unittest,json
from urllib.request import Request,urlopen
from urllib.error import HTTPError
from http.server import ThreadingHTTPServer
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'cards'))
from store import CardStore
from server import make_handler
class API(unittest.TestCase):
 def test_owner_gates_and_stable_public_address(self):
  with tempfile.TemporaryDirectory() as d:
   server=ThreadingHTTPServer(('127.0.0.1',0),make_handler(CardStore(Path(d)/'db'),'test-owner'))
   thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
   def call(path,method='GET',data=None,owner=False):
    headers={'Content-Type':'application/json'}
    if owner:headers['X-PET-Admin']='test-owner'
    req=Request(f'http://127.0.0.1:{server.server_port}'+path,method=method,headers=headers,data=json.dumps(data).encode() if data is not None else None)
    try:
     with urlopen(req,timeout=3) as r:return r.status,json.loads(r.read())
    except HTTPError as e:
     with e:return e.code,json.loads(e.read())
   try:
    self.assertEqual(call('/api/cards')[0],403)
    self.assertEqual(call('/api/cards','POST',{})[0],403)
    status,card=call('/api/cards','POST',{},True);self.assertEqual(status,201)
    path='/api/card/'+card['id'];original=call(path)[1]
    fields={'label':'ATLA / EAR','note':'Our prototype','url':'https://mindofaxd.com'}
    self.assertEqual(call(path,'POST',fields)[0],403);self.assertEqual(call(path)[1],original)
    self.assertEqual(call(path,'POST',fields,True)[0],200)
    updated=call(path)[1];self.assertEqual(updated['label'],'ATLA / EAR');self.assertEqual(updated['id'],card['id']);self.assertEqual(updated['revision'],2)
    self.assertEqual(call(path,'POST',fields,True)[1]['revision'],2)
    fields['url']='javascript:alert(1)';self.assertEqual(call(path,'POST',fields,True)[0],400)
    self.assertEqual(call(path)[1],updated)
    fields['url']='https://mindofaxd.com';fields['connection_note']='Color and human connection'
    self.assertEqual(call(path,'POST',fields)[0],403)
    saved=call(path,'POST',fields,True)[1]
    self.assertEqual(saved['connection_note'],fields['connection_note']);self.assertEqual(saved['id'],card['id'])
    self.assertEqual(call(path)[1],saved);self.assertEqual(call(path,'POST',fields,True)[1],saved)
    fields['connection_note']=False;self.assertEqual(call(path,'POST',fields,True)[0],400)
    self.assertEqual(call(path)[1],saved)
   finally:server.shutdown();server.server_close();thread.join()
if __name__=='__main__':unittest.main()
