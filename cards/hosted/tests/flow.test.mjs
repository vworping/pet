import {test,after,before} from 'node:test';
import assert from 'node:assert/strict';
import {Miniflare} from 'miniflare';
import {readFile,readdir} from 'node:fs/promises';
const admin='a'.repeat(48),device='d'.repeat(48);let mf,db;
before(async()=>{mf=new Miniflare({modules:true,scriptPath:'dist/server/index.js',compatibilityDate:'2025-09-27',d1Databases:['DB'],bindings:{PET_ADMIN_KEY:admin,PET_DEVICE_KEY:device}});db=await mf.getD1Database('DB');for(const file of (await readdir('drizzle')).filter(f=>f.endsWith('.sql')).sort())for(const sql of (await readFile('drizzle/'+file,'utf8')).split('--> statement-breakpoint'))if(sql.trim())await db.prepare(sql).run();});
after(async()=>{await mf?.dispose();});
async function call(path,method='GET',data,role){const headers={'Content-Type':'application/json'};if(role==='admin')headers['X-PET-Admin']=admin;if(role==='device')headers['X-PET-Device']=device;const r=await mf.dispatchFetch('https://pet.test'+path,{method,headers,body:data===undefined?undefined:JSON.stringify(data)});const body=await r.text();return {status:r.status,headers:r.headers,body:body.startsWith('{')?JSON.parse(body):body};}
async function encounter(){return (await call('/api/admin/encounters','POST',{},'admin')).body;}
async function card(){return (await call('/api/cards','POST',{},'admin')).body;}
const intro={name:'Zoë García',topic:'Color and human connection',consent:true,share_topic:true};
test('private lists and mutations require role-specific authorization',async()=>{for(const path of ['/api/cards','/api/admin/encounters','/api/admin/export'])assert.equal((await call(path)).status,403);assert.equal((await call('/api/device/encounters','POST',{id:'1'.repeat(32)},'admin')).status,403);assert.equal((await call('/api/cards','POST',{},'device')).status,403);});
test('Hello and assignment retries are idempotent; public page excludes private data',async()=>{const e=await encounter(),c=await card();assert.equal((await call('/h/'+e.id)).status,200);await call('/api/hello/'+e.id+'/opened','POST');assert.equal((await call('/api/hello/'+e.id,'POST',intro)).status,200);assert.equal((await call('/api/hello/'+e.id,'POST',intro)).status,200);assert.equal((await call('/api/hello/'+e.id,'POST',{...intro,name:'Different'})).status,409);const poll=await call('/api/device/encounters/'+e.id,'GET',undefined,'device');assert.equal(poll.body.name,intro.name);assert.equal(poll.body.opened,true);const a={event_id:'2'.repeat(32),card_id:c.id,encounter_id:e.id};assert.equal((await call('/api/device/assign','POST',a,'device')).status,200);assert.equal((await call('/api/device/assign','POST',a,'device')).status,200);const r=await call('/api/card/'+c.id);assert.equal(r.body.revision,2);assert.equal(r.body.connection_note,intro.topic);for(const k of ['name','encounter_id','submitted_at','consent_version'])assert.equal(r.body[k],undefined);assert.match(r.headers.get('X-Robots-Tag'),/noindex/);const exported=await call('/api/admin/export','GET',undefined,'admin');assert.equal(exported.body.encounters.find(x=>x.id===e.id).consent_version,'2026-09-29');});
test('unshared topic stays private and used card rejects replacement',async()=>{const e=await encounter(),c=await card();await call('/api/hello/'+e.id,'POST',{...intro,share_topic:false});const a={event_id:'3'.repeat(32),card_id:c.id,encounter_id:e.id};await call('/api/device/assign','POST',a,'device');assert.equal((await call('/api/card/'+c.id)).body.connection_note,'');const other=await encounter();await call('/api/hello/'+other.id,'POST',intro);assert.equal((await call('/api/device/assign','POST',{...a,event_id:'4'.repeat(32),encounter_id:other.id},'device')).status,409);});
test('expiry and cancellation prevent late submission; retries do not extend expiry',async()=>{const id='5'.repeat(32);const a=await call('/api/device/encounters','POST',{id},'device'),b=await call('/api/device/encounters','POST',{id},'device');assert.equal(a.body.expires_at,b.body.expires_at);await db.prepare('UPDATE encounters SET expires_at=1 WHERE id=?').bind(id).run();assert.equal((await call('/api/hello/'+id,'POST',intro)).status,410);const e=await encounter();await call('/api/device/encounters/'+e.id+'/close','POST',{},'device');assert.equal((await call('/api/hello/'+e.id,'POST',intro)).status,410);});
test('racing submissions accept only one introduction',async()=>{const e=await encounter();const r=await Promise.all([call('/api/hello/'+e.id,'POST',intro),call('/api/hello/'+e.id,'POST',{...intro,name:'Someone else'})]);assert.deepEqual(r.map(x=>x.status).sort(),[200,409]);});
test('racing card assignments commit once',async()=>{const c=await card(),a=await encounter(),b=await encounter();await call('/api/hello/'+a.id,'POST',intro);await call('/api/hello/'+b.id,'POST',intro);const r=await Promise.all([a,b].map((e,i)=>call('/api/device/assign','POST',{card_id:c.id,encounter_id:e.id,event_id:String(i+6).repeat(32)},'device')));assert.deepEqual(r.map(x=>x.status).sort(),[200,409]);assert.equal((await call('/api/card/'+c.id)).body.revision,2);});
test('deletion removes private record and shared topic',async()=>{const e=await encounter(),c=await card();await call('/api/hello/'+e.id,'POST',intro);await call('/api/admin/assign','POST',{event_id:'8'.repeat(32),card_id:c.id,encounter_id:e.id},'admin');assert.equal((await call('/api/admin/encounters/'+e.id,'DELETE',undefined,'admin')).status,200);assert.equal((await call('/api/card/'+c.id)).body.connection_note,'');assert.equal((await call('/api/device/encounters/'+e.id,'GET',undefined,'device')).status,410);});
test('validation rejects missing consent, unsupported text, large bodies, unsafe URLs',async()=>{const e=await encounter();for(const j of [{...intro,consent:false},{...intro,name:'😄'},{...intro,topic:'x'.repeat(81)}])assert.equal((await call('/api/hello/'+e.id,'POST',j)).status,400);assert.equal((await call('/api/hello/'+e.id,'POST',{...intro,topic:'x'.repeat(5000)})).status,413);const c=await card();assert.equal((await call('/api/card/'+c.id,'POST',{label:'test',note:'',connection_note:'',url:'javascript:alert(1)'},'admin')).status,400);});

