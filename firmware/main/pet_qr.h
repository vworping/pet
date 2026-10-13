#pragma once
#include "lvgl.h"
/* Integer modules and >= four-module quiet zone; no logo covers encoded data. */
lv_obj_t *pet_qr_create(lv_obj_t *parent,const char *text,int x,int y,int size);
