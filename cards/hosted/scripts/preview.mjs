import {Miniflare} from 'miniflare';
import {readFile,readdir,mkdir} from 'node:fs/promises';
await mkdir('.local',{recursive:true});
const mf=new Miniflare({modules:true,scriptPath:'dist/server/index.js',compatibilityDate:'2025-09-27',host:'127.0.0.1',port:8777,d1Databases:['DB'],d1Persist:'.local/preview-db',bindings:{PET_ADMIN_KEY:'local-preview-owner-key-00000000000000',PET_DEVICE_KEY:'local-preview-device-key-0000000000000'}});
const db=await mf.getD1Database('DB');await db.prepare('CREATE TABLE IF NOT EXISTS _local_migrations(name TEXT PRIMARY KEY)').run();
for(const f of (await readdir('drizzle')).filter(x=>x.endsWith('.sql')).sort())if(!await db.prepare('SELECT name FROM _local_migrations WHERE name=?').bind(f).first()){for(const s of (await readFile('drizzle/'+f,'utf8')).split('--> statement-breakpoint'))if(s.trim())await db.prepare(s).run();await db.prepare('INSERT INTO _local_migrations VALUES(?)').bind(f).run();}
console.log('PET preview ready: '+await mf.ready);
