#include "pet_qr.h"
#include "src/extra/libs/qrcode/qrcodegen.h"
#include <string.h>
static void free_canvas(lv_event_t *e){lv_mem_free(lv_event_get_user_data(e));}
lv_obj_t *pet_qr_create(lv_obj_t *parent,const char *text,int x,int y,int size){
 const int max_version=10;size_t bytes=qrcodegen_BUFFER_LEN_FOR_VERSION(max_version);
 uint8_t *code=lv_mem_alloc(bytes),*tmp=lv_mem_alloc(bytes);
 if(!code||!tmp){if(code)lv_mem_free(code);if(tmp)lv_mem_free(tmp);return NULL;}
 bool ok=qrcodegen_encodeText(text,tmp,code,qrcodegen_Ecc_QUARTILE,1,max_version,qrcodegen_Mask_AUTO,true);lv_mem_free(tmp);
 if(!ok){lv_mem_free(code);return NULL;}
 int n=qrcodegen_getSize(code),scale=size/(n+8),offset=(size-n*scale)/2;
 if(scale<2){lv_mem_free(code);return NULL;}
 void *buffer=lv_mem_alloc((size_t)size*size*sizeof(lv_color_t));if(!buffer){lv_mem_free(code);return NULL;}
 lv_obj_t *canvas=lv_canvas_create(parent);lv_canvas_set_buffer(canvas,buffer,size,size,LV_IMG_CF_TRUE_COLOR);lv_obj_set_pos(canvas,x,y);lv_obj_add_event_cb(canvas,free_canvas,LV_EVENT_DELETE,buffer);lv_obj_clear_flag(canvas,LV_OBJ_FLAG_CLICKABLE);
 lv_canvas_fill_bg(canvas,lv_color_hex(0xf3eee4),LV_OPA_COVER);
 lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=lv_color_hex(0x17211d);d.bg_opa=LV_OPA_COVER;d.border_width=0;
 for(int row=0;row<n;row++)for(int col=0;col<n;col++)if(qrcodegen_getModule(code,col,row)){
  bool finder=(col<8&&row<8)||(col>=n-8&&row<8)||(col<8&&row>=n-8);
  d.radius=finder?0:1;lv_canvas_draw_rect(canvas,offset+col*scale,offset+row*scale,scale,scale,&d);
 }
 lv_mem_free(code);return canvas;
}
