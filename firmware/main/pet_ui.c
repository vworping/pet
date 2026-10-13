#include "pet_ui.h"
#include "pet_constellation.h"
#include "pet_services.h"
#include "pet_qr.h"
#include "images/pet_images.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "board/waveshare_rgb_lcd_port.h"
#endif
LV_FONT_DECLARE(pet_archivo_104);
LV_FONT_DECLARE(pet_archivo_192);
LV_FONT_DECLARE(pet_archivo_72);
LV_FONT_DECLARE(pet_archivo_32);
LV_FONT_DECLARE(pet_dm_18);
LV_FONT_DECLARE(pet_dm_24);
LV_FONT_DECLARE(pet_dm_28);
LV_FONT_DECLARE(pet_dm_36);
LV_FONT_DECLARE(pet_greeting_96);
LV_FONT_DECLARE(pet_greeting_128);
#define BG 0x111316
#define PANEL 0x1c1e23
#define GREEN 0xbfd6b8
#define INK 0xf1f0eb
#define MUTED 0xa7acb3
#define LINE 0x34373d
#define PINK 0xd5a4bd
#define CYAN 0x59c9e8
enum { HOME, PET, ATLA, CONSTELLATION, HELLO, SETTINGS, WIFI, PASSWORD, NETWORK, COUNT };
enum { BACK=30, OPEN_HELLO, NEXT_QR, ACCEPT, WIFI_SCAN, WIFI_CONNECT, WIFI_OFF, WIFI_FORGET, SLEEP, MOTION, RESTART_HELLO, PREV_QR, CARD_ARM, CARD_SAVE, CARD_CLEAR, PASSWORD_EYE, ENTERPRISE_TOGGLE, NETWORK_EDIT, SPREAD_BASE=60, NET_BASE=100 };
static unsigned current,pending,phase,spread;
static unsigned settings_return;
static bool queued,refresh,was_asleep,wake_release;
static pet_status_t status;
static lv_obj_t *screen,*header_status,*dim_overlay,*password,*ssid_input,*brightness_value,*username,*join_hint,*join_keyboard;
static bool enterprise;
static lv_obj_t *loading_caption,*hello_hint;
static char chosen_ssid[33];
static unsigned last_revision;
static void render(void);
static void show(unsigned state);
static lv_obj_t *label(const char *s,int x,int y,int w,const lv_font_t *font,uint32_t color){
 lv_obj_t *o=lv_label_create(screen);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);lv_obj_set_style_text_line_space(o,0,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;
}
static lv_obj_t *box(int x,int y,int w,int h,uint32_t color){
 lv_obj_t *o=lv_obj_create(screen);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
static unsigned parent(void){if(current==PASSWORD||current==NETWORK)return WIFI;if(current==WIFI)return SETTINGS;if(current==SETTINGS)return settings_return;return HOME;}
static void action(lv_event_t *e){
 unsigned a=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 if(a>=SPREAD_BASE&&a<SPREAD_BASE+PET_SPREAD_COUNT){spread=a-SPREAD_BASE;show(CONSTELLATION);return;}
 if(a>=NET_BASE){unsigned i=a-NET_BASE;if(i<status.network_count){strlcpy(chosen_ssid,status.networks[i].ssid,33);enterprise=status.networks[i].enterprise;show(!strcmp(chosen_ssid,status.ssid)?NETWORK:PASSWORD);}return;}
 switch(a){
 case BACK:if(current==HELLO){pet_services_hello_cancel();phase=0;}show(parent());break;
 case OPEN_HELLO:case RESTART_HELLO:phase=0;pet_services_hello_start();show(HELLO);break;
 case NEXT_QR:phase=1;show(HELLO);break;
 case PREV_QR:phase=2;show(HELLO);break;
 case HOME:if(status.hello_starting||status.hello_active||status.introduction_ready)pet_services_hello_cancel();show(HOME);break;
 case SETTINGS:if(current!=SETTINGS&&current!=WIFI&&current!=PASSWORD&&current!=NETWORK)settings_return=current;show(SETTINGS);break;
 case CARD_ARM:pet_services_card_arm();break;
 case CARD_SAVE:pet_services_card_save();break;
 case CARD_CLEAR:pet_services_card_clear();break;
 case ACCEPT:if(pet_services_accept()){show(HOME);}break;
 case WIFI_SCAN:pet_services_scan();break;
 case PASSWORD_EYE:{bool hide=!lv_textarea_get_password_mode(password);lv_textarea_set_password_mode(password,hide);lv_label_set_text(lv_obj_get_child(lv_event_get_target(e),0),hide?LV_SYMBOL_EYE_OPEN:LV_SYMBOL_EYE_CLOSE);break;}
 case ENTERPRISE_TOGGLE:enterprise=!enterprise;lv_keyboard_set_textarea(join_keyboard,enterprise?username:password); if(enterprise)lv_obj_clear_flag(username,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(username,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(lv_obj_get_child(lv_event_get_target(e),0),enterprise?"Username + password":"Password only");break;
 case NETWORK_EDIT:show(PASSWORD);break;
 case NETWORK:enterprise=status.wifi_enterprise;strlcpy(chosen_ssid,status.ssid,33);show(NETWORK);break;
 case PASSWORD:chosen_ssid[0]=0;enterprise=false;show(PASSWORD);break;
 case WIFI_CONNECT:
  if(!lv_textarea_get_text(ssid_input)[0]||(enterprise&&(!lv_textarea_get_text(username)[0]||!lv_textarea_get_text(password)[0]))){lv_label_set_text(join_hint,"Enter the network name and required credentials.");break;}
  if(enterprise)pet_services_connect_enterprise(lv_textarea_get_text(ssid_input),lv_textarea_get_text(username),lv_textarea_get_text(password));else pet_services_connect(lv_textarea_get_text(ssid_input),lv_textarea_get_text(password));show(WIFI);break;
 case WIFI_OFF:pet_services_wifi_off();break;
 case WIFI_FORGET:pet_services_forget_wifi();show(WIFI);break;
 case SLEEP:pet_services_sleep(true);break;
 case MOTION:pet_services_set_motion(!status.motion);show(SETTINGS);break;
 default:if(a==WIFI)pet_services_scan();show(a);break;
 }
}
static lv_obj_t *button(const char *s,int x,int y,int w,int h,unsigned action_id,bool fill){
 lv_obj_t *b=lv_btn_create(screen);lv_obj_remove_style_all(b);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);lv_obj_set_style_radius(b,12,0);lv_obj_set_style_bg_color(b,lv_color_hex(fill?GREEN:PANEL),0);lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);lv_obj_set_style_bg_color(b,lv_color_hex(0x4d5a51),LV_STATE_PRESSED);
 lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_set_style_text_font(l,&pet_dm_24,0);lv_obj_set_style_text_color(l,lv_color_hex(fill?BG:INK),0);lv_obj_center(l);lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void*)(uintptr_t)action_id);return b;
}
static void settings_control(const char *name,const char *value,int y,unsigned id,bool fill){
 lv_obj_t *b=button(name,32,y,320,92,id,fill);
 lv_obj_align(lv_obj_get_child(b,0),LV_ALIGN_LEFT_MID,24,0);
 lv_obj_t *v=lv_label_create(b);lv_label_set_text(v,value);
 lv_obj_set_style_text_font(v,&pet_dm_24,0);lv_obj_set_style_text_color(v,lv_color_hex(fill?BG:INK),0);
 lv_obj_align(v,LV_ALIGN_RIGHT_MID,-24,0);
}
static void line(int y){box(32,y,960,1,LINE);}
static lv_obj_t *panel(int x,int y,int w,int h){lv_obj_t *o=box(x,y,w,h,PANEL);lv_obj_set_style_radius(o,18,0);return o;}
static void small_button(const char *s,int x,int y,int w,int h,unsigned id,bool fill){lv_obj_t *b=button(s,x,y,w,h,id,fill);lv_obj_set_style_text_font(lv_obj_get_child(b,0),&pet_dm_18,0);}
static void update_header(void){
 if(header_status)lv_label_set_text(header_status,status.hello_starting?"Opening link":status.hello_active?(status.phone_joined?"Phone linked":"Phone link"):(status.wifi_connected?"Wi-Fi on":"Wi-Fi off"));
}
static void header(const char *title){
 const char *names[]={"","BUILDING / PET","BUILDING / ATLA","COLOR STUDIES","A SMALL HELLO","SETTINGS","WI-FI","JOIN NETWORK","NETWORK DETAILS"};
 (void)title;
 small_button(current==HOME?"[pet.]":"< Back",32,16,104,44,current==HOME?HOME:BACK,false);
 if(current!=HOME)small_button("Home",144,16,84,44,HOME,false);
 if(current!=HOME)label(names[current],252,29,440,&pet_dm_18,MUTED);
 header_status=label("",704,29,160,&pet_dm_18,MUTED);lv_obj_set_style_text_align(header_status,LV_TEXT_ALIGN_RIGHT,0);update_header();
 small_button("Settings",880,16,112,44,SETTINGS,false);line(76);
}
static void img(const lv_img_dsc_t *src,int x,int y){lv_obj_t *o=lv_img_create(screen);lv_img_set_src(o,src);lv_obj_set_pos(o,x,y);}

