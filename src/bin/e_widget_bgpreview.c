#include "e.h"

typedef struct _E_Widget_Data E_Widget_Data;
struct _E_Widget_Data
{
   Evas_Object *obj, *table;
   Eina_List   *desks;
   int          w, h, dx, dy, cx, cy;
};
typedef struct _E_Widget_Desk_Data E_Widget_Desk_Data;
struct _E_Widget_Desk_Data
{
   Evas_Object         *icon, *thumb, *live;
   int                  zone, manager, x, y;
   Ecore_Event_Handler *bg_upd_hdl;
   Ecore_Job           *resize_job;
   Eina_Bool            configurable : 1;
};

/* local function prototypes */
static void      _e_wid_data_del(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__);
static void      _e_wid_livethumb_resize(void *data, Evas *e __UNUSED__, Evas_Object *obj EINA_UNUSED, void *event_info __UNUSED__);
static void      _e_wid_del_hook(Evas_Object *obj);
static void      _e_wid_reconfigure(E_Widget_Data *wd);
static void      _e_wid_desk_cb_config(void *data, Evas *evas, Evas_Object *obj, void *event);
static void      _e_wid_cb_resize(void *data, Evas *evas, Evas_Object *obj, void *event);
static Eina_Bool _e_wid_cb_bg_update(void *data, int type, void *event);

EAPI Evas_Object *
e_widget_bgpreview_add(Evas *evas, int nx, int ny)
{
   Evas_Object *obj;
   E_Widget_Data *wd;

   obj = e_widget_add(evas);
   wd = E_NEW(E_Widget_Data, 1);
   wd->obj = obj;
   wd->dx = nx;
   wd->dy = ny;
   e_widget_data_set(obj, wd);
   e_widget_del_hook_set(obj, _e_wid_del_hook);

   wd->table = evas_object_table_add(evas);
   evas_object_table_padding_set(wd->table, 0, 0);
   evas_object_table_align_set(wd->table, 0.5, 0.5);
   e_widget_resize_object_set(wd->obj, wd->table);
   evas_object_show(wd->table);
   e_widget_sub_object_add(wd->obj, wd->table);

   e_widget_bgpreview_num_desks_set(obj, wd->dx, wd->dy);

   evas_object_event_callback_add(obj, EVAS_CALLBACK_RESIZE,
                                  _e_wid_cb_resize, NULL);
   return obj;
}

EAPI void
e_widget_bgpreview_num_desks_set(Evas_Object *obj, int nx, int ny)
{
   E_Widget_Data *wd;

   if (!(wd = e_widget_data_get(obj))) return;
   wd->dx = nx;
   wd->dy = ny;
   _e_wid_reconfigure(wd);
}

EAPI Evas_Object *
e_widget_bgpreview_desk_add(Evas *e, E_Zone *zone, int x, int y)
{
   E_Widget_Desk_Data *dd;
   const char *bgfile;

   bgfile = e_bg_file_get(zone->comp->num, zone->num, x, y);

   dd = E_NEW(E_Widget_Desk_Data, 1);
   dd->manager = zone->comp->num;
   dd->zone = zone->num;
   dd->x = x;
   dd->y = y;

   dd->live = e_livethumb_add(e);

   dd->thumb = edje_object_add(e_livethumb_evas_get(dd->live));
   edje_object_file_set(dd->thumb, bgfile, "e/desktop/background");
   e_livethumb_thumb_set(dd->live, dd->thumb);
   evas_object_show(dd->thumb);
   eina_stringshare_del(bgfile);

   evas_object_data_set(dd->live, "desk_data", dd);
   evas_object_event_callback_add(dd->live, EVAS_CALLBACK_FREE, _e_wid_data_del, dd);
   evas_object_event_callback_add(dd->live, EVAS_CALLBACK_RESIZE, _e_wid_livethumb_resize, dd);

   dd->bg_upd_hdl = ecore_event_handler_add(E_EVENT_BG_UPDATE,
                                            _e_wid_cb_bg_update, dd);

   return dd->live;
}

EAPI void
e_widget_bgpreview_desk_configurable_set(Evas_Object *obj, Eina_Bool enable)
{
   E_Widget_Desk_Data *dd;

   EINA_SAFETY_ON_NULL_RETURN(obj);
   dd = evas_object_data_get(obj, "desk_data");
   EINA_SAFETY_ON_NULL_RETURN(dd);
   enable = !!enable;
   if (dd->configurable == enable) return;
   if (enable)
     evas_object_event_callback_add(dd->icon, EVAS_CALLBACK_MOUSE_DOWN,
                                    _e_wid_desk_cb_config, dd);
   else
     evas_object_event_callback_del_full(dd->icon, EVAS_CALLBACK_MOUSE_DOWN,
                                         _e_wid_desk_cb_config, dd);
   dd->configurable = enable;
}

/* local function prototypes */
static void
_e_wid_livethumb_resize_job(void *data)
{
   E_Widget_Desk_Data *dd = data;
   E_Zone *zone;
   int w, h;

   zone = e_comp_object_util_zone_get(dd->live);
   if (!zone) zone = eina_list_data_get(e_comp_get(NULL)->zones);
   evas_object_geometry_get(dd->live, NULL, NULL, &w, &h);
   if ((w != zone->w) || (h != zone->h))
     {
        w *= 2;
        h *= 2;
        if (w > 128)
          {
             w = 128;
             h = (zone->h * w) / zone->w;
          }
        if (h > 128)
          {
             h = 128;
             w = (zone->w * h) / zone->h;
          }
     }
   e_livethumb_vsize_set(dd->live, w, h);
   dd->resize_job = NULL;
}

