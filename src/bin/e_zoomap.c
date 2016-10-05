#include "e.h"

#define SMART_NAME     "e_zoomap"
#define API_ENTRY      E_Smart_Data * sd; sd = evas_object_smart_data_get(obj); if ((!obj) || (!sd) || (evas_object_type_get(obj) && strcmp(evas_object_type_get(obj), SMART_NAME)))
#define INTERNAL_ENTRY E_Smart_Data * sd; sd = evas_object_smart_data_get(obj); if (!sd) return;
typedef struct _E_Smart_Data E_Smart_Data;

struct _E_Smart_Data
{
   Evas_Object *smart_obj, *child_obj;
   Evas_Coord   x, y, w, h;
   Evas_Coord   child_w, child_h;
   int32_t      scale;
   uint32_t     transform;
   int          src_x, src_y, src_w, src_h;
   int          dst_w, dst_h;
   unsigned int recurse;
   Eina_Bool    solid : 1;
   Eina_Bool    smooth : 1;
   Eina_Bool    always : 1;
   Eina_Bool    viewport : 1;
};

/* local subsystem functions */
static void _e_zoomap_smart_child_del_hook(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void _e_zoomap_smart_child_resize_hook(void *data, Evas *e, Evas_Object *obj, void *event_info);

static void _e_zoomap_smart_reconfigure(E_Smart_Data *sd);
static void _e_zoomap_smart_add(Evas_Object *obj);
static void _e_zoomap_smart_del(Evas_Object *obj);
static void _e_zoomap_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void _e_zoomap_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h);
static void _e_zoomap_smart_show(Evas_Object *obj);
static void _e_zoomap_smart_hide(Evas_Object *obj);
static void _e_zoomap_smart_color_set(Evas_Object *obj, int r, int g, int b, int a);
static void _e_zoomap_smart_clip_set(Evas_Object *obj, Evas_Object *clip);
static void _e_zoomap_smart_clip_unset(Evas_Object *obj);
static void _e_zoomap_smart_init(void);

/* local subsystem globals */
static Evas_Smart *_e_smart = NULL;

/* externally accessible functions */
E_API void
e_zoomap_commit(Evas_Object *obj)
{
   API_ENTRY return;

   _e_zoomap_smart_reconfigure(sd);
}

E_API void
e_zoomap_viewport_source_set(Evas_Object *obj, int x, int y, int w, int h)
{
   API_ENTRY return;
fprintf(stderr, "ZOOM: %d %d %d %d\n", x, y, w, h);
   sd->src_x = x;
   sd->src_y = y;
   sd->src_w = w;
   sd->src_h = h;
   sd->viewport = EINA_TRUE;
}

E_API void
e_zoomap_viewport_destination_set(Evas_Object *obj, int w, int h)
{
   API_ENTRY return;

   sd->w = w;
   sd->h = h;
   sd->viewport = EINA_TRUE;
}

E_API void
e_zoomap_viewport_set(Evas_Object *obj, Eina_Bool enabled)
{
   API_ENTRY return;
fprintf(stderr, "ZOOM: %d\n", enabled);
   sd->viewport = enabled;
}

E_API void
e_zoomap_scale_set(Evas_Object *obj, int32_t scale)
{
   API_ENTRY return;

   sd->scale = scale;
}

E_API void
e_zoomap_transform_set(Evas_Object *obj, uint32_t transform)
{
   API_ENTRY return;

   sd->transform = transform;
}

E_API Evas_Object *
e_zoomap_add(Evas *evas)
{
   _e_zoomap_smart_init();
   return evas_object_smart_add(evas, _e_smart);
}

