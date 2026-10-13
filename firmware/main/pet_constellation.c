#include "pet_constellation.h"
#include "images/pet_images.h"
#include "pet_services.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(pet_archivo_32);
LV_FONT_DECLARE(pet_dm_18);
LV_FONT_DECLARE(pet_dm_24);
#define BG 0x111316
#define PANEL 0x1c1e23
#define INK 0xf1f0eb
#define MUTED 0xa7acb3
#define LINE 0x34373d
#define CARD_COUNT 3
#define PAPER 0xf1eee7
/* AXD front-page print treatment: contained images and palette footers. */
static lv_obj_t *root,*viewport,*cards[CARD_COUNT],*motion_label,*menu,*detail,*shutter,*sheet;
static unsigned spread;
static bool paused,allowed,dragged,closing;
static lv_point_t press_at;
static lv_obj_t *rect(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,bool opaque){
 lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
 lv_obj_set_style_bg_opa(o,opaque?LV_OPA_COVER:LV_OPA_TRANSP,0);return o;
}
static lv_obj_t *text(lv_obj_t *parent,const char *s,int x,int y,int w,const lv_font_t *font,uint32_t color){
 lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
 lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;
}
static lv_obj_t *control(lv_obj_t *parent,const char *s,int x,int y,int w,int h,lv_event_cb_t cb){
 lv_obj_t *o=lv_btn_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_set_style_bg_color(o,lv_color_hex(PANEL),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,10,0);
 lv_obj_set_style_bg_color(o,lv_color_hex(LINE),LV_STATE_PRESSED);
 lv_obj_t *l=text(o,s,0,0,w,&pet_dm_18,INK);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);
 lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,NULL);return o;
}
static bool moving(void){return allowed&&!paused;}
static void card_y(void *o,int32_t v){lv_obj_set_y(o,v);}
static void place_cards(void){
 for(int i=0;i<CARD_COUNT;i++)lv_obj_set_pos(cards[i],i*238,0);
}
static void update_motion_label(void){if(motion_label)lv_label_set_text(motion_label,moving()?"Stop motion":"Resume motion");}
void pet_constellation_set_motion(bool motion){allowed=motion;update_motion_label();}
static void toggle_motion(lv_event_t *e){
 (void)e;if(!allowed){pet_services_set_motion(true);pet_services_set_saver(false);allowed=true;paused=false;}else paused=!paused;
 update_motion_label();
}
static void palette(lv_obj_t *parent,unsigned photo,int x,int y,int w,int h){
 const pet_spread_t *s=&pet_spreads[spread];int at=0;
 for(int c=0;c<5;c++){int width=c==4?w-at:w*s->weights[photo][c]/10000;
  lv_obj_t *o=rect(parent,x+at,y,width,h,s->colors[photo][c],true);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);at+=width;}
}
/* Portfolio AXDDialogs' horizontal shutter reveal, reversed for dismissal.
 * Clip a fixed-size sheet instead of scaling the image on each frame. */
