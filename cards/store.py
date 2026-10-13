"""Persistent card addresses, destinations and public conversation notes."""
from contextlib import contextmanager
import secrets
import sqlite3
from urllib.parse import urlsplit

class CardStore:
    def __init__(self, path):
        self.path = str(path)
        with self.connect() as db:
            db.execute('CREATE TABLE IF NOT EXISTS cards (id TEXT PRIMARY KEY, label TEXT NOT NULL, note TEXT NOT NULL, url TEXT NOT NULL, revision INTEGER NOT NULL DEFAULT 1)')
            if 'connection_note' not in {row['name'] for row in db.execute('PRAGMA table_info(cards)')}:
                db.execute("ALTER TABLE cards ADD COLUMN connection_note TEXT NOT NULL DEFAULT ''")
    @contextmanager
    def connect(self):
        db = sqlite3.connect(self.path)
        db.row_factory = sqlite3.Row
        try:
            with db: yield db
        finally: db.close()
    def create(self):
        ident = secrets.token_hex(12)
        with self.connect() as db:
            db.execute('INSERT INTO cards (id,label,note,url,revision) VALUES (?,?,?,?,1)', (ident,'Andre Weiss','Photographer × researcher','https://mindofaxd.com'))
        return self.get(ident)
    def get(self, ident):
        with self.connect() as db:
            row = db.execute('SELECT * FROM cards WHERE id=?',(ident,)).fetchone()
            return dict(row) if row else None
    def all(self):
        with self.connect() as db:
            return [dict(r) for r in db.execute('SELECT * FROM cards ORDER BY rowid DESC')]
    def assign(self, ident, label, note, url, connection_note=None):
        parts = urlsplit(url)
        if not parts.hostname or parts.scheme != 'https' or parts.username or parts.password:
            raise ValueError('Use a public HTTPS destination without embedded credentials.')
        if not label.strip() or len(label)>80 or len(note)>280 or len(url)>2048 or any(ord(c)<32 for c in label+note+url):
            raise ValueError('Check the label, note and destination length.')
        if connection_note is not None:
            if not isinstance(connection_note,str) or len(connection_note)>80 or any(ord(c)<32 for c in connection_note):
                raise ValueError('How we connected must be text, at most 80 characters.')
            connection_note=connection_note.strip()
        with self.connect() as db:
            row = db.execute('SELECT * FROM cards WHERE id=?',(ident,)).fetchone()
            if not row: raise KeyError('Card not found')
            if connection_note is None: connection_note=row['connection_note']
            if (row['label'],row['note'],row['url'],row['connection_note']) != (label,note,url,connection_note):
                db.execute('UPDATE cards SET label=?,note=?,url=?,connection_note=?,revision=revision+1 WHERE id=?',(label,note,url,connection_note,ident))
        return self.get(ident)