E_API void
e_zoomap_child_set(Evas_Object *obj, Evas_Object *child)
{
   API_ENTRY return;
fprintf(stderr, "CHILD SET: %p %p\n", obj, child);
   if (child == sd->child_obj) return;
   if (sd->child_obj)
     {
        evas_object_map_set(sd->child_obj, NULL);
        evas_object_map_enable_set(sd->child_obj, EINA_FALSE);
        evas_object_clip_unset(sd->child_obj);
        evas_object_smart_member_del(sd->child_obj);
        evas_object_event_callback_del(sd->child_obj, EVAS_CALLBACK_DEL,
                                       _e_zoomap_smart_child_del_hook);
        evas_object_event_callback_del(sd->child_obj, EVAS_CALLBACK_RESIZE,
                                       _e_zoomap_smart_child_resize_hook);
        sd->child_obj = NULL;
     }
   if (child)
     {
        int r, g, b, a;

        sd->child_obj = child;
        evas_object_smart_member_add(sd->child_obj, sd->smart_obj);
        evas_object_geometry_get(sd->child_obj, NULL, NULL,
                                 &sd->child_w, &sd->child_h);
fprintf(stderr, "child set: (%p)'s CHILD (%p) SIZE: %d %d\n", obj, sd->child_obj, sd->child_w, sd->child_h);
        evas_object_event_callback_add(child, EVAS_CALLBACK_DEL,
                                       _e_zoomap_smart_child_del_hook, sd);
        evas_object_event_callback_add(child, EVAS_CALLBACK_RESIZE,
                                       _e_zoomap_smart_child_resize_hook, sd);
        if (evas_object_visible_get(obj)) evas_object_show(sd->child_obj);
        else evas_object_hide(sd->child_obj);
        evas_object_color_get(sd->smart_obj, &r, &g, &b, &a);
        evas_object_color_set(sd->child_obj, r, g, b, a);
        evas_object_clip_set(sd->child_obj, evas_object_clip_get(sd->smart_obj));
        _e_zoomap_smart_reconfigure(sd);
     }
}

E_API void
e_zoomap_child_resize(Evas_Object *obj, int w, int h)
{
   API_ENTRY return;
   evas_object_resize(sd->child_obj, w, h);
fprintf(stderr, "child_resize (%p)'s CHILD (%p) RESIZED... %d %d\n", obj, sd->child_obj, w, h);
sd->child_w = w; sd->child_h = h;
}

E_API Evas_Object *
e_zoomap_child_get(Evas_Object *obj)
{
   API_ENTRY return NULL;
   return sd->child_obj;
}

E_API void
e_zoomap_smooth_set(Evas_Object *obj, Eina_Bool smooth)
{
   API_ENTRY return;
   smooth = !!smooth;
   if (sd->smooth == smooth) return;
   sd->smooth = smooth;
   _e_zoomap_smart_reconfigure(sd);
}

E_API Eina_Bool
e_zoomap_smooth_get(Evas_Object *obj)
{
   API_ENTRY return EINA_FALSE;
   return sd->smooth;
}

E_API void
e_zoomap_solid_set(Evas_Object *obj, Eina_Bool solid)
{
   API_ENTRY return;
   solid = !!solid;
   if (sd->solid == solid) return;
   sd->solid = solid;
   _e_zoomap_smart_reconfigure(sd);
}

E_API Eina_Bool
e_zoomap_solid_get(Evas_Object *obj)
{
   API_ENTRY return EINA_FALSE;
   return sd->solid;
}

E_API void
e_zoomap_always_set(Evas_Object *obj, Eina_Bool always)
{
   API_ENTRY return;
   always = !!always;
   if (sd->always == always) return;
   sd->always = always;
   _e_zoomap_smart_reconfigure(sd);
}

E_API Eina_Bool
e_zoomap_always_get(Evas_Object *obj)
{
   API_ENTRY return EINA_FALSE;
   return sd->always;
}

E_API void
e_zoomap_child_edje_solid_setup(Evas_Object *obj)
{
   const char *s;
   Eina_Bool solid;

   API_ENTRY return;
   if (!sd->child_obj) return;
   s = edje_object_data_get(sd->child_obj, "argb");
   if (!s) s = edje_object_data_get(sd->child_obj, "shaped");
   solid = (!s) || (s[0] != '1');
   if (sd->solid == solid) return;
   sd->solid = solid;
   _e_zoomap_smart_reconfigure(sd);
}

/* local subsystem functions */
static void
_e_zoomap_smart_child_del_hook(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Smart_Data *sd;

   sd = data;
   sd->child_obj = NULL;
}

static void
_e_zoomap_smart_child_resize_hook(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Smart_Data *sd;
   Evas_Coord w, h;

   sd = data;
   if (!sd->child_obj) return;
   evas_object_geometry_get(sd->child_obj, NULL, NULL, &w, &h);
   if ((w != sd->child_w) || (h != sd->child_h))
     {
        sd->child_w = w;
        sd->child_h = h;
        _e_zoomap_smart_reconfigure(sd);
     }
}