static void reveal(void *obj,int32_t height){
 lv_obj_set_height(obj,height);lv_obj_set_y(obj,(600-height)/2);lv_obj_set_y(sheet,-(600-height)/2);
}
static void detail_gone(lv_anim_t *a){(void)a;lv_obj_del(detail);detail=shutter=sheet=NULL;closing=false;}
static void close_detail(lv_event_t *e){
 (void)e;if(!detail||closing)return;closing=true;lv_anim_del(shutter,reveal);
 if(!moving()){lv_obj_del(detail);detail=shutter=sheet=NULL;closing=false;return;}
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,shutter);lv_anim_set_exec_cb(&a,reveal);
 lv_anim_set_values(&a,lv_obj_get_height(shutter),6);lv_anim_set_time(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_in);
 lv_anim_set_ready_cb(&a,detail_gone);lv_anim_start(&a);
}
static void inspect(unsigned photo){
 if(detail)return;
 const pet_spread_t *s=&pet_spreads[spread];
 detail=rect(root,0,0,1024,600,BG,true);
 shutter=rect(detail,0,0,1024,600,BG,true);sheet=rect(shutter,0,0,1024,600,BG,true);
 rect(sheet,32,76,960,1,LINE,true);
 lv_obj_t *heading=text(sheet,photo==0?"MAIN PHOTOGRAPH":"CLOSEST PALETTE",48,29,580,&pet_dm_18,s->accent);
 lv_obj_set_width(heading,LV_SIZE_CONTENT);lv_obj_update_layout(heading);
 text(sheet,s->captions[photo],48+lv_obj_get_width(heading)+20,29,300,&pet_dm_18,MUTED);
 control(sheet,"Close",832,16,144,44,close_detail);
 const lv_img_dsc_t *src=s->details[photo];
 int w=src->header.w,h=src->header.h;
 int diameter=(h*75+50)/100;if(diameter<216)diameter=216;if(diameter>288)diameter=288;
 if(diameter>656-w-24)diameter=656-w-24;
 int frame_x=(1024-(w+24+48+diameter+40+184))/2,frame_y=340-(h+42)/2;
 int wheel_x=frame_x+w+24+48,wheel_y=382-diameter/2,legend_y=232,legend_h=300,legend_x=wheel_x+diameter+40;
 lv_obj_t *frame=rect(sheet,frame_x,frame_y,w+24,h+42,PAPER,true);
 lv_obj_set_style_shadow_color(frame,lv_color_hex(0x000000),0);lv_obj_set_style_shadow_opa(frame,LV_OPA_40,0);
 lv_obj_set_style_shadow_width(frame,32,0);lv_obj_set_style_shadow_ofs_y(frame,14,0);
 lv_obj_t *image=lv_img_create(frame);lv_img_set_src(image,src);lv_obj_set_pos(image,12,12);
 text(frame,s->captions[photo],12,h+20,w-40,&pet_dm_18,BG);
 rect(frame,w-20,h+26,32,3,s->accent,true);
 if(moving()){
  lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,frame);lv_anim_set_exec_cb(&a,card_y);lv_anim_set_values(&a,frame_y,frame_y-6);
  lv_anim_set_time(&a,4000);lv_anim_set_playback_time(&a,4000);lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE);lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);lv_anim_start(&a);
 }
 text(sheet,s->name,wheel_x,140,diameter+224,&pet_archivo_32,s->accent);
 text(sheet,"MEASURED PALETTE",wheel_x,196,diameter+224,&pet_dm_18,MUTED);
 unsigned at=0;
 for(int c=0;c<5;c++){
  unsigned start=at*360/10000;at+=s->weights[photo][c];unsigned end=c==4?360:at*360/10000;
  lv_obj_t *arc=lv_arc_create(sheet);lv_obj_remove_style_all(arc);lv_obj_set_pos(arc,wheel_x,wheel_y);lv_obj_set_size(arc,diameter,diameter);
  lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_arc_set_rotation(arc,270);lv_arc_set_bg_angles(arc,0,360);lv_arc_set_angles(arc,start,end);
  lv_obj_set_style_arc_width(arc,diameter*33/100,LV_PART_INDICATOR);lv_obj_set_style_arc_color(arc,lv_color_hex(s->colors[photo][c]),LV_PART_INDICATOR);lv_obj_set_style_arc_opa(arc,LV_OPA_COVER,LV_PART_INDICATOR);lv_obj_set_style_arc_rounded(arc,false,LV_PART_INDICATOR);
  char hex[16],weight[12];snprintf(hex,sizeof(hex),"#%06lX",(unsigned long)s->colors[photo][c]);snprintf(weight,sizeof(weight),"%.1f%%",s->weights[photo][c]/100.0);
  int row_y=legend_y+c*legend_h/5,row_h=legend_h/5;
  lv_obj_t *dot=rect(sheet,legend_x,row_y+(row_h-12)/2,12,12,s->colors[photo][c],true);lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);
  text(sheet,hex,legend_x+20,row_y+(row_h-22)/2,102,&pet_dm_18,INK);lv_obj_t *l=text(sheet,weight,legend_x+124,row_y+(row_h-22)/2,60,&pet_dm_18,MUTED);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_RIGHT,0);
  if(c<4)rect(sheet,legend_x,legend_y+(c+1)*legend_h/5-1,184,1,LINE,true);
 }
 if(photo){char caption[64];snprintf(caption,sizeof(caption),"Distance %s",s->distances[photo-1]);lv_obj_t *distance=text(sheet,caption,624,29,184,&pet_dm_18,MUTED);lv_obj_set_style_text_align(distance,LV_TEXT_ALIGN_RIGHT,0);}
 if(moving()){lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,shutter);lv_anim_set_exec_cb(&a,reveal);lv_anim_set_values(&a,6,600);lv_anim_set_time(&a,220);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);}
}
static void photo_event(lv_event_t *e){
 lv_event_code_t code=lv_event_get_code(e);unsigned photo=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 lv_indev_t *input=lv_indev_get_act();
 if(code==LV_EVENT_PRESSED){dragged=false;if(input){lv_indev_get_point(input,&press_at);}}
 else if(code==LV_EVENT_PRESSING&&input&&photo){lv_point_t point;lv_indev_get_point(input,&point);
  if(abs(point.x-press_at.x)>8)dragged=true;
 }
 else if(code==LV_EVENT_CLICKED&&!dragged)inspect(photo);
}
static lv_obj_t *photo_card(lv_obj_t *parent,unsigned photo,int x){
 const pet_spread_t *s=&pet_spreads[spread];const lv_img_dsc_t *src=s->images[photo];
 const int w=198,row_h=336;
 lv_obj_t *card=rect(parent,x,0,w+16,row_h,PANEL,true);
 lv_obj_add_event_cb(card,photo_event,LV_EVENT_ALL,(void*)(uintptr_t)photo);
 lv_obj_t *image=lv_img_create(card);lv_img_set_src(image,src);lv_obj_set_pos(image,8+(198-src->header.w)/2,8+(268-src->header.h)/2);
 palette(card,photo,8,286,198,8);
 char caption[48];if(photo)snprintf(caption,sizeof(caption),"Distance %s",s->distances[photo-1]);else snprintf(caption,sizeof(caption),"%s",s->name);
 text(card,caption,8,304,w,&pet_dm_18,photo?INK:s->accent);
 lv_obj_set_style_transform_pivot_x(card,(w+16)/2,0);lv_obj_set_style_transform_pivot_y(card,row_h/2,0);
 return card;
}
static void populate(void){
 lv_obj_clean(viewport);
 for(int i=0;i<CARD_COUNT;i++)cards[i]=photo_card(viewport,i+1,0);
 place_cards();
}
static lv_obj_t *source_card,*main_label,*closest_label;
static void style_spread(void){
 const pet_spread_t *s=&pet_spreads[spread];
 lv_dropdown_set_text(menu,s->name);
 lv_obj_set_style_text_color(menu,lv_color_hex(s->accent),0);
 lv_obj_set_style_text_color(menu,lv_color_hex(INK),LV_PART_INDICATOR);
 lv_obj_t *list=lv_dropdown_get_list(menu);
 const lv_style_selector_t states[]={LV_PART_SELECTED,LV_PART_SELECTED|LV_STATE_CHECKED,LV_PART_SELECTED|LV_STATE_PRESSED,LV_PART_SELECTED|LV_STATE_CHECKED|LV_STATE_PRESSED};
 for(unsigned i=0;i<4;i++){lv_obj_set_style_bg_color(list,lv_color_hex(LINE),states[i]);lv_obj_set_style_bg_opa(list,LV_OPA_COVER,states[i]);lv_obj_set_style_text_opa(list,LV_OPA_TRANSP,states[i]);}
}
static void layout_photos(void){
 source_card=photo_card(root,0,48);lv_obj_set_y(source_card,228);
 int match_x=286;
 lv_obj_set_pos(viewport,match_x,228);lv_obj_set_width(viewport,976-match_x);lv_obj_set_x(closest_label,match_x);
 populate();style_spread();
}
static void select_spread(lv_event_t *e){
 spread=lv_dropdown_get_selected(lv_event_get_target(e));lv_obj_set_style_text_color(main_label,lv_color_hex(pet_spreads[spread].accent),0);lv_obj_del(source_card);
 layout_photos();
}
static void cleanup(lv_event_t *e){
 (void)e;
 /* Free decoded JPEGs when leaving the page, including an interrupted detail. */
 for(unsigned s=0;s<PET_SPREAD_COUNT;s++)for(int p=0;p<4;p++){lv_img_cache_invalidate_src(pet_spreads[s].images[p]);lv_img_cache_invalidate_src(pet_spreads[s].details[p]);}
 root=viewport=motion_label=menu=detail=shutter=sheet=source_card=NULL;dragged=closing=false;
}
void pet_constellation_create(lv_obj_t *parent,bool motion){
 allowed=motion;lv_img_cache_set_size(6);
 root=rect(parent,0,0,1024,600,BG,false);lv_obj_clear_flag(root,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(root,cleanup,LV_EVENT_DELETE,NULL);
 text(root,"Palette Constellation",48,118,470,&pet_archivo_32,INK);
 menu=lv_dropdown_create(root);lv_obj_remove_style_all(menu);lv_obj_set_pos(menu,544,106);lv_obj_set_size(menu,224,56);
 lv_dropdown_set_options(menu,"Afterglow\nCrimson\nBlue hour\nSilver");lv_dropdown_set_selected(menu,spread);
 lv_obj_set_style_text_font(menu,&lv_font_montserrat_20,LV_PART_INDICATOR);lv_obj_set_style_text_font(menu,&pet_dm_18,0);lv_obj_set_style_text_color(menu,lv_color_hex(INK),0);lv_obj_set_style_bg_color(menu,lv_color_hex(PANEL),0);lv_obj_set_style_border_width(menu,0,0);lv_obj_set_style_bg_opa(menu,LV_OPA_COVER,0);lv_obj_set_style_radius(menu,10,0);lv_obj_set_style_pad_hor(menu,16,0);lv_obj_set_style_pad_ver(menu,18,0);
 lv_obj_t *list=lv_dropdown_get_list(menu);lv_obj_remove_style_all(list);
 lv_obj_set_style_text_font(list,&pet_dm_24,0);lv_obj_set_style_text_line_space(list,24,0);lv_obj_set_style_max_height(list,320,0);
 lv_obj_set_style_bg_color(list,lv_color_hex(PANEL),0);lv_obj_set_style_bg_opa(list,LV_OPA_COVER,0);lv_obj_set_style_text_color(list,lv_color_hex(INK),0);
 lv_obj_set_style_pad_hor(list,16,0);lv_obj_set_style_pad_ver(list,12,0);lv_obj_set_style_radius(list,10,0);lv_obj_set_style_border_width(list,1,0);lv_obj_set_style_border_color(list,lv_color_hex(LINE),0);
 /* Keep the native dropdown's selection behavior, with individually colored labels. */
 lv_obj_set_style_text_opa(lv_obj_get_child(list,0),LV_OPA_TRANSP,0);
 for(unsigned i=0;i<PET_SPREAD_COUNT;i++)text(list,pet_spreads[i].name,0,i*(lv_font_get_line_height(&pet_dm_24)+24),190,&pet_dm_24,pet_spreads[i].accent);

 lv_obj_add_event_cb(menu,select_spread,LV_EVENT_VALUE_CHANGED,NULL);
 lv_obj_t *motion_button=control(root,"",784,106,192,56,toggle_motion);motion_label=lv_obj_get_child(motion_button,0);update_motion_label();
 main_label=text(root,"Main",48,192,214,&pet_dm_18,pet_spreads[spread].accent);closest_label=text(root,"Closest palettes",286,192,280,&pet_dm_18,MUTED);
 viewport=rect(root,286,228,690,336,BG,false);lv_obj_clear_flag(viewport,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(viewport,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
 layout_photos();

}