test('recipient names remain private by default; owner can identify linked cards',async()=>{
 const e=await encounter(),c=await card();
 await call('/api/hello/'+e.id,'POST',intro);
 await call('/api/admin/assign','POST',{event_id:'9'.repeat(32),card_id:c.id,encounter_id:e.id},'admin');
 assert.equal((await call('/api/card/'+c.id)).body.recipient_name,'');
 const cards=(await call('/api/cards','GET',undefined,'admin')).body.cards;
 const linked=cards.find(x=>x.id===c.id);
 assert.equal(linked.recipient_name,intro.name);assert.equal(linked.encounter_id,e.id);
 assert.equal(linked.introduction_topic,intro.topic);
});
test('name sharing is independent of topic sharing and is cleared on deletion',async()=>{
 const e=await encounter(),c=await card();const data={...intro,share_topic:false,share_name:true};
 assert.equal((await call('/api/hello/'+e.id,'POST',data)).status,200);
 assert.equal((await call('/api/hello/'+e.id,'POST',data)).status,200);
 assert.equal((await call('/api/hello/'+e.id,'POST',{...data,share_name:false})).status,409);
 await call('/api/device/assign','POST',{event_id:'a'.repeat(32),card_id:c.id,encounter_id:e.id},'device');
 const shared=(await call('/api/card/'+c.id)).body;
 assert.equal(shared.recipient_name,intro.name);assert.equal(shared.connection_note,'');
 for(const k of ['name','topic','encounter_id','submitted_at','consent_version'])assert.equal(shared[k],undefined);
 await call('/api/card/'+c.id,'POST',{...shared,note:'Updated note'},'admin');
 assert.equal((await call('/api/card/'+c.id)).body.recipient_name,intro.name);
 await call('/api/admin/encounters/'+e.id,'DELETE',undefined,'admin');
 const deleted=(await call('/api/card/'+c.id)).body;
 assert.equal(deleted.recipient_name,'');assert.ok(deleted.revision>shared.revision);
});
test('invalid name-sharing consent is rejected',async()=>{
 const e=await encounter();
 assert.equal((await call('/api/hello/'+e.id,'POST',{...intro,share_name:'yes'})).status,400);
});
test('badge count follows saved introductions and deletion',async()=>{
 const stats=()=>call('/api/device/stats','GET',undefined,'device');
 assert.equal((await call('/api/device/stats')).status,403);
 assert.equal((await call('/api/device/stats','GET',undefined,'admin')).status,403);
 const before=(await stats()).body.connections;
 const e=await encounter();
 assert.equal((await stats()).body.connections,before);
 await call('/api/hello/'+e.id,'POST',intro);
 assert.equal((await stats()).body.connections,before+1);
 await call('/api/admin/encounters/'+e.id,'DELETE',undefined,'admin');
 assert.equal((await stats()).body.connections,before);
 await db.prepare('DELETE FROM assignments').run();
 await db.prepare('DELETE FROM encounters').run();
 assert.deepEqual((await stats()).body,{connections:0});
});
