#include "lvgl.h"
#include "pet_ui.h"
#include "pet_services.h"
void test_introduction(void);
void test_card_found(void);
void test_card_updated(void);
void test_phone_joined(bool joined);
void test_form_opened(void);
void test_link_closed(void);
void test_hello_opened(void);
void test_long_introduction(void);
void test_hello_offline(void);
void test_card_cleared(void);
void test_hello_stage(pet_hello_stage_t stage);
#include <assert.h>
#include <stdio.h>
#include <string.h>
static lv_color_t pixels[1024*600],draw[1024*40];
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)pixels[y*1024+x]=*p++;
 lv_disp_flush_ready(d);
}
static void tick(unsigned ms){for(unsigned i=0;i<ms;i+=10){lv_tick_inc(10);lv_timer_handler();}}
static void shot(const char *name){
 lv_refr_now(NULL);FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");
 for(int i=0;i<1024*600;i++){lv_color32_t c={.full=lv_color_to32(pixels[i])};unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,f);}fclose(f);
}
static lv_obj_t *find(lv_obj_t *o,const char *value){
 if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return NULL;
 if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),value))return o;
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find(lv_obj_get_child(o,i),value);if(r)return r;}return NULL;
}
static void click(const char *value){lv_obj_t *l=find(lv_scr_act(),value);assert(l);lv_event_send(lv_obj_get_parent(l),LV_EVENT_CLICKED,NULL);tick(30);}
static lv_obj_t *find_type(lv_obj_t *o,const lv_obj_class_t *type){
 if(lv_obj_check_type(o,type))return o;
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find_type(lv_obj_get_child(o,i),type);if(r)return r;}return NULL;
}
static void choose_spread(unsigned index){
 lv_obj_t *menu=find_type(lv_scr_act(),&lv_dropdown_class);assert(menu);
 lv_dropdown_set_selected(menu,index);lv_event_send(menu,LV_EVENT_VALUE_CHANGED,NULL);tick(30);
}
static void check_bounds(lv_obj_t *o);
static void check_constellation(void){
 click("Constellation");assert(find(lv_scr_act(),"Palette Constellation"));assert(!find(lv_scr_act(),"1 / 3"));
 tick(5500);assert(!find(lv_scr_act(),"2 / 3"));
 click("Stop motion");tick(5500);assert(!find(lv_scr_act(),"2 / 3"));
 assert(!find(lv_scr_act(),">")&&!find(lv_scr_act(),"<"));
 shot("constellation-paused.ppm");click("Afterglow");assert(find(lv_scr_act(),"MEASURED PALETTE"));check_bounds(lv_scr_act());
 shot("constellation-detail-still.ppm");click("Close");assert(!find(lv_scr_act(),"MEASURED PALETTE"));assert(find(lv_scr_act(),"Resume motion"));
 click("Resume motion");tick(5500);assert(!find(lv_scr_act(),"3 / 3"));
 for(unsigned i=0;i<4;i++){
  choose_spread(i);const char *names[]={"Afterglow","Crimson","Blue hour","Silver"};assert(find(lv_scr_act(),names[i]));
  lv_obj_t *source=lv_obj_get_parent(find(lv_scr_act(),names[i]));
  const uint32_t accents[]={0xd5a4bd,0xdf746b,0x9bb9e8,0xc9cdc9};
  assert(lv_obj_get_style_text_color(find(lv_scr_act(),names[i]),0).full==lv_color_hex(accents[i]).full);
  lv_obj_t *menu=find_type(lv_scr_act(),&lv_dropdown_class);assert(lv_obj_get_style_text_color(menu,0).full==lv_color_hex(accents[i]).full);
  lv_obj_t *photo=find_type(source,&lv_img_class);assert(photo);
  lv_obj_update_layout(source);assert(lv_obj_get_width(source)==214&&lv_obj_get_height(source)==336);assert(lv_obj_get_width(photo)==168&&lv_obj_get_height(photo)==252);assert(lv_obj_get_x(photo)==8+(198-lv_obj_get_width(photo))/2);assert(lv_obj_get_y(photo)==8+(268-lv_obj_get_height(photo))/2);
  int swatch_width=0;for(unsigned c=1;c<=5;c++)swatch_width+=lv_obj_get_width(lv_obj_get_child(source,c));assert(swatch_width==198);
  click(names[i]);tick(1000);assert(find(lv_scr_act(),"MEASURED PALETTE"));assert(!find(lv_scr_act(),"Source palette"));
  assert(lv_obj_get_style_text_color(find(lv_scr_act(),names[i]),0).full==lv_color_hex(accents[i]).full);
  lv_obj_t *detail_photo=find_type(lv_obj_get_parent(find(lv_scr_act(),"MEASURED PALETTE")),&lv_img_class);assert(detail_photo&&lv_obj_get_x(detail_photo)==12&&lv_obj_get_y(detail_photo)==12);
  lv_obj_t *frame=lv_obj_get_parent(detail_photo);assert(lv_obj_get_width(frame)<=416&&lv_obj_get_height(frame)<=434);
  assert(lv_obj_get_style_bg_color(lv_obj_get_parent(frame),0).full==lv_color_hex(0x111316).full);
  assert(!find(lv_scr_act(),"05")&&!find(lv_scr_act(),"COLORS"));
  check_bounds(lv_scr_act());

  char path[48];snprintf(path,sizeof(path),"constellation-detail-%u.ppm",i);shot(path);
  click("Close");tick(300);assert(!find(lv_scr_act(),"MEASURED PALETTE"));
 }
 choose_spread(0);click("Distance 20.12");tick(800);assert(find(lv_scr_act(),"CLOSEST PALETTE"));shot("constellation-neighbor-detail.ppm");click("Close");tick(300);
 /* The overview stays idle; opening the dropdown and returning remain safe. */
 lv_obj_t *menu=find_type(lv_scr_act(),&lv_dropdown_class);lv_dropdown_open(menu);tick(30);shot("constellation-menu.ppm");lv_dropdown_close(menu);
 tick(45000);shot("constellation-loop.ppm");click("Stop motion");click("Afterglow");pet_ui_show(0);tick(300);assert(!find(lv_scr_act(),"MEASURED PALETTE"));
 /* Global reduced motion is respected on entry and can be resumed explicitly. */
 pet_services_set_motion(false);click("Constellation");assert(find(lv_scr_act(),"Resume motion"));click("Resume motion");assert(find(lv_scr_act(),"Stop motion"));
 click("< Back");
}
static void check_step(unsigned active){
 const char *steps[]={"01 / INTRODUCTION","02 / SAY HELLO","03 / CONNECT"};
 for(unsigned i=0;i<3;i++){
  lv_obj_t *l=find(lv_scr_act(),steps[i]);assert(l);
  lv_color_t expected=lv_color_hex(i==active?0x111316:0xa7acb3);
  assert(lv_obj_get_style_text_color(l,0).full==expected.full);
 }
}
static lv_obj_t *loading_segment(void){
 for(unsigned i=0;i<lv_obj_get_child_cnt(lv_scr_act());i++){
  lv_obj_t *o=lv_obj_get_child(lv_scr_act(),i);
  if(lv_obj_get_y(o)==420&&lv_obj_get_width(o)==72&&lv_obj_get_height(o)==4)return o;
 }
 return NULL;
}
static void check_guest_layout(void){
 lv_obj_t *prompt=find(lv_scr_act(),"CONVERSATION STARTER");assert(prompt);
 pet_status_t status;pet_services_snapshot(&status);
 lv_obj_t *topic=find(lv_scr_act(),status.interest);assert(topic);
 lv_area_t a,prompt_area;lv_obj_get_coords(prompt,&prompt_area);lv_obj_get_coords(topic,&a);assert(a.y2<524&&a.y1>prompt_area.y2);
 char greeting[PET_NAME_SIZE+16];snprintf(greeting,sizeof(greeting),"%s",status.guest);
 lv_obj_t *name=find(lv_scr_act(),greeting);assert(name);
 lv_obj_get_coords(name,&a);if(a.y2>=prompt_area.y1-8)fprintf(stderr,"Name bottom=%d prompt=%d name=%s\n",a.y2,prompt_area.y1,greeting);assert(a.y2<352);
}
static void check_bounds(lv_obj_t *o){
 if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
 lv_area_t a;lv_obj_get_coords(o,&a);
 if(a.x1<0||a.y1<0||a.x2>=1024||a.y2>=600){fprintf(stderr,"Overflow: %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2);assert(0);}
 if(lv_obj_get_scroll_dir(o)==LV_DIR_VER&&lv_obj_has_flag(o,LV_OBJ_FLAG_SCROLLABLE))return;
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)check_bounds(lv_obj_get_child(o,i));
}
int main(void){
 lv_init();static lv_disp_draw_buf_t b;lv_disp_draw_buf_init(&b,draw,NULL,1024*40);
 static lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=1024;d.ver_res=600;d.draw_buf=&b;d.flush_cb=flush;lv_disp_drv_register(&d);
 pet_services_init();pet_ui_init();tick(40);check_bounds(lv_scr_act());shot("home.ppm");
 const char *greetings[]={"hi","hey","howdy","hola","buenas","salut","bonjour","coucou","hello"};
 for(unsigned g=0;g<9;g++){
  unsigned waited=0;while(!find(lv_scr_act(),greetings[g])&&waited<8000){tick(10);waited+=10;check_bounds(lv_scr_act());}
  assert(find(lv_scr_act(),greetings[g]));tick(200);check_bounds(lv_scr_act());
  char path[40];snprintf(path,sizeof(path),"home-%s.ppm",greetings[g]);shot(path);
 }
 click("hello");assert(find(lv_scr_act(),"Scan to start a conversation."));click("Home");
 pet_ui_show(1);tick(500);assert(find(lv_scr_act(),"Inside [pet.]"));check_bounds(lv_scr_act());shot("pet-project.ppm");click("Open Hello >");assert(find(lv_scr_act(),"Scan to start a conversation."));click("Home");
 click("ATLA");assert(find(lv_scr_act(),"ATLA Prints"));click("< Back");
 check_constellation();
 click("hello");assert(find(lv_scr_act(),"Scan to start a conversation."));check_bounds(lv_scr_act());check_step(0);shot("hello-opening.ppm");lv_obj_t *activity=loading_segment();assert(activity);int initial_x=lv_obj_get_x(activity);tick(400);assert(lv_obj_get_x(activity)!=initial_x);shot("hello-loading-mid.ppm");test_hello_stage(PET_HELLO_CLOCK);tick(500);assert(find(lv_scr_act(),"SYNCING CLOCK"));assert(loading_segment()==activity);check_bounds(lv_scr_act());shot("hello-clock.ppm");test_hello_stage(PET_HELLO_RETRY);tick(500);assert(find(lv_scr_act(),"TRYING AGAIN"));test_hello_offline();tick(500);check_step(0);check_bounds(lv_scr_act());shot("hello-offline.ppm");click("Home");tick(500);assert(find(lv_scr_act(),"ANDRE\nWEISS."));
 click("hello");test_hello_opened();tick(500);assert(find(lv_scr_act(),"Scan to start a conversation."));check_step(0);check_bounds(lv_scr_act());shot("hello-join.ppm");for(unsigned g=0;g<9;g++){for(unsigned elapsed=0;elapsed<12000&&!find(lv_scr_act(),greetings[g]);elapsed+=100)tick(100);lv_obj_t *word=find(lv_scr_act(),greetings[g]);assert(word);assert(lv_obj_get_style_text_color(word,0).full==lv_color_hex(0xbfd6b8).full);check_bounds(lv_scr_act());}test_phone_joined(true);tick(500);assert(find(lv_scr_act(),"Scan to start a conversation."));shot("hello-joined.ppm");test_form_opened();tick(500);assert(find(lv_scr_act(),"Your turn."));check_step(1);check_bounds(lv_scr_act());shot("hello-form.ppm");test_phone_joined(false);tick(500);assert(find(lv_scr_act(),"Scan to start a conversation."));test_phone_joined(true);test_form_opened();tick(500);
 pet_services_hello_cancel();tick(500);assert(find(lv_scr_act(),"Try again."));check_step(1);check_bounds(lv_scr_act());shot("hello-expired.ppm");assert(!pet_services_accept());click("Start again");test_hello_opened();test_phone_joined(true);test_form_opened();tick(500);
 test_introduction();tick(500);test_link_closed();tick(500);assert(find(lv_scr_act(),"Zoë García"));check_bounds(lv_scr_act());check_step(2);check_guest_layout();shot("guest-name.ppm");test_card_updated();tick(500);check_bounds(lv_scr_act());shot("card-updated.ppm");test_long_introduction();tick(500);check_bounds(lv_scr_act());check_guest_layout();shot("guest-long.ppm");test_introduction();tick(500);click("Clear tag");tick(500);check_bounds(lv_scr_act());shot("card-clear.ppm");test_card_cleared();tick(500);check_step(2);shot("card-cleared.ppm");click("Prepare tag");tick(500);test_card_found();tick(500);assert(find(lv_scr_act(),"Save to card"));assert(!find(lv_scr_act(),"Done"));check_bounds(lv_scr_act());shot("card-review.ppm");click("Save to card");tick(500);check_bounds(lv_scr_act());click("Done");pet_status_t status;pet_services_snapshot(&status);assert(status.connections==1);assert(!pet_services_accept());
 assert(find(lv_scr_act(),"ANDRE\nWEISS."));pet_ui_show(5);tick(500);click("Wi-Fi");tick(500);
 lv_obj_t *networks=lv_obj_get_parent(lv_obj_get_parent(find(lv_scr_act(),"Studio")));
 assert(lv_obj_get_scroll_bottom(networks)>0);lv_obj_scroll_to_y(networks,1000,LV_ANIM_OFF);tick(50);assert(lv_obj_get_scroll_y(networks)>0);
 lv_area_t viewport,last;lv_obj_get_coords(networks,&viewport);lv_obj_get_coords(find(lv_scr_act(),"Library"),&last);assert(last.y1>=viewport.y1&&last.y2<=viewport.y2);
 shot("wifi-scrolled.ppm");click("Library");assert(find(lv_scr_act(),"Connect to Wi-Fi"));click("< Back");click("Studio");check_bounds(lv_scr_act());click("< Back");assert(find(lv_scr_act(),"Studio"));click("eduroam");check_bounds(lv_scr_act());
 lv_obj_t *user=NULL,*pass=NULL,*ssid=NULL;
 for(unsigned i=0;i<lv_obj_get_child_cnt(lv_scr_act());i++){lv_obj_t *o=lv_obj_get_child(lv_scr_act(),i);if(lv_obj_check_type(o,&lv_textarea_class)){if(lv_textarea_get_password_mode(o))pass=o;else if(!strcmp(lv_textarea_get_text(o),"eduroam"))ssid=o;else user=o;}}
 assert(user&&pass&&ssid&&!lv_obj_has_flag(user,LV_OBJ_FLAG_HIDDEN));
 lv_textarea_set_text(user,"student@school.edu");lv_textarea_set_text(pass,"test-secret");
 click(LV_SYMBOL_EYE_OPEN);assert(!lv_textarea_get_password_mode(pass));assert(!strcmp(lv_textarea_get_text(pass),"test-secret"));click(LV_SYMBOL_EYE_CLOSE);assert(lv_textarea_get_password_mode(pass));
 click("Username + password");assert(lv_obj_has_flag(user,LV_OBJ_FLAG_HIDDEN));click("Password only");assert(!lv_obj_has_flag(user,LV_OBJ_FLAG_HIDDEN));assert(!strcmp(lv_textarea_get_text(user),"student@school.edu"));shot("wifi-enterprise.ppm");
 click("Connect");tick(500);lv_obj_t *active=lv_obj_get_parent(find(lv_scr_act(),"eduroam"));assert(lv_obj_get_y(active)==228);assert(lv_obj_get_style_bg_color(active,0).full==lv_color_hex(0xbfd6b8).full);assert(find(lv_scr_act(),"Manage >"));check_bounds(lv_scr_act());shot("wifi-connected.ppm");
 click("eduroam");assert(find(lv_scr_act(),"Forget this network"));check_bounds(lv_scr_act());shot("wifi-details.ppm");click("< Back");pet_services_snapshot(&status);assert(status.wifi_connected);click("eduroam");click("Forget this network");tick(500);pet_services_snapshot(&status);assert(!status.wifi_connected&&!status.ssid[0]);
 click("Manual entry");for(unsigned i=0;i<lv_obj_get_child_cnt(lv_scr_act());i++){lv_obj_t *o=lv_obj_get_child(lv_scr_act(),i);if(lv_obj_check_type(o,&lv_textarea_class))assert(!lv_textarea_get_text(o)[0]);}click("< Back");click("< Back");assert(find(lv_scr_act(),"Settings"));
 lv_obj_t *slider=NULL;for(unsigned i=0;i<lv_obj_get_child_cnt(lv_scr_act());i++){lv_obj_t *child=lv_obj_get_child(lv_scr_act(),i);if(lv_obj_check_type(child,&lv_slider_class))slider=child;}assert(slider);
 /* During a drag, values and geometry change without committing brightness. */
 for(int v=0;v<=100;v++){
  lv_slider_set_value(slider,v,LV_ANIM_ON);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);tick(10);
  int expected=v<20?20:v;char percentage[16];snprintf(percentage,sizeof(percentage),"%d%%",expected);assert(find(lv_scr_act(),percentage));
  assert(lv_slider_get_value(slider)==expected);pet_services_snapshot(&status);assert(status.brightness==100);
  lv_refr_now(NULL);lv_area_t track,knob=((lv_slider_t*)slider)->right_knob_area;lv_obj_get_coords(slider,&track);assert(lv_obj_get_style_anim_time(slider,0)==0);
  assert(knob.x1>=track.x1&&knob.x2<=track.x1+552*expected/100-1&&knob.y1>=track.y1&&knob.y2<=track.y2);
  if(v==52)shot("settings-brightness-52.ppm");
 }
 lv_slider_set_value(slider,20,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);lv_event_send(slider,LV_EVENT_RELEASED,NULL);tick(250);pet_services_snapshot(&status);assert(status.brightness==20);
 lv_slider_set_value(slider,100,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);lv_event_send(slider,LV_EVENT_RELEASED,NULL);tick(250);pet_services_snapshot(&status);assert(status.brightness==100);
 click("Motion");pet_services_snapshot(&status);assert(!status.motion);assert(find(lv_scr_act(),"Off"));click("Motion");click("Sleep");tick(500);assert(pet_ui_touch_wake(true));tick(500);assert(pet_ui_touch_wake(false));assert(!pet_ui_touch_wake(false));
 for(unsigned state=0;state<8;state++){pet_ui_show(state);tick(500);check_bounds(lv_scr_act());char path[32];snprintf(path,sizeof(path),"v6-state-%u.ppm",state);shot(path);}
 for(int n=0;n<100;n++){pet_ui_show(n%10);tick(50);}
 puts("PASS: v6 layouts, 4 spreads, phone join/form/disconnect states, project/back navigation, live-event UI, single acceptance, wake-touch consumption, 100 transitions");return 0;
}
