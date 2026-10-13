from pathlib import Path
import sys,tempfile,unittest,sqlite3
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'cards'))
from store import CardStore
class Cards(unittest.TestCase):
 def test_connection_note_migrates_and_stays_with_one_card(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'cards.db'
   with sqlite3.connect(p) as db:
    db.execute('CREATE TABLE cards (id TEXT PRIMARY KEY,label TEXT NOT NULL,note TEXT NOT NULL,url TEXT NOT NULL,revision INTEGER NOT NULL DEFAULT 1)')
    db.execute('INSERT INTO cards VALUES (?,?,?,?,?)',('existing','Andre','Bio','https://mindofaxd.com',7))
   s=CardStore(p);before=s.get('existing');other=s.create()
   self.assertEqual(before['connection_note'],'');self.assertEqual(before['revision'],7)
   saved=s.assign('existing','Andre','Bio','https://mindofaxd.com','Color and human connection')
   self.assertEqual(saved['revision'],8);self.assertEqual(saved['note'],'Bio')
   self.assertEqual(CardStore(p).get('existing'),saved);self.assertEqual(s.get(other['id']),other)
   self.assertEqual(s.assign('existing','Andre','Bio','https://mindofaxd.com'),saved)
   self.assertEqual(s.assign('existing','Andre','Bio','https://mindofaxd.com',saved['connection_note']),saved)
   for invalid in ['a'*81,'a\nb',False,3,{}]:
    with self.assertRaises(ValueError):s.assign('existing','Andre','Bio','https://mindofaxd.com',invalid)
   self.assertEqual(s.get('existing'),saved)
   cleared=s.assign('existing','Andre','Bio','https://mindofaxd.com','')
   self.assertEqual(cleared['connection_note'],'');self.assertEqual(cleared['revision'],9)
 def test_persistent_address_assignment_and_idempotency(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'cards.db';s=CardStore(p);first=s.create();other=s.create()
   updated=s.assign(first['id'],'ATLA / EAR','Ask me about our prototype.','https://mindofaxd.com')
   self.assertEqual(first['id'],updated['id']);self.assertEqual(updated['revision'],2)
   self.assertEqual(CardStore(p).get(first['id']),updated)
   self.assertEqual(s.assign(first['id'],updated['label'],updated['note'],updated['url'])['revision'],2)
   self.assertEqual(s.get(other['id']),other)
   for url in ['javascript:alert(1)','http://example.com','https://user:secret@example.com']:
    with self.assertRaises(ValueError):s.assign(first['id'],'Title','Note',url)
   self.assertEqual(s.get(first['id']),updated)
if __name__=='__main__':unittest.main()