/* Portfolio brackets, with a shared animated width for the word and link rule. */
static lv_timer_t *greeting_timer;
static lv_obj_t *greeting_word,*greeting_left,*greeting_right,*greeting_rule;
static unsigned greeting_index,greeting_hold;
static bool greeting_erasing,greeting_on_hello;
static char greeting_text[16];
static const char *greetings[]={"hello","hi","hey","howdy","hola","buenas","salut","bonjour","coucou"};
static const uint32_t greeting_colors[]={GREEN,0xe8b5ce,0xe8bf88,0xaacbdc,0xd6c2ed,0xe6ad94,0xbfd6b8,0xaacbdc,0xe8b5ce};
#define GREETING_COUNT (sizeof(greetings)/sizeof(greetings[0]))
static void greeting_color(unsigned index){
 lv_color_t color=lv_color_hex(greeting_on_hello?GREEN:greeting_colors[index]);
 lv_obj_set_style_text_color(greeting_word,color,0);
 lv_obj_set_style_text_color(greeting_left,color,0);
 lv_obj_set_style_text_color(greeting_right,color,0);
 lv_obj_set_style_bg_color(greeting_rule,color,0);
}
static void greeting_width(void *obj,int32_t width){
 lv_obj_set_width(obj,width>0?width:1);
 int bracket=greeting_on_hello?32:24;
 int left=greeting_on_hello?0:(420-width-44)/2;
 int top=greeting_on_hello?0:28;
 lv_obj_set_pos(greeting_left,left,top);
 lv_obj_set_pos(obj,left+bracket,top);
 lv_obj_set_pos(greeting_right,left+bracket+width-2,top);
 /* Match the underline to the visible bracket edges, not glyph advance widths. */
 lv_font_glyph_dsc_t opening,closing;
 lv_font_get_glyph_dsc(greeting_on_hello?&pet_greeting_128:&pet_greeting_96,&opening,'[',0);
 lv_font_get_glyph_dsc(greeting_on_hello?&pet_greeting_128:&pet_greeting_96,&closing,']',0);
 int rule_left=left+opening.ofs_x;
 int rule_right=left+bracket+width-2+closing.ofs_x+closing.box_w;
 lv_obj_set_pos(greeting_rule,rule_left,top+(greeting_on_hello?148:111));
 lv_obj_set_width(greeting_rule,rule_right-rule_left);
}
static void greeting_tick(lv_timer_t *timer){
 (void)timer;
 if(status.asleep||!status.motion||status.save_power)return;
 if(greeting_hold){greeting_hold--;return;}
 size_t n=strlen(greeting_text),prefix=0;
 unsigned next=(greeting_index+1)%GREETING_COUNT;
 const char *target=greetings[next];
 while(greeting_text[prefix]&&target[prefix]&&greeting_text[prefix]==target[prefix])prefix++;
 if(greeting_erasing&&n>prefix)greeting_text[n-1]=0;
 else {greeting_erasing=false;greeting_color(next);greeting_text[n]=target[n];greeting_text[n+1]=0;}
 int before=lv_obj_get_width(greeting_word);
 lv_label_set_text(greeting_word,greeting_text);
 lv_point_t size;lv_txt_get_size(&size,greeting_text,greeting_on_hello?&pet_greeting_128:&pet_greeting_96,-3,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,greeting_word);lv_anim_set_exec_cb(&a,greeting_width);lv_anim_set_values(&a,before,size.x);lv_anim_set_time(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);
 if(!strcmp(greeting_text,target)){greeting_index=next;greeting_hold=10;greeting_erasing=true;}
}
static void greeting(void){
 /* The stable hit area includes the text, moving brackets, and underline. */
 greeting_on_hello=current==HELLO;
 lv_obj_t *b=button("hello",greeting_on_hello?32:562,greeting_on_hello?168:183,greeting_on_hello?592:420,greeting_on_hello?160:214,OPEN_HELLO,false);
 if(greeting_on_hello)lv_obj_clear_flag(b,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0);
 lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,LV_STATE_PRESSED);
 greeting_word=lv_obj_get_child(b,0);lv_obj_set_align(greeting_word,LV_ALIGN_TOP_LEFT);
 greeting_left=lv_label_create(b);greeting_right=lv_label_create(b);
 lv_label_set_text(greeting_left,"[");lv_label_set_text(greeting_right,"]");
 lv_obj_t *parts[]={greeting_word,greeting_left,greeting_right};
 for(unsigned i=0;i<3;i++){lv_obj_set_style_text_font(parts[i],greeting_on_hello?&pet_greeting_128:&pet_greeting_96,0);lv_obj_set_style_text_letter_space(parts[i],-3,0);lv_obj_clear_flag(parts[i],LV_OBJ_FLAG_CLICKABLE);}
 greeting_rule=lv_obj_create(b);lv_obj_remove_style_all(greeting_rule);lv_obj_set_height(greeting_rule,3);lv_obj_set_style_bg_opa(greeting_rule,LV_OPA_COVER,0);lv_obj_clear_flag(greeting_rule,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
 lv_label_set_long_mode(greeting_word,LV_LABEL_LONG_CLIP);
 lv_point_t size;lv_txt_get_size(&size,"hello",greeting_on_hello?&pet_greeting_128:&pet_greeting_96,-3,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 greeting_width(greeting_word,size.x);greeting_color(0);
 strcpy(greeting_text,"hello");greeting_index=0;greeting_hold=10;greeting_erasing=true;
 if(status.motion&&!status.save_power)greeting_timer=lv_timer_create(greeting_tick,210,NULL);
}
/* Shared Hello grid: OS steps, left conversation, right paper interaction. */
static void hello_steps(unsigned step){
 const char *steps[]={"01 / INTRODUCTION","02 / SAY HELLO","03 / CONNECT"};
 for(unsigned i=0;i<3;i++){
  int x=32+i*328;bool active=i==step;
  if(active)box(x,96,304,36,GREEN);
  label(steps[i],x+10,102,284,&pet_dm_18,active?BG:MUTED);
  box(x,140,304,2,active?GREEN:LINE);
 }

}
static lv_obj_t *hello_button(const char *text,int x,int y,int width,int height,unsigned id,bool fill){
 lv_obj_t *b=button(text,x,y,width,height,id,fill);
 lv_obj_t *text_obj=lv_obj_get_child(b,0);lv_obj_align(text_obj,LV_ALIGN_LEFT_MID,16,0);
 if(!fill){lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(text_obj,lv_color_hex(0x10232b),0);}
 return b;
}
static lv_obj_t *hello_info(const char *text,int x,int y,int width,const lv_font_t *font,uint32_t color){
 lv_obj_t *o=label(text,x,y,width,font,color);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_set_height(o,font->line_height);return o;
}
static void hello_sheet(void){lv_obj_t *paper=box(656,168,336,336,0xf3eee4);lv_obj_set_style_radius(paper,8,0);}
static void loading_position(void *obj,int32_t x){lv_obj_set_x(obj,x);}
static const char *loading_stage(void){
 return status.hello_stage==PET_HELLO_WIFI?"WAITING FOR WI-FI":status.hello_stage==PET_HELLO_CLOCK?"SYNCING CLOCK":status.hello_stage==PET_HELLO_RETRY?"TRYING AGAIN":"OPENING YOUR LINK";
}
static void hello_qr(bool opening){
 hello_sheet();
 if(!opening){
  pet_qr_create(screen,status.hello_url,656,168,336);
  hello_info(status.form_opened?"Continue on your phone.":"Scan with your camera.",656,536,336,&pet_dm_18,MUTED);return;
 }
 hello_info("Opening link",680,244,288,&pet_archivo_32,0x10232b);
 box(680,420,288,4,0xd4d3c9);lv_obj_t *activity=box(680,420,72,4,0x477e73);
 if(status.motion&&!status.save_power){
  lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,activity);lv_anim_set_exec_cb(&a,loading_position);
  lv_anim_set_values(&a,680,896);lv_anim_set_time(&a,1400);lv_anim_set_playback_time(&a,1400);
  lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE);lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);lv_anim_start(&a);
 }
 loading_caption=hello_info(loading_stage(),656,536,336,&pet_dm_18,MUTED);
}
/* Draw the inset knob in the same pass as the fill, using its actual edge. */
static void brightness_knob(lv_event_t *e){
 lv_obj_draw_part_dsc_t *d=lv_event_get_draw_part_dsc(e);
 if(d->part==LV_PART_KNOB){d->draw_area->x1-=24;d->draw_area->x2-=24;}
}
/* Dragging only repaints the small control. Flash persistence and the full-screen
   dim overlay are applied once on release, outside the continuous drag path. */
