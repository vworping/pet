(()=>{
'use strict';
const $=s=>document.querySelector(s),ui=$('.os');
const m={page:'home',settingsReturn:'home',spread:0,palettePaused:false,wifi:false,brightness:100,motion:!matchMedia('(prefers-reduced-motion: reduce)').matches,hello:false,joined:false,form:false,received:false,opening:false,cardStage:'idle',connections:0,network:'Studio',savedNetwork:'',enterprise:false};
const P=window.PET_CONSTELLATION.spreads;let openingTimer,greetingTimer;
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const pos=(x,y,w,h)=>`left:${x/10.24}cqw;top:${y/10.24}cqw;width:${w/10.24}cqw;${h?`height:${h/10.24}cqw;`:''}`;
const txt=(s,x,y,w,size=24,cls='',color='')=>`<div class="el txt ${cls}" style="${pos(x,y,w)}font-size:${size/10.24}cqw;${color?`color:${color};`:''}">${s}</div>`;
const title=(s,x,y,w,size=72,cls='')=>`<h1 class="el display ${cls}" style="${pos(x,y,w)}font-size:${size/10.24}cqw">${s}</h1>`;
const box=(x,y,w,h,cls='panel',extra='')=>`<div aria-hidden="true" class="el ${cls}" style="${pos(x,y,w,h)}${extra}"></div>`;
const btn=(s,x,y,w,h,action,fill=false,size=24,cls='')=>`<button aria-label="${s?esc(s.replace(/<[^>]*>/g,'')):'Explore Palette Constellation'}" class="el button ${fill?'primary':''} ${cls}" data-action="${action}" style="${pos(x,y,w,h)}font-size:${size/10.24}cqw">${s}</button>`;
const helloGreeting=()=>`<div class="el greeting-link hello-greeting" style="${pos(32,168,592,160)}" aria-label="Hello"><span class="greeting-brackets">[<span class="greeting-word">hello</span>]</span></div>`;
const qr=()=>`<div class="el hello-sheet" style="${pos(656,168,336,336)}">${m.opening?txt('Opening link',24,76,288,32,'display hello-info')+`<div class="el loading-track" style="${pos(24,252,288,4)}"><i></i></div>`:`<img class="el" style="${pos(0,0,336,336)}" src="assets/preview-qr.svg?v=clean-qr-13" alt="Demo QR code linking to the PET project page">`}</div>`+txt(m.opening?'OPENING YOUR LINK':'Demo QR opens the PET page.',656,536,336,18,'hello-info muted');
const header=()=>{
 const names={home:'',pet:'BUILDING / PET',atla:'BUILDING / ATLA',palette:'COLOR STUDIES',hello:'A SMALL HELLO',settings:'SETTINGS',wifi:'WI-FI',password:'JOIN NETWORK',network:'NETWORK DETAILS'};
 return btn(m.page==='home'?'[pet.]':'← Back',32,16,104,44,m.page==='home'?'home':'back',false,18)+(m.page==='home'?'':btn('Home',144,16,84,44,'home',false,18))+txt(names[m.page],252,29,440,18,'micro muted')+txt(m.opening?'Opening link':m.hello?(m.joined?'Phone linked':'Phone link'):(m.wifi?'Wi-Fi on':'Wi-Fi off'),704,29,160,18,'muted header-status')+btn('Settings',880,16,112,44,'settings',false,18)+box(32,76,960,1,'rule');
};
function page(){
 let h='';
 if(m.page==='home'){
  h=title('ANDRE<br>WEISS.',54,141,650,104,'home-name')+txt('Photographer x researcher',54,374,650,28,'muted');
  h+=`<button class="el greeting-link" style="${pos(562,183,420,214)}" data-action="hello" aria-label="Open Hello"><span class="greeting-brackets">[<span class="greeting-word">hello</span>]</span></button>`;
  h+=`<div class="el building-heading" style="${pos(32,453,960,24)}"><span>CURRENTLY BUILDING</span><i></i></div>`+btn('[pet.]',32,491,296,72,'pet')+btn('ATLA',344,491,296,72,'atla')+btn('Constellation',656,491,336,72,'palette');
 }else if(m.page==='pet'){
  h=title('[pet.]',32,144,586,192,'pet-wordmark')+txt('A wearable introduction.',40,394,578,36,'muted')+box(650,110,342,460)+title('Inside [pet.]',674,139,294,32)+box(674,193,294,1,'rule');
  h+=txt('DISPLAY + BRAIN',674,210,294,18,'','var(--green)')+txt('5-inch Waveshare touch<br>ESP32-S3 / 1024 x 600',674,241,294,24,'hardware-lines')+box(674,316,294,1,'rule');
  h+=txt('NFC / RFID',674,326,294,18,'','var(--green)')+txt('NULLLAB RC522 / I2C<br>13.56 MHz card reader',674,357,294,24,'hardware-lines')+box(674,432,294,1,'rule');
  h+=txt('POWER',674,442,294,18,'','var(--green)')+txt('3.7V / 1000mAh LiPo<br>Rechargeable battery',674,473,294,24,'hardware-lines')+btn('Open Hello ›',40,494,286,72,'hello',true);
 }else if(m.page==='atla'){
  h=`<img class="el atla-logo" style="${pos(144,210,400,257)}" src="assets/atla-logo.png" alt="ATLA Innovations">`+txt('Ask me about...',552,225,424,28,'muted atla-centered')+txt('FIRST PRODUCT / PRE-SEED',552,272,424,18,'micro atla-centered','#e18a4e')+title('EAR',552,310,424,104,'atla-centered')+txt('ATLA Prints',552,416,424,36,'atla-centered');
 }else if(m.page==='palette'){
  const s=P[m.spread],mainWidth=214,mainHeight=336,matchX=286;
  h=title('Palette Constellation',48,118,470,32,'palette-title');
  h+=`<button id="palette-spread" class="el button spread-toggle" style="${pos(544,106,224,56)}" aria-label="Spread" aria-haspopup="listbox" aria-expanded="false" aria-controls="spread-options" data-action="spread-menu"><span style="color:${s.accent}">${s.name}</span><i aria-hidden="true"></i></button><div id="spread-options" class="el spread-options" style="${pos(544,170,224)}--spread-accent:${s.accent}" role="listbox" aria-label="Spread options" hidden>${P.map((p,i)=>`<button role="option" data-spread="${i}" style="color:${p.accent}" aria-selected="${i===m.spread}" tabindex="-1">${p.name}</button>`).join('')}</div>`;
  h+=btn(m.palettePaused||!m.motion?'Resume motion':'Stop motion',784,106,192,56,'palette-motion',false,18);
  h+=txt('Main',48,192,mainWidth,18,'',s.accent)+txt('Closest palettes',matchX,192,320,18,'muted');
  h+=`<div class="el main-photo" style="${pos(48,228,mainWidth,mainHeight)}--spread-accent:${s.accent}">${paletteCard(s.source,0)}</div><div class="el palette-deck" style="${pos(matchX,228,976-matchX,mainHeight)}--spread-accent:${s.accent}" aria-label="Closest palette photographs">${s.neighbors.map((p,i)=>paletteCard(p,i+1)).join('')}</div>`;
 }else if(m.page==='hello'){
  const step=m.received?2:m.opening?0:!m.hello||m.form?1:0;
  ['INTRODUCTION','SAY HELLO','CONNECT'].forEach((name,i)=>{h+=`<div class="el hello-step ${i===step?'active':''}" style="${pos(32+i*328,96,304,36)}" ${i===step?'aria-current="step"':''}>0${i+1} / ${name}</div>`+box(32+i*328,140,304,2,'rule',`background:${i===step?'var(--green)':'var(--line)'}`);});

  if(m.received){
   const review=m.cardStage==='review',saved=m.cardStage==='saved';
   h+=txt('NICE TO MEET YOU',32,178,592,18,'hello-info','var(--green)')+title('Maya Chen',32,214,592,72,'guest-greeting')+box(32,352,592,1,'rule')+txt('CONVERSATION STARTER',32,374,592,18,'hello-info muted')+txt('Color and human connection',32,414,592,28,'hello-topic')+box(656,168,336,340,'hello-sheet')+title(saved?'Card updated.':'NFC card',680,196,288,32,'hello-paper')+txt(saved?'Ready to share.':review?'Confirm the card below.':'Save this introduction.',680,258,288,18,'hello-info hello-paper');
   const messages={idle:'Prepare a tag to save this introduction, or select Done.',armed:'Hold a blank NFC tag on PET.',review:'Tag found. Select Save to card to confirm.',saved:'Updated. This card is ready to give.',clearing:'Remove any card, then tap a writable tag to erase its link.',cleared:'Tag cleared. Ready for a new introduction.'};
   h+=box(32,524,960,1,'rule')+txt(messages[m.cardStage],32,544,960,18,'hello-info card-message',saved?'var(--green)':'var(--muted)');
   if(['idle','cleared'].includes(m.cardStage))h+=btn('Prepare tag',680,364,164,48,'card-arm',false,24,'hello-action hello-outline')+btn('Clear tag',856,364,112,48,'card-clear',false,18,'hello-action hello-paper');
   if(review)h+=txt('Card 22222222',680,376,288,18,'hello-info hello-paper');
   h+=btn(review?'Save to card':'Done',680,428,288,56,review?'card-save':'accept',true,24,'hello-action');
  }else if(!m.hello&&!m.opening){
   h+=title('Try again.',32,190,592,72)+txt('This introduction has ended.',32,336,592,28,'hello-info')+txt('Open a new link when you are ready.',32,408,592,24,'hello-info muted')+box(656,168,336,336,'hello-sheet')+title('New hello',680,208,288,32,'hello-paper')+btn('Start again',680,420,288,56,'hello',true,24,'hello-action');
  }else{
   const form=m.form&&!m.opening;
   h+=(form?title('Your turn.',32,188,592,72)+box(32,300,592,1,'rule')+txt('NAME',32,328,592,18,'hello-info muted')+txt('Introduce yourself on your phone.',32,364,592,28,'hello-info')+box(32,418,592,1,'rule')+txt('TOPIC',32,440,592,18,'hello-info muted')+txt("What would you like to talk about?",32,476,592,28,'hello-info'):helloGreeting()+txt('Scan to start a conversation.',32,400,592,28,'hello-info')+txt('No app or sign-in needed.',32,452,592,18,'hello-info muted'))+qr();
  }
 }else if(m.page==='settings'){
  h=title('Settings',32,110,950)+btn('Wi-Fi <span aria-hidden="true">›</span>',32,228,320,92,'wifi',false,24,'settings-control')+btn(`Motion <span>${m.motion?'On':'Off'}</span>`,32,336,320,92,'motion',m.motion,24,'settings-control')+btn('Sleep',32,444,320,92,'sleep',false,24,'settings-control')+txt('Brightness',424,290,360,36)+txt(`<output>${m.brightness}%</output>`,828,296,148,28,'brightness-value','var(--green)')+`<input id="brightness" class="el range" aria-label="Brightness" style="${pos(424,360,552,48)}--level:${m.brightness}%" type="range" min="0" max="100" value="${m.brightness}"><i id="brightness-handle" class="el brightness-handle" style="${pos(424+552*m.brightness/100-40,368,32,32)}" aria-hidden="true"></i>`+title(m.connections,720,450,256,72,'hello-count hello-total')+txt('Hellos',720,530,256,24,'hello-count');
 }else if(m.page==='wifi'){
  const active=m.wifi&&m.savedNetwork;
  h=title('Wi-Fi',32,110,590)+txt(m.wifi?'Connected':'Wi-Fi off',640,144,336,24,'brightness-value','var(--green)')+btn('Scan',32,228,320,92,'scan',false,24,'settings-control')+btn('Manual entry',32,336,320,92,'manual-network',false,24,'settings-control')+btn('Wi-Fi off',32,444,320,92,'wifi-off',false,24,'settings-control')+(active?btn(`<span class="network-name">${esc(m.savedNetwork)}</span><small>2.4 GHz</small><small>Manage ›</small>`,424,228,536,92,'network',true,24,'connected-network'):'')+`<div class="el wifi-networks" style="${pos(424,active?336:228,552,active?200:308)}">${['Studio','Guest','Workshop','Lobby','Office','eduroam'].filter(n=>!active||n!==m.savedNetwork).map(n=>`<button class="button" data-network="${n}" data-action="password"><span class="network-name">${n}</span><small>2.4 GHz　 ›</small></button>`).join('')}</div>`+(!active&&m.savedNetwork?btn('Manage saved network ›',424,544,536,40,'network',false,18):'');
 }else if(m.page==='network'){
  h=title('Network details',32,110,960)+box(32,228,960,348)+txt(esc(m.savedNetwork),56,250,652,36)+txt(m.wifi?'Connected':'Saved',744,259,224,24,'brightness-value','var(--green)')+box(56,310,912,1,'rule')+['BAND','CHANNEL','SIGNAL','IP ADDRESS'].map((heading,i)=>txt(heading,56+232*i,334,216,18,'muted')+txt(m.wifi?['2.4 GHz','6','-48 dBm','192.0.2.10'][i]:'--',56+232*i,369,216,24)).join('')+box(56,421,912,1,'rule')+txt('Forget removes saved credentials and disconnects Wi-Fi.',56,441,912,18,'muted')+btn('Edit connection',56,488,440,64,'edit-network',false,24,'edit-network')+btn('Forget this network',512,488,456,64,'forget-wifi',false,24,'forget-network');
 }else if(m.page==='password'){
  h=title('Connect to Wi-Fi',32,97,960,32)+`<input class="el field" aria-label="Network name" style="${pos(32,145,460,58)}font-size:2.344cqw" id="ssid" value="${esc(m.network)}" maxlength="32"><input class="el field" aria-label="Network password" style="${pos(512,145,408,58)}font-size:2.344cqw" id="wifi-password" type="password" placeholder="Password" maxlength="63"><button class="el button" aria-label="Show password" aria-pressed="false" data-action="show-password" style="${pos(932,145,60,58)}"><svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/></svg></button><input class="el field" aria-label="Username" id="wifi-username" style="${pos(32,215,460,58)}font-size:2.344cqw;${m.enterprise?'':'display:none'}" placeholder="Full email / user@umb.edu" maxlength="127">`+btn(m.enterprise?'Username + password':'Password only',512,215,240,58,'enterprise-toggle',false,18)+btn('Connect',768,215,224,58,'connect-wifi',true)+txt('Preview only. Credentials are not saved or transmitted.',32,282,960,18,'muted');

 }
 return h;
}
let paletteDialog,returnPhoto;
const swatches=p=>p.palette.map((c,i)=>`<i style="background:${c.hex};flex:${c.weight};--swatch-index:${i}"></i>`).join('');
function paletteCard(p,i){return `<button class="palette-card" data-photo="${i}" aria-label="Inspect ${i?'closest palette '+i:'main photograph'}, ${esc(P[m.spread].name)}"><span class="palette-photo"><img src="${p.src}" alt="${esc(p.id)} / ${p.year}" draggable="false"></span><span class="swatches">${swatches(p)}</span><span class="palette-caption">${i?`Distance ${p.distance.toFixed(2)}`:P[m.spread].name}</span></button>`;}
function shutterFrames(){return [{clipPath:'inset(49.5% 0 49.5% 0)',transform:'scale(.985)',opacity:.7},{clipPath:'inset(46% 0 46% 0)',transform:'scale(.99)',opacity:1,offset:.18},{clipPath:'inset(0% 0 0% 0)',transform:'scale(1)',opacity:1}];}
function inspectionLayout(p){
 const [width,height]=p.detailDimensions,frameWidth=width+24,frameHeight=height+42;
 const diameter=Math.min(288,Math.max(216,Math.round(height*.75)),656-frameWidth);
 const groupWidth=frameWidth+48+diameter+40+184,x=(1024-groupWidth)/2;
 return {width,height,x,y:340-frameHeight/2,diameter,wheelX:x+frameWidth+48,wheelY:382-diameter/2,legendY:232,legendHeight:300,legendX:x+frameWidth+48+diameter+40};
}
function paletteWheel(p){
 const total=p.palette.reduce((sum,c)=>sum+c.weight,0);let at=0;
 return p.palette.map(c=>{const start=at;at+=c.weight/total*100;return `${c.hex} ${start}% ${at}%`;}).join(',');
}
function inspectPhoto(index,trigger){
 if(paletteDialog)return;const s=P[m.spread],p=[s.source,...s.neighbors][index],frame=inspectionLayout(p);returnPhoto=trigger;
 paletteDialog=document.createElement('div');paletteDialog.className='palette-detail';paletteDialog.setAttribute('role','dialog');paletteDialog.setAttribute('aria-modal','true');paletteDialog.setAttribute('aria-label','Photograph and measured palette');
 paletteDialog.innerHTML=`<div class="detail-sheet" style="--spread-accent:${s.accent}"><div class="el detail-heading" style="${pos(48,29,552)}"><span class="detail-kicker" style="color:${s.accent}">${index?'CLOSEST PALETTE':'MAIN PHOTOGRAPH'}</span><span class="detail-meta">${esc(p.id)} / ${p.year}</span></div>${btn('Close',832,16,144,44,'close-photo',false,18,'detail-close')}<figure class="el detail-print" style="${pos(frame.x,frame.y,frame.width+24,frame.height+42)}"><img class="el detail-photo" style="${pos(12,12,frame.width,frame.height)}" src="${p.detailSrc}" alt="${esc(p.id)} / ${p.year}"><figcaption class="el" style="${pos(12,frame.height+20,frame.width)}"><span>${esc(p.id)} / ${p.year}</span><i style="background:${s.accent}"></i></figcaption></figure>${title(s.name,frame.wheelX,140,frame.diameter+224,40,'spread-name detail-title')}${txt('MEASURED PALETTE',frame.wheelX,196,frame.diameter+224,18,'muted detail-palette-label')}<div class="el palette-wheel" style="${pos(frame.wheelX,frame.wheelY,frame.diameter,frame.diameter)}background:conic-gradient(${paletteWheel(p)})" role="img" aria-label="Five measured colors; each segment shows its share of the photograph"></div><div class="el palette-values" style="${pos(frame.legendX,frame.legendY,184,frame.legendHeight)}">${p.palette.map(c=>`<div><i style="background:${c.hex}"></i><span>${c.hex.toUpperCase()}</span><span>${(c.weight*100).toFixed(1)}%</span></div>`).join('')}</div>${index?txt(`Distance ${p.distance.toFixed(2)}`,624,29,184,18,'muted detail-distance'):''}</div>`;

 ui.append(paletteDialog);$('.page').inert=true;ui.querySelectorAll(':scope > button').forEach(b=>b.inert=true);paletteDialog.querySelector('button').focus({preventScroll:true});
 paletteDialog.classList.toggle('no-motion',!m.motion||m.palettePaused);
 if(m.motion&&!m.palettePaused)paletteDialog.querySelector('.detail-sheet').animate(shutterFrames(),{duration:220,easing:'cubic-bezier(.16,1,.3,1)'});
}
async function closePhoto(){
 if(!paletteDialog||paletteDialog.dataset.closing)return;const dialog=paletteDialog;dialog.dataset.closing='true';
 if(m.motion&&!m.palettePaused)await dialog.querySelector('.detail-sheet').animate(shutterFrames(),{duration:180,easing:'cubic-bezier(.7,0,.84,0)',direction:'reverse',fill:'forwards'}).finished.catch(()=>{});
 dialog.remove();paletteDialog=null;$('.page').inert=false;ui.querySelectorAll(':scope > button').forEach(b=>b.inert=false);returnPhoto?.focus({preventScroll:true});
}
function toggleSpread(open){const menu=$('#spread-options');if(!menu)return;const expanded=open??menu.hidden;menu.hidden=!expanded;$('#palette-spread').setAttribute('aria-expanded',String(expanded));if(expanded)menu.querySelector('[aria-selected="true"]').focus();}
document.addEventListener('click',e=>{if(!e.target.closest('.spread-options,#palette-spread'))toggleSpread(false);});
document.addEventListener('keydown',e=>{
 const menu=$('#spread-options');if(!menu||menu.hidden)return;
 const options=[...menu.querySelectorAll('[role="option"]')],index=options.indexOf(document.activeElement);
 if(e.key==='Escape'){e.preventDefault();toggleSpread(false);$('#palette-spread').focus();}
 if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?options.length-1:(index+(e.key==='ArrowDown'?1:-1)+options.length)%options.length;options[next].focus();}
});
document.addEventListener('pointerdown',()=>ui.classList.remove('keyboard-nav'));
document.addEventListener('keydown',e=>{if(['Tab','ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(e.key))ui.classList.add('keyboard-nav');});
document.addEventListener('keydown',e=>{
 if(document.querySelector('dialog[open]'))return;
 if(paletteDialog&&e.key==='Escape'){e.preventDefault();closePhoto();}
 if(paletteDialog&&e.key==='Tab'){
  const buttons=[...paletteDialog.querySelectorAll('button')],at=buttons.indexOf(document.activeElement);
  e.preventDefault();buttons[(at+(e.shiftKey?-1:1)+buttons.length)%buttons.length].focus();
 }
});
document.addEventListener('keydown',e=>{if(!paletteDialog&&e.target.closest('.palette-deck')&&['ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();const cards=[...document.querySelectorAll('.palette-deck .palette-card')],index=cards.indexOf(e.target.closest('.palette-card'));cards[(index+(e.key==='ArrowLeft'?2:1))%cards.length].focus({preventScroll:true});}});
function startGreeting(){
 clearInterval(greetingTimer);
 if(!$('.greeting-word')||!m.motion)return;
 const words=['hello','hi','hey','howdy','hola','buenas','salut','bonjour','coucou'],colors=['#bfd6b8','#e8b5ce','#e8bf88','#aacbdc','#d6c2ed','#e6ad94','#bfd6b8','#aacbdc','#e8b5ce'];
 let index=0,hold=10,erasing=true,current='hello';
 const word=$('.greeting-word'),link=$('.greeting-link');
 greetingTimer=setInterval(()=>{
  if($('.sleep')||document.hidden)return;if(hold){hold--;return;}
  const next=(index+1)%words.length,target=words[next];let prefix=0;
  while(current[prefix]&&current[prefix]===target[prefix])prefix++;
  if(erasing&&current.length>prefix)current=current.slice(0,-1);
  else {erasing=false;link.style.color=m.page==='hello'?'var(--green)':colors[next];current+=target[current.length]??'';}
  const before=word.getBoundingClientRect().width;word.textContent=current;
  const after=word.getBoundingClientRect().width;
  word.animate([{width:before+'px'},{width:after+'px'}],{duration:180,easing:'ease-out'});
  if(current===target){index=next;hold=10;erasing=true;}
 },210);
}
function display(){ $('.dim').style.opacity=(100-m.brightness)/128;$('.screen').classList.toggle('still',!m.motion);document.querySelector('.controls [data-action="motion"]').textContent=m.motion?'Pause motion':'Resume motion'; }
function render(){clearInterval(greetingTimer);paletteDialog=null;ui.innerHTML=header()+`<main class="page" aria-label="${m.page}">${page()}</main>`;
 const next=$('[data-demo="next"]');next.disabled=m.page==='hello'&&(m.opening||m.received&&m.cardStage==='review');next.textContent=m.page!=='hello'?'Try Hello':m.opening?'Preparing link…':m.received?(['armed','clearing'].includes(m.cardStage)?'Tap NFC tag':m.cardStage==='review'?'Review tag above':'Restart Hello'):!m.hello?'New hello':m.form?'Send introduction':'Scan QR';
 $('[data-demo="reset"]').disabled=m.page==='home';display();startGreeting();}
function go(p){m.page=p;render();}
function reset(){clearTimeout(openingTimer);m.hello=m.joined=m.form=m.received=m.opening=false;m.cardStage='idle';}
function back(){const p=m.page;if(p==='hello'||p==='done')reset();go(p==='settings'?m.settingsReturn:p==='wifi'?'settings':['password','network'].includes(p)?'wifi':'home');}
document.addEventListener('click',e=>{
 if(e.target.closest('[data-save-image],dialog'))return;
 const b=e.target.closest('button');if(!b)return;const a=b.dataset.action,d=b.dataset.demo;if(b.dataset.network){m.network=b.dataset.network;m.enterprise=m.network==='eduroam';}
 if(d==='reset'){reset();go('home');return;}
 if(d==='next'){
  if(m.page==='hello'&&m.received&&['armed','clearing'].includes(m.cardStage)){m.cardStage=m.cardStage==='armed'?'review':'cleared';render();return;}
  if(m.page!=='hello'||m.received||!m.hello&&!m.opening){reset();m.opening=true;go('hello');openingTimer=setTimeout(()=>{if(m.page==='hello'&&m.opening){m.opening=false;m.hello=true;render();}},1800);}
  else if(!m.opening){if(m.form){m.received=true;m.cardStage='idle';}else{m.joined=true;m.form=true;}render();}
  return;
 }
 if(d&&m.page==='hello'){
  if(d==='tag'&&m.received){if(m.cardStage==='armed')m.cardStage='review';else if(m.cardStage==='clearing')m.cardStage='cleared';render();return;}
  if(d==='ready'&&m.opening){clearTimeout(openingTimer);m.opening=false;m.hello=true;render();return;}
  if(!m.hello||m.received||m.opening)return;
  if(d==='join')m.joined=true;
  if(d==='open'&&m.joined)m.form=true;
  if(d==='receive'&&m.form){m.received=true;m.cardStage='idle';}
  if(d==='disconnect'){m.joined=false;m.form=false;}
  if(d==='expire')reset();render();return;
 }
 if(a==='card-arm'&&m.received){m.cardStage='armed';render();return;}
 if(a==='card-clear'&&m.received){m.cardStage='clearing';render();return;}
 if(a==='card-save'&&m.received&&m.cardStage==='review'){m.cardStage='saved';render();return;}
 if(a==='spread-menu'){toggleSpread();return;}
 if(b.dataset.photo!==undefined){inspectPhoto(+b.dataset.photo,b);return;}
 if(a==='close-photo'){closePhoto();return;}
 if(a==='palette-motion'){if(!m.motion){m.motion=true;m.palettePaused=false;}else m.palettePaused=!m.palettePaused;b.textContent=m.palettePaused?'Resume motion':'Stop motion';display();return;}
 if(a==='show-password'){const input=$('#wifi-password'),show=input.type==='password';input.type=show?'text':'password';b.setAttribute('aria-label',show?'Hide password':'Show password');b.setAttribute('aria-pressed',String(show));return;}
 if(a==='enterprise-toggle'){m.enterprise=!m.enterprise;$('#wifi-username').style.display=m.enterprise?'':'none';b.textContent=m.enterprise?'Username + password':'Password only';return;}
 if(a==='manual-network'){m.network='';m.enterprise=false;go('password');return;}
 if(a==='edit-network'){m.network=m.savedNetwork;go('password');return;}
 if(a==='back'){back();return;}if(a==='home'){reset();go('home');return;}
 if(a==='settings'){if(!['settings','wifi','password','network'].includes(m.page))m.settingsReturn=m.page;go('settings');return;}
 if(a==='hello'){reset();m.opening=true;go('hello');openingTimer=setTimeout(()=>{if(m.page==='hello'&&m.opening){m.opening=false;m.hello=true;render();}},1800);return;}
 if(b.dataset.spread!==undefined){m.spread=Number(b.dataset.spread);render();$('#palette-spread').focus();return;}
 else if(a==='accept'&&m.received){m.connections++;reset();go('home');return;}
 else if(a==='wifi-off')m.wifi=false;
 else if(a==='forget-wifi'){m.wifi=false;m.savedNetwork='';go('wifi');return;}
 else if(a==='guest-network'){m.network='Guest';m.wifi=true;}
 else if(a==='connect-wifi'){m.network=$('#ssid').value;if(!m.network||(m.enterprise&&(!$('#wifi-username').value||!$('#wifi-password').value)))return;m.savedNetwork=m.network;m.wifi=true;go('wifi');return;}
 else if(a==='motion')m.motion=!m.motion;
 else if(a==='sleep'){const el=document.createElement('div');el.className='sleep';el.innerHTML='<button data-action="wake">Tap to wake</button>';$('.screen').append(el);return;}
 else if(a==='wake'){$('.sleep')?.remove();return;}
 else if(['pet','atla','palette','wifi','password','network'].includes(a)){go(a);return;}
 render();
});
document.addEventListener('input',e=>{if(e.target.id==='brightness'){m.brightness=Math.max(20,+e.target.value);e.target.value=m.brightness;e.target.style.setProperty('--level',m.brightness+'%');$('output').textContent=m.brightness+'%';$('#brightness-handle').style.left=(424+552*m.brightness/100-40)/10.24+'cqw';}});
document.addEventListener('change',e=>{if(e.target.id==='brightness')display();});
if(location.hash==='#palette')m.page='palette';
window.addEventListener('hashchange',()=>{if(location.hash==='#palette')go('palette');});
// Optional host-page motion control; the standalone preview remains independent.
document.addEventListener('pet:motion',event=>{
 m.motion=!event.detail.paused;
 display();startGreeting();
 paletteDialog?.classList.toggle('no-motion',!m.motion||m.palettePaused);
});
render();
})();
