#include "main.h"

static Elm_Palette_Color *
_find_col(Eina_List *colors, const char *name)
{
   Eina_List *l;
   Elm_Palette_Color *col;

   EINA_LIST_FOREACH(colors, l, col)
     {
        if ((col->name) && (!strcmp(col->name, name))) return col;
     }
   return NULL;
}

void
palimg_update(Evas_Object *img, Elm_Palette *pal)
{
   unsigned char *pix, *p, *lin;
   int w, h, stride;
   Eina_List *l;
   Elm_Palette_Color *col;
   Elm_Palette_Color *cols[8];
   int x = 0, pixn = 0, xx, yy;
   int i, n, r, g, b, a;

   if (!pal) return;
   evas_object_image_size_get(img, &w, &h);
   stride = evas_object_image_stride_get(img);
   pix = evas_object_image_data_get(img, EINA_TRUE);
   lin = p = pix;
   memset(pix, 0, stride * h);
#define PIX_WRITE(r, g, b, a) \
   for (yy = 0; yy < 4; yy++) \
   { \
      for (xx = 0; xx < 4; xx++) \
        { \
           ((int *)(void *)(p + (yy * stride)))[xx] = \
               (a << 24) | (r << 16) | (g << 8) | (b); \
        } \
   } \
   pixn += 16; \
   x += 4; \
   p += 16; \
   if (x == w) \
   { \
      lin += stride * 4; \
      p = lin; \
      x = 0; \
   } \
   if (pixn >= 256) break

   n = 0;
   cols[n] = _find_col(pal->colors, ":bg"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":fg"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":selected"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":bg-dark"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":fg-light"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":selected-alt"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":selected2"); if (cols[n]) n++;
   cols[n] = _find_col(pal->colors, ":selected3"); if (cols[n]) n++;
   i = 0;
   while (pixn < 256)
     {
        if (i >= n) break;
        r = cols[i]->r;
        g = cols[i]->g;
        b = cols[i]->b;
        a = cols[i]->a;
        evas_color_argb_premul(a, &r, &g, &b);
        PIX_WRITE(r, g, b, a);
        i++;
     }
#undef PIX_WRITE
   if (!((i == 0) || (i == 4) || (i == 8)))
     {
        lin += stride * 4;
        p = lin;
        x = 0;
     }

#define PIX_WRITE(r, g, b, a) \
   ((int *)(void *)p)[0] = (a << 24) | (r << 16) | (g << 8) | (b); \
   p += sizeof(int); \
   pixn++; \
   x++; \
   if (x == w) \
   { \
      lin += stride; \
      p = lin; \
      x = 0; \
   } \
   if (pixn >= 256) break
   while (pixn < 256)
     {
        if (!pal->colors)
          {
             PIX_WRITE(0, 0, 0, 0);
          }
        else
          {
             EINA_LIST_FOREACH(pal->colors, l, col)
               {
                  r = col->r;
                  g = col->g;
                  b = col->b;
                  a = col->a;
                  evas_color_argb_premul(a, &r, &g, &b);
                  PIX_WRITE(r, g, b, a);
               }
          }
     }
   evas_object_image_data_set(img, pix);
   evas_object_image_data_update_add(img, 0, 0, w, h);
}

Evas_Object *
palimg_add(Evas_Object *win)
{
   const int zoom = 2;
   Evas_Object *o = evas_object_image_filled_add(evas_object_evas_get(win));
   evas_object_image_smooth_scale_set(o, EINA_FALSE);
   evas_object_image_alpha_set(o, EINA_TRUE);
   evas_object_image_size_set(o, 16, 16);
   evas_object_size_hint_min_set(o,
                                 ELM_SCALE_SIZE(zoom * 16),
                                 ELM_SCALE_SIZE(zoom * 16));
   return o;
}