static void brightness(lv_event_t *e){
 lv_obj_t *slider=lv_event_get_target(e);
 int v=lv_slider_get_value(slider);if(v<20){v=20;lv_slider_set_value(slider,v,LV_ANIM_OFF);}
 char s[16];snprintf(s,sizeof(s),"%d%%",v);lv_label_set_text(brightness_value,s);
 if(lv_event_get_code(e)==LV_EVENT_RELEASED){
  pet_services_set_brightness(v);
  if(dim_overlay)lv_obj_set_style_bg_opa(dim_overlay,(100-v)*2,0);
 }
}
static void focus_input(lv_event_t *e){lv_obj_t *kb=lv_event_get_user_data(e);lv_keyboard_set_textarea(kb,lv_event_get_target(e));}
static void render(void){
 if(greeting_timer){lv_timer_del(greeting_timer);greeting_timer=NULL;}
 lv_obj_clean(screen);loading_caption=NULL;hello_hint=NULL;header_status=NULL;dim_overlay=NULL;password=NULL;ssid_input=NULL;username=NULL;join_hint=NULL;join_keyboard=NULL;brightness_value=NULL;
 pet_services_snapshot(&status);last_revision=status.revision;header("");
 ESP_LOGI("pet_ui","v6 page=%u",current);
 if(current==HOME){
  lv_obj_t *n=label("ANDRE\nWEISS.",54,141,650,&pet_archivo_104,INK);lv_obj_set_style_text_line_space(n,-4,0);
  label("Photographer x researcher",54,374,650,&pet_dm_28,MUTED);
  greeting();
  lv_obj_t *building=label("CURRENTLY BUILDING",32,453,250,&pet_dm_18,MUTED);
  lv_obj_set_width(building,LV_SIZE_CONTENT);lv_obj_update_layout(building);
  int rule_x=32+lv_obj_get_width(building)+12;
  box(rule_x,464,992-rule_x,1,LINE);
  button("[pet.]",32,491,296,72,PET,false);button("ATLA",344,491,296,72,ATLA,false);button("Constellation",656,491,336,72,CONSTELLATION,false);
 }else if(current==PET){
  label("[pet.]",32,144,586,&pet_archivo_192,INK);
  label("A wearable introduction.",40,394,578,&pet_dm_36,MUTED);
  panel(650,110,342,460);label("Inside [pet.]",674,139,294,&pet_archivo_32,INK);box(674,193,294,1,LINE);
  label("DISPLAY + BRAIN",674,210,294,&pet_dm_18,GREEN);
  lv_obj_t *display=label("5-inch Waveshare touch\nESP32-S3 / 1024 x 600",674,241,294,&pet_dm_24,INK);lv_obj_set_style_text_line_space(display,6,0);box(674,316,294,1,LINE);
  label("NFC / RFID",674,326,294,&pet_dm_18,GREEN);
  lv_obj_t *reader=label("NULLLAB RC522 / I2C\n13.56 MHz card reader",674,357,294,&pet_dm_24,INK);lv_obj_set_style_text_line_space(reader,6,0);box(674,432,294,1,LINE);
  label("POWER",674,442,294,&pet_dm_18,GREEN);
  lv_obj_t *power=label("3.7V / 1000mAh LiPo\nRechargeable battery",674,473,294,&pet_dm_24,INK);lv_obj_set_style_text_line_space(power,6,0);
  button("Open Hello >",40,494,286,72,OPEN_HELLO,true);
 }else if(current==ATLA){
  img(&pet_img_atla,144,210);
  lv_obj_t *prompt=label("Ask me about...",552,225,424,&pet_dm_28,MUTED);
  lv_obj_set_style_text_align(prompt,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_t *stage=label("FIRST PRODUCT / PRE-SEED",552,272,424,&pet_dm_18,0xe18a4e);
  lv_obj_set_style_text_align(stage,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_t *ear=label("EAR",552,310,424,&pet_archivo_104,INK);
  lv_obj_set_style_text_align(ear,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_t *prints=label("ATLA Prints",552,416,424,&pet_dm_36,INK);
  lv_obj_set_style_text_align(prints,LV_TEXT_ALIGN_CENTER,0);
 }else if(current==CONSTELLATION){
  pet_constellation_create(screen,status.motion&&!status.save_power);
 }else if(current==HELLO){
  unsigned step=status.introduction_ready?2:status.hello_starting?0:(!status.hello_active||status.form_opened)?1:0;
  hello_steps(step);
 if(status.introduction_ready){
   hello_info("NICE TO MEET YOU",32,178,592,&pet_dm_18,GREEN);
   lv_point_t size;lv_txt_get_size(&size,status.guest,&pet_archivo_72,0,0,592,LV_TEXT_FLAG_NONE);
   label(status.guest,32,214,592,size.y<=128?&pet_archivo_72:&pet_dm_28,INK);
   box(32,352,592,1,LINE);
   hello_info("CONVERSATION STARTER",32,374,592,&pet_dm_18,MUTED);
   const lv_font_t *topic_font=&pet_dm_28;
   lv_txt_get_size(&size,status.interest,topic_font,0,0,592,LV_TEXT_FLAG_NONE);
   if(size.y>104){topic_font=&pet_dm_24;lv_txt_get_size(&size,status.interest,topic_font,0,0,592,LV_TEXT_FLAG_NONE);}
   if(size.y>104)topic_font=&pet_dm_18;
   label(status.interest,32,414,592,topic_font,INK);
   bool card_actions=status.card_stage==0||status.card_stage==5||status.card_stage==7;
   bool review=status.card_stage==2,saved=status.card_stage==4;
   lv_obj_t *paper=box(656,168,336,340,0xf3eee4);lv_obj_set_style_radius(paper,8,0);
   hello_info(saved?"Card updated.":"NFC card",680,196,288,&pet_archivo_32,0x10232b);
   hello_info(saved?"Ready to share.":review?"Confirm the card below.":"Save this introduction.",680,258,288,&pet_dm_18,0x10232b);
   if(card_actions){
    lv_obj_t *card=hello_button("Prepare tag",680,364,164,48,CARD_ARM,false);
    lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x477e73),0);
    lv_obj_t *clear=hello_button("Clear tag",856,364,112,48,CARD_CLEAR,false);lv_obj_set_style_text_font(lv_obj_get_child(clear,0),&pet_dm_18,0);
   }else if(review){char short_id[60];snprintf(short_id,sizeof(short_id),"Card %.8s",status.card_id);hello_info(short_id,680,376,288,&pet_dm_18,0x10232b);}
   hello_button(review?"Save to card":"Done",680,428,288,56,review?CARD_SAVE:ACCEPT,true);
   box(32,524,960,1,LINE);
   const char *card_hint=status.card_stage==0?"Prepare a tag to save this introduction, or select Done.":status.card_status;
   hello_info(card_hint,32,544,960,&pet_dm_18,saved?GREEN:MUTED);
  }else if(!status.hello_active&&!status.hello_starting){
   label("Try again.",32,190,592,&pet_archivo_72,INK);
   hello_info("This introduction has ended.",32,336,592,&pet_dm_28,INK);
   hello_info(status.hello_error[0]?status.hello_error:"Open a new link when you are ready.",32,408,592,&pet_dm_24,MUTED);
   hello_sheet();hello_info("New hello",680,208,288,&pet_archivo_32,0x10232b);
   hello_button("Start again",680,420,288,56,RESTART_HELLO,true);
  }else{
   bool form=status.form_opened&&!status.hello_starting;
   if(form){
    label("Your turn.",32,188,592,&pet_archivo_72,INK);box(32,300,592,1,LINE);
    hello_info("NAME",32,328,592,&pet_dm_18,MUTED);hello_info("Introduce yourself on your phone.",32,364,592,&pet_dm_28,INK);
    box(32,418,592,1,LINE);hello_info("TOPIC",32,440,592,&pet_dm_18,MUTED);hello_info("What would you like to talk about?",32,476,592,&pet_dm_28,INK);
   }else{
    greeting();hello_info("Scan to start a conversation.",32,400,592,&pet_dm_28,INK);
    hello_hint=hello_info(status.hello_error[0]?status.hello_error:"No app or sign-in needed.",32,452,592,&pet_dm_18,MUTED);
   }
   hello_qr(status.hello_starting);
  }
 }else if(current==SETTINGS){
  label("Settings",32,110,950,&pet_archivo_72,INK);
  settings_control("Wi-Fi",">",228,WIFI,false);
  settings_control("Motion",status.motion?"On":"Off",336,MOTION,status.motion);
  settings_control("Sleep","",444,SLEEP,false);
  label("Brightness",424,290,360,&pet_dm_36,INK);
  char s[32];snprintf(s,sizeof(s),"%d%%",status.brightness);
  brightness_value=label(s,828,296,148,&pet_dm_28,GREEN);
  lv_obj_set_style_text_align(brightness_value,LV_TEXT_ALIGN_RIGHT,0);
  lv_obj_t *slider=lv_slider_create(screen);lv_obj_remove_style_all(slider);
  lv_obj_set_pos(slider,424,360);lv_obj_set_size(slider,552,48);
  lv_slider_set_range(slider,0,100);lv_slider_set_value(slider,status.brightness,LV_ANIM_OFF);
  lv_obj_set_ext_click_area(slider,16);
  lv_obj_set_style_radius(slider,24,0);lv_obj_set_style_bg_color(slider,lv_color_hex(PANEL),0);lv_obj_set_style_bg_opa(slider,LV_OPA_COVER,0);
  lv_obj_set_style_radius(slider,24,LV_PART_INDICATOR);lv_obj_set_style_bg_color(slider,lv_color_hex(GREEN),LV_PART_INDICATOR);lv_obj_set_style_bg_opa(slider,LV_OPA_COVER,LV_PART_INDICATOR);
  lv_obj_set_style_anim_time(slider,0,0);
  lv_obj_set_style_bg_color(slider,lv_color_hex(INK),LV_PART_KNOB);lv_obj_set_style_bg_opa(slider,LV_OPA_COVER,LV_PART_KNOB);
  lv_obj_set_style_radius(slider,16,LV_PART_KNOB);lv_obj_set_style_pad_all(slider,-8,LV_PART_KNOB);
  lv_obj_add_event_cb(slider,brightness_knob,LV_EVENT_DRAW_PART_BEGIN,NULL);
  lv_obj_add_event_cb(slider,brightness,LV_EVENT_VALUE_CHANGED,NULL);
  lv_obj_add_event_cb(slider,brightness,LV_EVENT_RELEASED,NULL);
  snprintf(s,sizeof(s),"%u",status.connections);
  lv_obj_t *count=label(s,720,450,256,&pet_archivo_72,GREEN);lv_obj_set_style_text_align(count,LV_TEXT_ALIGN_RIGHT,0);
  lv_obj_t *caption=label("Hellos",720,530,256,&pet_dm_24,INK);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_RIGHT,0);
 }else if(current==WIFI){
  label("Wi-Fi",32,110,590,&pet_archivo_72,INK);
  lv_obj_t *connection=label(status.wifi_status,640,144,336,&pet_dm_24,GREEN);lv_obj_set_style_text_align(connection,LV_TEXT_ALIGN_RIGHT,0);
  label(status.service_status,32,192,944,&pet_dm_18,MUTED);
  settings_control(status.scanning?"Scanning...":"Scan","",228,WIFI_SCAN,false);
  settings_control("Manual entry","",336,PASSWORD,false);
  settings_control("Wi-Fi off","",444,WIFI_OFF,false);
  bool connected=status.wifi_connected&&status.ssid[0];
  if(connected){
   lv_obj_t *b=button(status.ssid,424,228,536,92,NETWORK,true);
   lv_obj_t *name=lv_obj_get_child(b,0);lv_obj_set_width(name,286);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_align(name,LV_ALIGN_LEFT_MID,24,0);
   lv_obj_t *caption=lv_label_create(b);lv_label_set_text(caption,"Manage >");lv_obj_set_style_text_font(caption,&pet_dm_18,0);lv_obj_set_style_text_color(caption,lv_color_hex(BG),0);lv_obj_align(caption,LV_ALIGN_RIGHT_MID,-24,0);
   lv_obj_t *band=lv_label_create(b);lv_label_set_text(band,"2.4 GHz");lv_obj_set_style_text_font(band,&pet_dm_18,0);lv_obj_set_style_text_color(band,lv_color_hex(BG),0);lv_obj_align(band,LV_ALIGN_RIGHT_MID,-132,0);
  }
  int list_y=connected?336:228;
  lv_obj_t *networks=lv_obj_create(screen);lv_obj_remove_style_all(networks);lv_obj_set_pos(networks,424,list_y);lv_obj_set_size(networks,552,536-list_y);
  lv_obj_set_scroll_dir(networks,LV_DIR_VER);lv_obj_set_scrollbar_mode(networks,LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(networks,lv_color_hex(GREEN),LV_PART_SCROLLBAR);lv_obj_set_style_bg_opa(networks,LV_OPA_COVER,LV_PART_SCROLLBAR);lv_obj_set_style_width(networks,4,LV_PART_SCROLLBAR);
  if(!status.network_count)label(status.scanning?"Scanning...":"No networks found",448,list_y+24,504,&pet_dm_24,MUTED);
  unsigned row=0;
  for(unsigned i=0;i<status.network_count;i++){
   if(connected&&!strcmp(status.networks[i].ssid,status.ssid))continue;
   bool duplicate=false;for(unsigned j=0;j<i;j++)if(!strcmp(status.networks[i].ssid,status.networks[j].ssid))duplicate=true;if(duplicate||!status.networks[i].ssid[0])continue;
   lv_obj_t *b=button(status.networks[i].ssid,0,row*108,536,92,NET_BASE+i,false);lv_obj_set_parent(b,networks);lv_obj_set_pos(b,0,row++*108);
   lv_obj_t *name=lv_obj_get_child(b,0);lv_obj_set_width(name,326);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_align(name,LV_ALIGN_LEFT_MID,24,0);
   lv_obj_t *arrow=lv_label_create(b);lv_label_set_text(arrow,"2.4 GHz    >");lv_obj_set_style_text_font(arrow,&pet_dm_18,0);lv_obj_set_style_text_color(arrow,lv_color_hex(MUTED),0);lv_obj_align(arrow,LV_ALIGN_RIGHT_MID,-24,0);
  }
  if(!connected&&status.ssid[0])small_button("Manage saved network >",424,544,536,40,NETWORK,false);
 }else if(current==NETWORK){
  label("Network details",32,110,960,&pet_archivo_72,INK);
  panel(32,228,960,348);
  lv_obj_t *network_name=label(chosen_ssid,56,250,652,&pet_dm_36,INK);lv_label_set_long_mode(network_name,LV_LABEL_LONG_DOT);
  lv_obj_t *connection=label(status.wifi_connected?"Connected":"Saved",744,259,224,&pet_dm_24,status.wifi_connected?GREEN:MUTED);lv_obj_set_style_text_align(connection,LV_TEXT_ALIGN_RIGHT,0);
  box(56,310,912,1,LINE);
  const int columns[]={56,288,520,752};const char *headings[]={"BAND","CHANNEL","SIGNAL","IP ADDRESS"};
  char channel[16]="--",signal[24]="--";
  if(status.wifi_connected&&status.wifi_channel){snprintf(channel,sizeof(channel),"%u",status.wifi_channel);snprintf(signal,sizeof(signal),"%d dBm",status.wifi_rssi);}
  const char *values[]={status.wifi_connected?"2.4 GHz":"--",channel,signal,status.wifi_connected&&status.ip[0]?status.ip:"--"};
  for(unsigned i=0;i<4;i++){label(headings[i],columns[i],334,216,&pet_dm_18,MUTED);label(values[i],columns[i],369,216,&pet_dm_24,INK);}
  box(56,421,912,1,LINE);
  label("Forget removes saved credentials and disconnects Wi-Fi.",56,441,912,&pet_dm_18,MUTED);
  button("Edit connection",56,488,440,64,NETWORK_EDIT,false);
  lv_obj_t *edit=lv_obj_get_child(screen,lv_obj_get_child_cnt(screen)-1);lv_obj_set_style_bg_color(edit,lv_color_hex(LINE),0);
  lv_obj_t *forget=button("Forget this network",512,488,456,64,WIFI_FORGET,false);
  lv_obj_set_style_border_width(forget,1,0);lv_obj_set_style_border_color(forget,lv_color_hex(PINK),0);lv_obj_set_style_text_color(lv_obj_get_child(forget,0),lv_color_hex(PINK),0);
 }else if(current==PASSWORD){
  label("Connect to Wi-Fi",32,97,960,&pet_archivo_32,INK);
  ssid_input=lv_textarea_create(screen);lv_obj_set_pos(ssid_input,32,145);lv_obj_set_size(ssid_input,460,58);lv_textarea_set_one_line(ssid_input,true);lv_textarea_set_max_length(ssid_input,32);lv_textarea_set_text(ssid_input,chosen_ssid);lv_textarea_set_placeholder_text(ssid_input,"Network name");lv_obj_set_style_text_font(ssid_input,&pet_dm_24,0);
  password=lv_textarea_create(screen);lv_obj_set_pos(password,512,145);lv_obj_set_size(password,408,58);lv_textarea_set_one_line(password,true);lv_textarea_set_password_mode(password,true);lv_textarea_set_max_length(password,63);lv_textarea_set_placeholder_text(password,"Password");lv_obj_set_style_text_font(password,&pet_dm_24,0);
  lv_obj_t *eye=button(LV_SYMBOL_EYE_OPEN,932,145,60,58,PASSWORD_EYE,false);lv_obj_set_style_text_font(lv_obj_get_child(eye,0),&lv_font_montserrat_24,0);
  username=lv_textarea_create(screen);lv_obj_set_pos(username,32,215);lv_obj_set_size(username,460,58);lv_textarea_set_one_line(username,true);lv_textarea_set_max_length(username,127);lv_textarea_set_placeholder_text(username,"Full email / user@umb.edu");lv_obj_set_style_text_font(username,&pet_dm_24,0);if(!enterprise)lv_obj_add_flag(username,LV_OBJ_FLAG_HIDDEN);
  small_button(enterprise?"Username + password":"Password only",512,215,240,58,ENTERPRISE_TOGGLE,false);
  button("Connect",768,215,224,58,WIFI_CONNECT,true);
  join_hint=label("2.4 GHz only. Browser sign-in needs venue help or a hotspot.",32,282,960,&pet_dm_18,MUTED);
  lv_obj_t *kb=lv_keyboard_create(screen);join_keyboard=kb;lv_obj_set_align(kb,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(kb,20,316);lv_obj_set_size(kb,984,272);lv_keyboard_set_textarea(kb,chosen_ssid[0]?(enterprise?username:password):ssid_input);lv_obj_set_style_text_font(kb,&lv_font_montserrat_24,0);
  lv_obj_add_event_cb(ssid_input,focus_input,LV_EVENT_FOCUSED,kb);lv_obj_add_event_cb(password,focus_input,LV_EVENT_FOCUSED,kb);lv_obj_add_event_cb(username,focus_input,LV_EVENT_FOCUSED,kb);

 }
 dim_overlay=box(0,0,1024,600,0x000000);lv_obj_set_style_bg_opa(dim_overlay,(100-status.brightness)*2,0);
}
static void apply(void *arg){(void)arg;queued=false;current=pending;render();}
static void show(unsigned state){pending=state<COUNT?state:HOME;if(!queued){queued=true;lv_async_call(apply,NULL);}}
static void poll(lv_timer_t *timer){
 (void)timer;pet_status_t next;pet_services_snapshot(&next);
 if(next.asleep!=was_asleep){
#ifdef ESP_PLATFORM
  esp_err_t err=next.asleep?wavesahre_rgb_lcd_bl_off():wavesahre_rgb_lcd_bl_on();
  if(err!=ESP_OK){ESP_LOGE("pet_ui","Backlight operation failed: %d",err);}
#endif
  was_asleep=next.asleep;
 }
 if(next.revision!=last_revision){
  bool hello_change=next.hello_stage!=status.hello_stage||next.hello_starting!=status.hello_starting||next.hello_active!=status.hello_active||next.introduction_ready!=status.introduction_ready||next.phone_joined!=status.phone_joined||next.form_opened!=status.form_opened||strcmp(next.hello_error,status.hello_error)||strcmp(next.guest,status.guest)||strcmp(next.interest,status.interest)||next.card_stage!=status.card_stage||strcmp(next.card_status,status.card_status);
  bool opening_update=current==HELLO&&loading_caption&&status.hello_starting&&next.hello_starting&&!next.introduction_ready;
  bool networks=next.wifi_channel!=status.wifi_channel||next.wifi_rssi!=status.wifi_rssi||strcmp(next.ip,status.ip)||next.wifi_connected!=status.wifi_connected||strcmp(next.ssid,status.ssid)||memcmp(next.networks,status.networks,sizeof(next.networks))||next.scanning!=status.scanning||next.network_count!=status.network_count||strcmp(next.wifi_status,status.wifi_status)||strcmp(next.service_status,status.service_status);
  bool count_changed=next.connections!=status.connections;
  bool motion=next.motion!=status.motion||next.save_power!=status.save_power;
  if(next.phone_joined!=status.phone_joined||next.form_opened!=status.form_opened)phase=0;
  status=next;last_revision=next.revision;update_header();if(dim_overlay)lv_obj_set_style_bg_opa(dim_overlay,(100-status.brightness)*2,0);
  if(opening_update&&!motion){
   lv_label_set_text(loading_caption,loading_stage());
   if(hello_hint)lv_label_set_text(hello_hint,status.hello_error[0]?status.hello_error:"No app or sign-in needed.");
   hello_change=false;
  }
  if((current==HELLO&&(hello_change||motion))||((current==WIFI||current==NETWORK)&&networks)||(current==HOME&&motion)||(current==SETTINGS&&count_changed)){show(current);}
 }
 if(refresh){refresh=false;show(current);}
}
/* Consume the wake touch through release, so it cannot activate a hidden button. */
bool pet_ui_touch_wake(bool pressed){
 if(pressed&&was_asleep){pet_services_sleep(false);wake_release=true;return true;}
 if(wake_release){if(!pressed)wake_release=false;return true;}return was_asleep;
}
void pet_ui_show(unsigned state){show(state);}
void pet_ui_init(void){
 screen=lv_scr_act();lv_obj_remove_style_all(screen);lv_obj_set_style_bg_color(screen,lv_color_hex(BG),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);current=HOME;render();lv_timer_create(poll,200,NULL);
}
