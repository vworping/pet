import pages from './pages.js';
const ID=/^[a-f0-9]{32}$/;
const consentVersion='2026-09-29';
const headers={'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','X-Robots-Tag':'noindex, nofollow, noarchive','Referrer-Policy':'no-referrer','Content-Security-Policy':"default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'"};
const response=(data,status=200,type='application/json')=>new Response(type==='application/json'?JSON.stringify(data):data,{status,headers:{...headers,'Content-Type':type}});
const fail=(status,message)=>{throw Object.assign(new Error(message),{status});};
const randomId=()=>[...crypto.getRandomValues(new Uint8Array(16))].map(b=>b.toString(16).padStart(2,'0')).join('');
async function authorized(request,secret,header){
 if(!secret||secret.length<32)return false;
 const candidate=request.headers.get(header)||'';
 if(candidate.length>256)return false;
 const hash=async s=>new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(s)));
 const [a,b]=await Promise.all([hash(candidate),hash(secret)]);let diff=0;for(let i=0;i<a.length;i++)diff|=a[i]^b[i];return diff===0;
}
async function json(request){
 if(!request.headers.get('content-type')?.startsWith('application/json'))fail(415,'Send JSON.');
 const reader=request.body?.getReader();if(!reader)fail(400,'Missing request.');let size=0;const chunks=[];
 while(true){const {value,done}=await reader.read();if(done)break;size+=value.length;if(size>4096){await reader.cancel();fail(413,'Request too large.');}chunks.push(value);}
 const bytes=new Uint8Array(size);let offset=0;for(const c of chunks){bytes.set(c,offset);offset+=c.length;}
 try{const j=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes));if(!j||Array.isArray(j)||typeof j!=='object')throw Error();return j;}catch{fail(400,'Invalid request.');}
}
const text=(value,max,required=true)=>{
 if(typeof value!=='string')fail(400,'Check the fields.');const v=value.trim().normalize('NFC');
 if((required&&!v)||[...v].length>max||/[\p{Cc}\p{Cf}]/u.test(v))fail(400,'Check the fields and their length.');return v;
};
function publicFields(j){const label=text(j.label,80),note=text(j.note,280,false),connection_note=text(j.connection_note??'',80,false),url=text(j.url,2048);let u;try{u=new URL(url);}catch{fail(400,'Use an HTTPS destination.');}if(u.protocol!=='https:'||u.username||u.password)fail(400,'Use an HTTPS destination.');return {label,note,connection_note,url};}
const publicCard=c=>({id:c.id,label:c.label,note:c.note,url:c.url,connection_note:c.connection_note,revision:c.revision,recipient_name:c.recipient_name||''});
const dbFor=env=>({one:(sql,...args)=>env.DB.prepare(sql).bind(...args).first(),all:async(sql,...args)=>(await env.DB.prepare(sql).bind(...args).all()).results,run:(sql,...args)=>env.DB.prepare(sql).bind(...args).run(),stmt:(sql,...args)=>env.DB.prepare(sql).bind(...args)});
export default {async fetch(request,env){
 try{
  const u=new URL(request.url),path=u.pathname,method=request.method,now=Math.floor(Date.now()/1000),db=dbFor(env);
  if(method==='GET'&&path==='/robots.txt')return response('User-agent: *\nDisallow: /\n',200,'text/plain');
  if(method==='GET'&&path==='/favicon.svg')return response(pages.favicon,200,'image/svg+xml');
  if(method==='GET'&&(path==='/'||path==='/hello'))return response(pages.landing,200,'text/html; charset=utf-8');
  if(method==='GET'&&path==='/admin')return response(pages.admin,200,'text/html; charset=utf-8');
  let m;
  if(method==='GET'&&(m=path.match(/^\/h\/([a-f0-9]{32})$/))){
   const e=await db.one('SELECT id,expires_at,closed,submitted_at FROM encounters WHERE id=?',m[1]);
   if(!e||e.closed||(!e.submitted_at&&e.expires_at<now))return response(pages.expired,410,'text/html; charset=utf-8');
   return response(pages.hello,200,'text/html; charset=utf-8');
  }
  if(method==='GET'&&(m=path.match(/^\/(?:c|api\/card)\/([a-f0-9]{32})$/))){
   const c=await db.one('SELECT c.*,CASE WHEN e.share_name=1 THEN e.name ELSE NULL END AS recipient_name FROM cards c LEFT JOIN assignments a ON a.card_id=c.id LEFT JOIN encounters e ON e.id=a.encounter_id WHERE c.id=? AND c.disabled=0',m[1]);if(!c)fail(404,'Card unavailable.');
   return path.startsWith('/c/')?response(pages.card,200,'text/html; charset=utf-8'):response(publicCard(c));
  }
  if(method==='POST'&&(m=path.match(/^\/api\/hello\/([a-f0-9]{32})(\/opened)?$/))){
   // Only a random, short-lived encounter link grants submission access.
   if(request.headers.get('Origin')&&request.headers.get('Origin')!==u.origin)fail(403,'Open the original Hello page.');
   const e=await db.one('SELECT * FROM encounters WHERE id=?',m[1]);if(!e||e.closed)fail(410,'This hello has ended. Scan a new code on PET.');
   if(m[2]){if(e.expires_at<now&&!e.submitted_at)fail(410,'This hello has expired.');await db.run('UPDATE encounters SET opened_at=COALESCE(opened_at,?) WHERE id=?',now,e.id);return response({ok:true});}
   const j=await json(request),name=text(j.name,40),topic=text(j.topic,80);
   if(j.consent!==true||typeof j.share_topic!=='boolean'||(j.share_name!==undefined&&typeof j.share_name!=='boolean'))fail(400,'Please confirm how your hello will be saved.');
   // Match PET's installed glyphs. Keep unsupported text in the form for correction.
   if([...name+topic].some(c=>!pages.glyphs.includes(c.codePointAt(0))))fail(400,'PET cannot display one of these characters yet. Please use another spelling.');
   const same=x=>x.name===name&&x.topic===topic&&Boolean(x.share_topic)===j.share_topic&&Boolean(x.share_name)===(j.share_name===true);
   if(e.submitted_at){if(!same(e))fail(409,'This hello already has an introduction.');return response({ok:true});}
   if(e.expires_at<now)fail(410,'This hello has expired. Scan a new code on PET.');
   await db.run('UPDATE encounters SET name=?,topic=?,share_topic=?,share_name=?,consent_version=?,submitted_at=? WHERE id=? AND submitted_at IS NULL AND closed=0 AND expires_at>=?',name,topic,j.share_topic?1:0,j.share_name===true?1:0,consentVersion,now,e.id,now);
   const saved=await db.one('SELECT * FROM encounters WHERE id=?',e.id);if(!saved.submitted_at||!same(saved))fail(409,'This hello is already submitted or closed.');
   return response({ok:true});
  }
  const owner=path.startsWith('/api/admin/')||path==='/api/cards'||(method==='POST'&&path.startsWith('/api/card/'));
  const device=path.startsWith('/api/device/');
  if(owner&&!await authorized(request,env.PET_ADMIN_KEY,'X-PET-Admin'))fail(403,'Owner access required.');
  if(device&&!await authorized(request,env.PET_DEVICE_KEY,'X-PET-Device'))fail(403,'Badge access required.');
  if(owner){
   if(method==='GET'&&path==='/api/cards')return response({cards:await db.all('SELECT c.*,e.name AS recipient_name,e.topic AS introduction_topic,e.share_name,e.share_topic,a.encounter_id,a.assigned_at FROM cards c LEFT JOIN assignments a ON a.card_id=c.id LEFT JOIN encounters e ON e.id=a.encounter_id ORDER BY c.created_at DESC'),publicBase:u.origin});
   if(method==='POST'&&path==='/api/cards'){const id=randomId();await db.run('INSERT INTO cards(id,created_at) VALUES(?,?)',id,now);return response(await db.one('SELECT * FROM cards WHERE id=?',id),201);}
   if(method==='GET'&&path==='/api/admin/encounters')return response({encounters:await db.all('SELECT e.*,a.card_id FROM encounters e LEFT JOIN assignments a ON a.encounter_id=e.id WHERE e.submitted_at IS NOT NULL ORDER BY e.created_at DESC LIMIT 200')});
   if(method==='GET'&&path==='/api/admin/export')return response({schema_version:1,exported_at:now,encounters:await db.all('SELECT e.*,a.card_id,a.assigned_at FROM encounters e LEFT JOIN assignments a ON a.encounter_id=e.id WHERE e.submitted_at IS NOT NULL ORDER BY e.created_at')});
   if(method==='POST'&&path==='/api/admin/encounters'){const id=randomId();await db.run('INSERT INTO encounters(id,created_at,expires_at) VALUES(?,?,?)',id,now,now+600);return response({id,url:u.origin+'/h/'+id,expires_at:now+600},201);}
   if(method==='POST'&&(m=path.match(/^\/api\/card\/([a-f0-9]{32})$/))){const f=publicFields(await json(request));const c=await db.one('SELECT * FROM cards WHERE id=?',m[1]);if(!c)fail(404,'Card unavailable.');if(['label','note','url','connection_note'].some(k=>c[k]!==f[k]))await db.run('UPDATE cards SET label=?,note=?,url=?,connection_note=?,revision=revision+1 WHERE id=?',f.label,f.note,f.url,f.connection_note,c.id);return response(publicCard(await db.one('SELECT * FROM cards WHERE id=?',c.id)));}
   if(method==='POST'&&path==='/api/admin/assign')return await assign(await json(request),db,env,now);
   if(method==='DELETE'&&(m=path.match(/^\/api\/admin\/encounters\/([a-f0-9]{32})$/))){await env.DB.batch([db.stmt('UPDATE cards SET connection_note=?,revision=revision+1 WHERE id IN (SELECT card_id FROM assignments WHERE encounter_id=?)','',m[1]),db.stmt('DELETE FROM assignments WHERE encounter_id=?',m[1]),db.stmt('DELETE FROM encounters WHERE id=?',m[1])]);return response({ok:true});}
  }
  if(device){
   if(method==='GET'&&path==='/api/device/stats')return response(await db.one('SELECT COUNT(*) AS connections FROM encounters WHERE submitted_at IS NOT NULL'));
   if(method==='POST'&&path==='/api/device/encounters'){
    const j=await json(request);if(!ID.test(j.id||''))fail(400,'Invalid encounter.');
    await db.run('INSERT INTO encounters(id,created_at,expires_at) VALUES(?,?,?) ON CONFLICT(id) DO NOTHING',j.id,now,now+600);
    const e=await db.one('SELECT * FROM encounters WHERE id=?',j.id);if(e.closed||(!e.submitted_at&&e.expires_at<now))fail(410,'Hello expired.');
    return response({id:e.id,url:u.origin+'/h/'+e.id,expires_at:e.expires_at});
   }
   if(method==='POST'&&path==='/api/device/cards'){
    // PET supplies a random, durable ID so a retry after a lost response
    // returns the same unlisted page instead of creating an orphan each time.
    const j=await json(request);if(!ID.test(j.id||''))fail(400,'Invalid card.');
    await db.run('INSERT INTO cards(id,created_at) VALUES(?,?) ON CONFLICT(id) DO NOTHING',j.id,now);
    const c=await db.one('SELECT id,disabled FROM cards WHERE id=?',j.id);if(!c||c.disabled)fail(404,'Card unavailable.');
    return response({id:c.id,url:u.origin+'/c/'+c.id});
   }
   if(method==='GET'&&(m=path.match(/^\/api\/device\/encounters\/([a-f0-9]{32})$/))){const e=await db.one('SELECT e.*,a.card_id FROM encounters e LEFT JOIN assignments a ON a.encounter_id=e.id WHERE e.id=?',m[1]);if(!e||e.closed)fail(410,'Hello ended.');return response({card_id:e.card_id??'',id:e.id,opened:!!e.opened_at,submitted:!!e.submitted_at,expired:!e.submitted_at&&e.expires_at<now,name:e.name??'',topic:e.topic??'',share_topic:!!e.share_topic});}
   if(method==='POST'&&(m=path.match(/^\/api\/device\/encounters\/([a-f0-9]{32})\/close$/))){await db.run('UPDATE encounters SET closed=1 WHERE id=? AND submitted_at IS NULL',m[1]);return response({ok:true});}
   if(method==='POST'&&path==='/api/device/assign')return await assign(await json(request),db,env,now);
  }
  fail(404,'Not found.');
 }catch(e){if(!e.status)console.error('PET request failed',e.name);return response({error:e.status?e.message:'Temporarily unavailable. Please try again.'},e.status||503);}
}};
async function assign(j,db,env,now){
 for(const k of ['card_id','encounter_id','event_id'])if(!ID.test(j[k]||''))fail(400,'Invalid assignment.');
 const prior=await db.one('SELECT * FROM assignments WHERE event_id=?',j.event_id);
 if(prior){if(prior.card_id!==j.card_id||prior.encounter_id!==j.encounter_id)fail(409,'Assignment does not match.');return response({ok:true,card_id:prior.card_id});}
 const e=await db.one('SELECT * FROM encounters WHERE id=? AND submitted_at IS NOT NULL',j.encounter_id),c=await db.one('SELECT * FROM cards WHERE id=? AND disabled=0',j.card_id);
 if(!e||!c)fail(404,'Card or completed hello unavailable.');
 const occupied=await db.one('SELECT * FROM assignments WHERE card_id=? OR encounter_id=?',c.id,e.id);
 if(occupied)fail(409,'This card or hello is already assigned. Choose an unused card.');
 try{await env.DB.batch([db.stmt('INSERT INTO assignments(event_id,card_id,encounter_id,assigned_at) VALUES(?,?,?,?)',j.event_id,c.id,e.id,now),db.stmt('UPDATE cards SET connection_note=?,revision=revision+1 WHERE id=?',e.share_topic?e.topic:'',c.id)]);}catch(err){const p=await db.one('SELECT * FROM assignments WHERE event_id=?',j.event_id);if(p&&p.card_id===c.id&&p.encounter_id===e.id)return response({ok:true,card_id:c.id});if(await db.one('SELECT event_id FROM assignments WHERE card_id=? OR encounter_id=?',c.id,e.id))fail(409,'This card or hello is already assigned.');throw err;}
 return response({ok:true,card_id:c.id});
}