static void
_e_wid_livethumb_resize(void *data, Evas *e __UNUSED__, Evas_Object *obj EINA_UNUSED, void *event_info __UNUSED__)
{
   E_Widget_Desk_Data *dd = data;

   if (!dd->resize_job)
     dd->resize_job = ecore_job_add(_e_wid_livethumb_resize_job, dd);
}

static void
_e_wid_data_del(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   E_Widget_Desk_Data *dd;
   dd = data;
   if (!dd) return;
   if (dd->bg_upd_hdl) ecore_event_handler_del(dd->bg_upd_hdl);
   evas_object_del(dd->live);
   ecore_job_del(dd->resize_job);
   free(dd);
}

static void
_e_wid_del_hook(Evas_Object *obj)
{
   E_Widget_Data *wd;
   Evas_Object *o;

   if (!(wd = e_widget_data_get(obj))) return;

   EINA_LIST_FREE(wd->desks, o)
     evas_object_del(o);

   E_FREE(wd);
}

static void
_e_wid_reconfigure(E_Widget_Data *wd)
{
   Eina_List *l, *ll;
   Evas_Object *dw;
   E_Zone *zone;
   int tw, th, mw, mh, y;
   E_Widget_Desk_Data *dd;
   double zone_ratio, desk_ratio;

   zone = e_util_zone_current_get(e_manager_current_get());

   evas_object_geometry_get(wd->table, NULL, NULL, &tw, &th);
   if ((tw == 0) || (th == 0))
     {
        mw = mh = 0;
     }
   else
     {
        zone_ratio = (double)zone->w / zone->h;
        desk_ratio = (tw / wd->dx) / (th / wd->dy);

        if (zone_ratio > desk_ratio)
          {
             mw = tw / wd->dx;
             mh = mw / zone_ratio;
          }
        else
          {
             mh = th / wd->dy;
             mw = mh * zone_ratio;
          }
     }

   EINA_LIST_FOREACH_SAFE(wd->desks, l, ll, dw)
     {
        if (!(dd = evas_object_data_get(dw, "desk_data"))) continue;
        if ((dd->x < wd->dx) && (dd->y < wd->dy))
          {
             evas_object_size_hint_min_set(dw, mw, mh);
             evas_object_size_hint_max_set(dw, mw, mh);
          }
        else
          {
             evas_object_del(dd->live);
             evas_object_del(dw);
             wd->desks = eina_list_remove(wd->desks, dw);
          }
     }

   for (y = 0; y < wd->dy; y++)
     {
        int x, sx;

        if (y >= wd->cy) sx = 0;
        else sx = wd->cx;
        for (x = sx; x < wd->dx; x++)
          {
             Evas_Object *dp;
             Evas *e;

             e = evas_object_evas_get(wd->obj);
             dp = e_widget_bgpreview_desk_add(e, zone, x, y);

             dd = evas_object_data_get(dp, "desk_data");
             dp = dd->icon = edje_object_add(e);
             e_theme_edje_object_set(dd->icon, "base/theme/widgets",
                                     "e/widgets/deskpreview/desk");

             edje_object_part_swallow(dd->icon, "e.swallow.content", dd->live);
             dd->configurable = EINA_TRUE;
             evas_object_event_callback_add(dd->icon, EVAS_CALLBACK_MOUSE_DOWN,
                                            _e_wid_desk_cb_config, dd);
             evas_object_show(dd->icon);
             evas_object_data_set(dd->icon, "desk_data", dd);
             evas_object_size_hint_min_set(dp, mw, mh);
             evas_object_size_hint_max_set(dp, mw, mh);
             evas_object_size_hint_aspect_set(dp, EVAS_ASPECT_CONTROL_BOTH, zone->w, zone->h);
             evas_object_table_pack(wd->table, dp, x, y, 1, 1);
             wd->desks = eina_list_append(wd->desks, dp);
          }
     }

   wd->cx = wd->dx;
   wd->cy = wd->dy;
}

static void
_e_wid_desk_cb_config(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   E_Widget_Desk_Data *dd;
   Evas_Event_Mouse_Down *ev;

   ev = event;
   if (!(dd = data)) return;
   if (ev->button == 1)
     {
        char buff[256];

        snprintf(buff, sizeof(buff), "%i %i %i %i",
                 dd->manager, dd->zone, dd->x, dd->y);
        e_configure_registry_call("internal/desk", e_util_comp_current_get(), buff);
     }
}

static void
_e_wid_cb_resize(void *data __UNUSED__, Evas *evas __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   E_Widget_Data *wd;

   if (!(wd = e_widget_data_get(obj))) return;
   _e_wid_reconfigure(wd);
}

static Eina_Bool
_e_wid_cb_bg_update(void *data, int type, void *event)
{
   E_Event_Bg_Update *ev;
   E_Widget_Desk_Data *dd;

   if (type != E_EVENT_BG_UPDATE) return ECORE_CALLBACK_PASS_ON;
   if (!(dd = data)) return ECORE_CALLBACK_PASS_ON;
   ev = event;

   if (((ev->manager < 0) || (dd->manager == ev->manager)) &&
       ((ev->zone < 0) || (dd->zone == ev->zone)) &&
       ((ev->desk_x < 0) || (dd->x == ev->desk_x)) &&
       ((ev->desk_y < 0) || (dd->y == ev->desk_y)))
     {
        const char *bgfile;

        bgfile = e_bg_file_get(dd->manager, dd->zone, dd->x, dd->y);
        edje_object_file_set(dd->thumb, bgfile, "e/desktop/background");
        eina_stringshare_del(bgfile);
     }

   return ECORE_CALLBACK_PASS_ON;
}