static void
_e_zoomap_size(E_Smart_Data *sd, int32_t *ow, int32_t *oh)
{
   switch (sd->transform)
     {
      case WL_OUTPUT_TRANSFORM_90:
      case WL_OUTPUT_TRANSFORM_270:
      case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        *ow = sd->child_h;
        *oh = sd->child_w;
        break;
      default:
        *ow = sd->child_w;
        *oh = sd->child_h;
        break;
     }

   if (sd->scale > 1)
     {
        *ow /= sd->scale;
        *oh /= sd->scale;
     }
}

static void
_e_zoomap_transform_point(E_Smart_Data *sd, int sx, int sy, int *dx, int *dy)
{
   int width, height;

   _e_zoomap_size(sd, &width, &height);

   switch (sd->transform)
     {
      case WL_OUTPUT_TRANSFORM_NORMAL:
      default:
        *dx = sx, *dy = sy;
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED:
        *dx = width - sx, *dy = sy;
        break;
      case WL_OUTPUT_TRANSFORM_90:
        *dx = height - sy, *dy = sx;
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        *dx = height - sy, *dy = width - sx;
        break;
      case WL_OUTPUT_TRANSFORM_180:
        *dx = width - sx, *dy = height - sy;
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        *dx = sx, *dy = height - sy;
        break;
      case WL_OUTPUT_TRANSFORM_270:
        *dx = sy, *dy = width - sx;
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        *dx = sy, *dy = sx;
        break;
     }

   if (sd->scale > 1)
     {
        *dx *= sd->scale;
        *dy *= sd->scale;
     }

fprintf(stderr, "TRANSFORM %d %d rot %d scale %d to %d %d\n", sx, sy, sd->transform, sd->scale, *dx, *dy);
}

static Eina_Bool
_e_zoomap_transformed(E_Smart_Data *sd)
{
   if (sd->scale > 1) return EINA_TRUE;
fprintf(stderr, "TRANSFORM: %d\n", sd->transform);
   if (sd->transform != 0) return EINA_TRUE;

   return EINA_FALSE;
}

static void
_e_zoomap_smart_reconfigure(E_Smart_Data *sd)
{
   if (!sd->child_obj) return;
   sd->recurse++;
   if ((!sd->always) &&
       ((sd->w == sd->child_w) && (sd->h == sd->child_h)) &&
       (!_e_zoomap_transformed(sd)))
     {
        evas_object_map_set(sd->child_obj, NULL);
        evas_object_map_enable_set(sd->child_obj, EINA_FALSE);
        evas_object_move(sd->child_obj, sd->x, sd->y);
        evas_object_resize(sd->child_obj, sd->w, sd->h);
     }
   else
     {
        Evas_Map *m;
        Evas *e = evas_object_evas_get(sd->child_obj);
        Evas_Coord cx = 0, cy = 0;
        int r = 0, g = 0, b = 0, a = 0;
        int width, height, x, y;

        evas_object_geometry_get(sd->child_obj, &cx, &cy, NULL, NULL);
        if (sd->recurse != 1)
          {
             /* recursion: do move and exit to set map in outer call */
             evas_object_move(sd->child_obj, sd->x, sd->y);
             sd->recurse--;
             return;
          }
        if ((cx != sd->x) || (cy != sd->y))
          {
             evas_smart_objects_calculate(e);
             evas_nochange_push(e);
             evas_object_move(sd->child_obj, sd->x, sd->y);
             evas_smart_objects_calculate(e);
             evas_nochange_pop(e);
          }
        evas_object_color_get(sd->child_obj, &r, &g, &b, &a);
        m = evas_map_new(4);
        _e_zoomap_size(sd, &width, &height);
fprintf(stderr, "PLZ HALP buffer:(%d, %d)  surface:(%d, %d)\n", sd->child_w, sd->child_h, width, height);
        evas_map_util_points_populate_from_geometry(m, sd->x, sd->y,
                                                    width, height, 0);

        _e_zoomap_transform_point(sd, 0, 0, &x, &y);
        evas_map_point_image_uv_set(m, 0, x, y);

        _e_zoomap_transform_point(sd, width, 0, &x, &y);
        evas_map_point_image_uv_set(m, 1, x, y);

        _e_zoomap_transform_point(sd, width, height, &x, &y);
        evas_map_point_image_uv_set(m, 2, x, y);

        _e_zoomap_transform_point(sd, 0, height, &x, &y);
        evas_map_point_image_uv_set(m, 3, x, y);

        evas_map_smooth_set(m, sd->smooth);
        evas_map_point_color_set(m, 0, r, g, b, a);
        evas_map_point_color_set(m, 1, r, g, b, a);
        evas_map_point_color_set(m, 2, r, g, b, a);
        evas_map_point_color_set(m, 3, r, g, b, a);
        if (a >= 255) evas_map_alpha_set(m, !sd->solid);
        else evas_map_alpha_set(m, EINA_TRUE); 
        evas_object_map_set(sd->child_obj, m);
        evas_object_map_enable_set(sd->child_obj, EINA_TRUE);
        evas_map_free(m);
     }
   sd->recurse--;
}

static void
_e_zoomap_smart_add(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = E_NEW(E_Smart_Data, 1);
   if (!sd) return;
   sd->smart_obj = obj;
   sd->x = sd->y = sd->w = sd->h = 0;
   sd->solid = EINA_FALSE;
   sd->always = EINA_FALSE;
   sd->smooth = EINA_TRUE;
   evas_object_smart_data_set(obj, sd);
}

static void
_e_zoomap_smart_del(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   if (sd->child_obj)
     {
        Evas_Object *o = sd->child_obj;
        
        sd->child_obj = NULL;
        evas_object_event_callback_del(o, EVAS_CALLBACK_DEL,
                                       _e_zoomap_smart_child_del_hook);
        evas_object_event_callback_del(o, EVAS_CALLBACK_RESIZE,
                                       _e_zoomap_smart_child_resize_hook);
        evas_object_del(o);
     }
   E_FREE(sd);
}

static void
_e_zoomap_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   INTERNAL_ENTRY;
   sd->x = x;
   sd->y = y;
   _e_zoomap_smart_reconfigure(sd);
}

static void
_e_zoomap_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   INTERNAL_ENTRY;
   sd->w = w;
   sd->h = h;
   _e_zoomap_smart_reconfigure(sd);
}

static void
_e_zoomap_smart_show(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   if (sd->child_obj) evas_object_show(sd->child_obj);
   if (!evas_object_map_enable_get(sd->child_obj)) _e_zoomap_smart_reconfigure(sd);
}

static void
_e_zoomap_smart_hide(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   if ((!sd->always) &&
       (evas_object_map_enable_get(sd->child_obj)))
     {
        evas_object_map_set(sd->child_obj, NULL);
        evas_object_map_enable_set(sd->child_obj, EINA_FALSE);
        evas_object_move(sd->child_obj, sd->x, sd->y);
        evas_object_resize(sd->child_obj, sd->w, sd->h);
     }
   if (sd->child_obj) evas_object_hide(sd->child_obj);
}

static void
_e_zoomap_smart_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   INTERNAL_ENTRY;
   if (sd->child_obj) evas_object_color_set(sd->child_obj, r, g, b, a);
}

static void
_e_zoomap_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   INTERNAL_ENTRY;
   if (sd->child_obj) evas_object_clip_set(sd->child_obj, clip);
}

static void
_e_zoomap_smart_clip_unset(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   if (sd->child_obj) evas_object_clip_unset(sd->child_obj);
}

/* never need to touch this */
static void
_e_zoomap_smart_init(void)
{
   static const Evas_Smart_Class sc =
     {
        SMART_NAME, EVAS_SMART_CLASS_VERSION,
        _e_zoomap_smart_add, _e_zoomap_smart_del, _e_zoomap_smart_move, _e_zoomap_smart_resize,
        _e_zoomap_smart_show, _e_zoomap_smart_hide, _e_zoomap_smart_color_set, _e_zoomap_smart_clip_set,
        _e_zoomap_smart_clip_unset, NULL, NULL, NULL, NULL, NULL, NULL, NULL
     };
   if (_e_smart) return;
   _e_smart = evas_smart_class_new(&sc);
}

