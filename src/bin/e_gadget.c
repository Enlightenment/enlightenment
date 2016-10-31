#include "e.h"

#define SNAP_DISTANCE 5
#define E_GADGET_TYPE 0xE31337

#define IS_HORIZ(orient) \
  ((orient) == E_GADGET_SITE_ORIENT_HORIZONTAL)

#define IS_VERT(orient) \
  ((orient) == E_GADGET_SITE_ORIENT_VERTICAL)

#define ZGS_GET(obj) \
   E_Gadget_Site *zgs; \
   zgs = evas_object_data_get((obj), "__e_gadget_site"); \
   if (!zgs) abort()

typedef struct E_Gadget_Config E_Gadget_Config;

typedef struct E_Gadget_Site
{
   Eina_Stringshare *name;
   Eina_Bool autoadd;
   E_Gadget_Site_Gravity gravity;
   E_Gadget_Site_Orient orient;
   E_Gadget_Site_Anchor anchor;
   Eina_List *gadgets;
   Eina_Inlist *gadget_list;

   Evas_Object *layout;
   Evas_Object *events;
   E_Gadget_Style_Cb style_cb;
   int cur_size;

   E_Gadget_Config *action;
   Ecore_Event_Handler *move_handler;
   Ecore_Event_Handler *mouse_up_handler;
} E_Gadget_Site;


struct E_Gadget_Config
{
   EINA_INLIST;
   int id;
   Eina_Stringshare *type;
   E_Object *e_obj_inherit;
   Evas_Object *display;
   Evas_Object *gadget;
   struct
   {
      Evas_Object *obj;
      int minw, minh;
      Eina_Stringshare *name;
   } style;
   E_Gadget_Configure_Cb configure;
   Evas_Object *cfg_object;
   E_Gadget_Site *site;
   E_Menu *menu;

   struct
   {
      Evas_Object *popup;
      Evas_Smart_Cb allow;
      Evas_Smart_Cb deny;
      void *data;
   } allow_deny;

   Eina_Hash *drop_handlers;

   double x, y; //fixed % positioning
   double w, h; //fixed % sizing
   Evas_Point offset; //offset from mouse down
   Evas_Point down; //coords from mouse down
   E_Gadget_Config *orig; //gadget is a copy of the original gadget during a move
   Eina_Bool moving : 1;
   Eina_Bool resizing : 1;
   Eina_Bool display_del : 1; //deleted using ->display
};

typedef struct E_Gadget_Sites
{
   Eina_List *sites;
} E_Gadget_Sites;

typedef struct E_Gadget_Type
{
   E_Gadget_Create_Cb cb;
   E_Gadget_Wizard_Cb wizard;
} E_Gadget_Type;

typedef struct Gadget_Item
{
   Evas_Object *editor;
   Evas_Object *gadget;
   Evas_Object *site;
} Gadget_Item;

#define DESKLOCK_DEMO_LAYER (E_LAYER_CLIENT_POPUP - 100)

static Eina_List *desktop_handlers;
static Evas_Object *desktop_rect;
static Evas_Object *desktop_editor;
static Eina_Bool added = 1;

static Evas_Object *pointer_site;
static Eina_List *handlers;

static Eina_Hash *gadget_types;
static E_Gadget_Sites *sites;
static Ecore_Event_Handler *comp_add_handler;

static Evas_Object *comp_site;

static E_Action *move_act;
static E_Action *resize_act;
static E_Action *configure_act;
static E_Action *menu_act;

static E_Config_DD *edd_sites;
static E_Config_DD *edd_site;
static E_Config_DD *edd_gadget;

static void _gadget_object_finalize(E_Gadget_Config *zgc);
static void _editor_pointer_site_init(E_Gadget_Site_Orient orient, Evas_Object *site, Evas_Object *editor, Eina_Bool );

static void
_comp_site_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_resize(data, e_comp->w, e_comp->h);
}

static void
_gadget_free(E_Gadget_Config *zgc)
{
   evas_object_del(zgc->display);
   eina_stringshare_del(zgc->type);
   eina_stringshare_del(zgc->style.name);
   free(zgc);
}

static E_Gadget_Config *
_gadget_at_xy(E_Gadget_Site *zgs, int x, int y, E_Gadget_Config *exclude)
{
   Eina_List *l;
   E_Gadget_Config *zgc, *saved = NULL;

   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     {
        int ox, oy, ow, oh;

        if (!zgc->gadget) continue;

        evas_object_geometry_get(zgc->display, &ox, &oy, &ow, &oh);
        if (E_INSIDE(x, y, ox, oy, ow, oh))
          {
             if (zgc == exclude) saved = zgc;
             else return zgc;
          }
     }
   return saved;
}

static void
_gravity_apply(E_Gadget_Site *zgs, E_Gadget_Site_Gravity gravity)
{
   double ax = 0.5, ay = 0.5;

   switch (gravity)
     {
      case E_GADGET_SITE_GRAVITY_LEFT:
        ax = 0;
        break;
      case E_GADGET_SITE_GRAVITY_RIGHT:
        ax = 1;
        break;
      default: break;
     }
   switch (gravity)
     {
      case E_GADGET_SITE_GRAVITY_TOP:
        ay = 0;
        break;
      case E_GADGET_SITE_GRAVITY_BOTTOM:
        ay = 1;
        break;
      default: break;
     }
   elm_box_align_set(zgs->layout, ax, ay);
   evas_object_smart_callback_call(zgs->layout, "gadget_site_gravity", NULL);
}

static void
_gadget_reparent(E_Gadget_Site *zgs, E_Gadget_Config *zgc)
{
   Eina_Inlist *l = EINA_INLIST_GET(zgc);
   E_Gadget_Config *z;

   if (!zgs->orient)
     {
        evas_object_layer_set(zgc->display, evas_object_layer_get(zgs->layout));
        if (evas_object_visible_get(zgs->events))
          evas_object_show(zgc->display);
        return;
     }
   switch (zgs->gravity)
     {
      case E_GADGET_SITE_GRAVITY_NONE:
        /* fake */
        break;
      case E_GADGET_SITE_GRAVITY_LEFT:
      case E_GADGET_SITE_GRAVITY_TOP:
        for (l = l->prev; l; l = l->prev)
          {
             z = EINA_INLIST_CONTAINER_GET(l, E_Gadget_Config);
             if (!z->display) continue;
             elm_box_pack_after(zgs->layout, zgc->display, z->display);
             return;
          }
        elm_box_pack_end(zgs->layout, zgc->display);
        break;
      default:
        for (l = l->next; l; l = l->next)
          {
             z = EINA_INLIST_CONTAINER_GET(l, E_Gadget_Config);
             if (!z->display) continue;
             elm_box_pack_before(zgs->layout, zgc->display, z->display);
             return;
          }
        /* right aligned: pack on left */
        elm_box_pack_start(zgs->layout, zgc->display);
     }
}

static void
_gadget_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   E_Gadget_Site *zgs = data;

   if (event_info) elm_object_tree_focus_allow_set(event_info, 0);
   evas_object_smart_callback_call(zgs->layout, "gadget_site_popup", event_info);
}

static void
_gadget_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Gadget_Config *zgc = data;

   zgc->display_del = obj == zgc->display;
   if (!e_object_is_del(zgc->e_obj_inherit))
     e_object_del(zgc->e_obj_inherit);
}

static void
_gadget_util_allow_deny_cleanup(E_Gadget_Config *zgc)
{
   zgc->allow_deny.allow = zgc->allow_deny.deny = NULL;
   zgc->allow_deny.data = NULL;
   evas_object_hide(zgc->allow_deny.popup);
   E_FREE_FUNC(zgc->allow_deny.popup, evas_object_del);
}

static void
_gadget_drop_handler_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Gadget_Config *zgc = data;

   if (zgc->drop_handlers)//may be calling during object_free
     eina_hash_del_by_key(zgc->drop_handlers, &obj);
}

static void
_gadget_object_free(E_Object *eobj)
{
   E_Gadget_Config *zgc;
   Evas_Object *g;

   g = e_object_data_get(eobj);
   zgc = evas_object_data_get(g, "__e_gadget");
   evas_object_smart_callback_call(zgc->site->layout, "gadget_destroyed", zgc->gadget);
   evas_object_event_callback_del_full(zgc->gadget, EVAS_CALLBACK_DEL, _gadget_del, zgc);
   if (zgc->display_del || (zgc->gadget == zgc->display))
     zgc->display = NULL;
   else
     {
        evas_object_event_callback_del_full(zgc->display, EVAS_CALLBACK_DEL, _gadget_del, zgc);
        E_FREE_FUNC(zgc->display, evas_object_del);
     }
   zgc->gadget = NULL;

   E_FREE_FUNC(zgc->drop_handlers, eina_hash_free);
   E_FREE_FUNC(zgc->gadget, evas_object_del);
   E_FREE_FUNC(zgc->cfg_object, evas_object_del);
   E_FREE_FUNC(zgc->style.obj, evas_object_del);
   _gadget_util_allow_deny_cleanup(zgc);
   E_FREE(zgc->e_obj_inherit);
   zgc->configure = NULL;
   zgc->display_del = zgc->moving = zgc->resizing = 0;
}

static void
_gadget_wizard_end(void *data, int id)
{
   E_Gadget_Config *zgc = data;

   zgc->id = id;
   evas_object_smart_callback_call(zgc->site->layout, "gadget_site_unlocked", NULL);
   _gadget_object_finalize(zgc);
}

static Eina_Bool
_gadget_object_create(E_Gadget_Config *zgc)
{
   E_Gadget_Type *t;
   Evas_Object *g;

   t = eina_hash_find(gadget_types, zgc->type);
   if (!t) return EINA_TRUE; //can't create yet

   if (!zgc->id)
     {
        if (t->wizard)
          {
             evas_object_smart_callback_call(zgc->site->layout, "gadget_site_locked", NULL);
             t->wizard(_gadget_wizard_end, zgc);
             added = 1;
             return EINA_TRUE;
          }
     }
   /* if id is < 0, gadget creates dummy config for demo use
    * if id is 0, gadget creates new config and returns id
    * otherwise, config of `id` is applied to created object
    */
   g = t->cb(zgc->site->layout, &zgc->id, zgc->site->orient);
   EINA_SAFETY_ON_NULL_RETURN_VAL(g, EINA_FALSE);
   added = 1;

   zgc->e_obj_inherit = E_OBJECT_ALLOC(E_Object, E_GADGET_TYPE, _gadget_object_free);
   e_object_data_set(zgc->e_obj_inherit, g);
   zgc->gadget = zgc->display = g;
   evas_object_smart_callback_add(g, "gadget_popup", _gadget_popup, zgc->site);
   evas_object_data_set(g, "__e_gadget", zgc);
   if (zgc->site->style_cb)
     zgc->site->style_cb(zgc->site->layout, zgc->style.name, g);

   if (!zgc->site->orient)
     evas_object_smart_need_recalculate_set(zgc->site->layout, 1);
   evas_object_event_callback_add(g, EVAS_CALLBACK_DEL, _gadget_del, zgc);
   _gadget_reparent(zgc->site, zgc);
   elm_object_tree_focus_allow_set(zgc->gadget, 0);
   evas_object_raise(zgc->site->events);

   evas_object_smart_callback_call(zgc->site->layout, "gadget_created", g);
   evas_object_show(zgc->display);
   return EINA_TRUE;
}

static void
_gadget_object_finalize(E_Gadget_Config *zgc)
{
   zgc->moving = 0;
   if (_gadget_object_create(zgc))
     {
        if (!zgc->display) return;
        evas_object_smart_callback_call(zgc->site->layout, "gadget_added", zgc->gadget);
        e_config_save_queue();
        return;
     }
   zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
   zgc->site->gadget_list = eina_inlist_remove(zgc->site->gadget_list, EINA_INLIST_GET(zgc));
   eina_stringshare_del(zgc->type);
   free(zgc);
}

static void
_site_gadget_resize(Evas_Object *g, int w, int h, Evas_Coord *ww, Evas_Coord *hh, Evas_Coord *ow, Evas_Coord *oh)
{
   Evas_Coord mnw, mnh, mxw, mxh;
   E_Gadget_Config *zgc;
   Evas_Aspect_Control aspect;
   int ax, ay;

   zgc = evas_object_data_get(g, "__e_gadget");
   w -= zgc->style.minw;
   h -= zgc->style.minh;

   evas_object_size_hint_min_get(g, &mnw, &mnh);
   evas_object_size_hint_max_get(g, &mxw, &mxh);
   evas_object_size_hint_aspect_get(g, &aspect, &ax, &ay);

   if (IS_HORIZ(zgc->site->orient))
     {
        *ww = mnw, *hh = h;
        if (!(*ww)) *ww = *hh;
     }
   else if (IS_VERT(zgc->site->orient))
     {
        *hh = mnh, *ww = w;
        if (!(*hh)) *hh = *ww;
     }
   else
     {
        *ww = mnw, *hh = mnh;
        if ((!(*ww)) || ((*ww) < w)) *ww = w;
        if ((!(*hh)) || ((*hh) < h)) *hh = h;
     }
   if (aspect && ax && ay)
     {
        switch (aspect)
          {
           case EVAS_ASPECT_CONTROL_HORIZONTAL:
             *hh = (*ww * ay / ax);
             break;
           case EVAS_ASPECT_CONTROL_VERTICAL:
             *ww = (*hh * ax / ay);
             break;
           default:
             if (IS_HORIZ(zgc->site->orient))
               *ww = (*hh * ax / ay);
             else if (IS_VERT(zgc->site->orient))
               *hh = (*ww * ay / ax);
             else
               {
                  double ar = ax / (double) ay;

                  if (ax == ay)
                    {
                       if (*ww > *hh)
                         *hh = *ww;
                       else
                         *ww = *hh;
                    }
                  else if (ar > 1.0)
                    *hh = (*ww * ay / ax);
                  else
                    *ww = (*hh * ax / ay);
               }
          }
     }
   *ww += zgc->style.minw;
   *hh += zgc->style.minh;
   *ow = *ww, *oh = *hh;
   if ((!ax) && (!ay))
     {
        if ((mxw >= 0) && (mxw < *ow)) *ow = mxw;
        if ((mxh >= 0) && (mxh < *oh)) *oh = mxh;
     }
   //fprintf(stderr, "%s: %dx%d\n", zgc->type, *ow, *oh);
   evas_object_resize(zgc->display, *ow, *oh);
}

static void
_site_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int x, y, w, h;
   E_Gadget_Site *zgs = data;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->events, x, y, w, h);
   evas_object_raise(zgs->events);
   if (!zgs->orient)
     evas_object_smart_need_recalculate_set(zgs->layout, 1);
}

static void
_site_layout_orient(Evas_Object *o, E_Gadget_Site *zgs)
{
   Evas_Coord x, y, w, h, xx, yy;
   Eina_List *l;
   double ax, ay;
   E_Gadget_Config *zgc;
   int mw, mh, sw, sh;

   evas_object_size_hint_min_get(o, &mw, &mh);
   evas_object_size_hint_min_get(zgs->layout, &sw, &sh);
   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->events, x, y, w, h);

   evas_object_box_align_get(o, &ax, &ay);

   xx = x;
   yy = y;

   if (zgs->gravity % 2)//left/top
     {
        EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
          {
             Evas_Coord gx = xx, gy = yy;
             int ww, hh, ow, oh;

             if (!zgc->display) continue;

             _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
             if (IS_HORIZ(zgs->orient))
               gx += (Evas_Coord)(((double)(ww - ow)) * 0.5),
               gy += (h / 2) - (oh / 2);
             else if (IS_VERT(zgs->orient))
               gy += (Evas_Coord)(((double)(hh - oh)) * 0.5),
               gx += (w / 2) - (ow / 2);
             evas_object_move(zgc->display, gx, gy);
             if (IS_HORIZ(zgs->orient))
               xx += ow;
             else
               yy += oh;
          }
     }
   else if (zgs->gravity)
     {
        if (IS_HORIZ(zgs->orient))
          xx += w;
        else
          yy += h;

        EINA_LIST_REVERSE_FOREACH(zgs->gadgets, l, zgc)
          {
             Evas_Coord gx = xx, gy = yy;
             int ww, hh, ow, oh;

             if (!zgc->display) continue;

             _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
             if (IS_HORIZ(zgs->orient))
               gx -= (Evas_Coord)(((double)(ww - ow)) * 0.5) + ow,
               gy += (h / 2) - (oh / 2);
             else
               gy -= (Evas_Coord)(((double)(hh - oh)) * 0.5) + oh,
               gx += (w / 2) - (ow / 2);
             evas_object_move(zgc->display, gx, gy);
             if (IS_HORIZ(zgs->orient))
               xx -= ow;
             else
               yy -= oh;
          }
     }

   if (IS_HORIZ(zgs->orient))
     zgs->cur_size = abs(xx - x);
   else if (IS_VERT(zgs->orient))
     zgs->cur_size = abs(yy - y);

   w = IS_HORIZ(zgs->orient) ? zgs->cur_size : w;
   h = IS_VERT(zgs->orient) ? zgs->cur_size : h;
   if ((w == mw) && (h == mh) && (sw == sh) && (sw == -1))
     {
        /* FIXME: https://phab.enlightenment.org/T4747 */
        evas_object_size_hint_min_set(o, -1, -1);
     }
   evas_object_size_hint_min_set(o, w, h);
}

static void
_site_layout(Evas_Object *o, Evas_Object_Box_Data *priv EINA_UNUSED, void *data)
{
   E_Gadget_Site *zgs = data;
   Evas_Coord x, y, w, h, xx, yy;//, px, py;
   Eina_List *l;
   double ax, ay;
   E_Gadget_Config *zgc;

   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->events, x, y, w, h);

   evas_object_box_align_get(o, &ax, &ay);

   xx = x;
   yy = y;
   if (zgs->orient)
     {
        _site_layout_orient(o, zgs);
        return;
     }
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     {
        Evas_Coord gx = xx, gy = yy;
        int ww, hh, ow, oh;

        if (!zgc->display) continue;
        if (zgc->moving)
          _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
        else
          {
             _site_gadget_resize(zgc->gadget, w * zgc->w, h * zgc->h, &ww, &hh, &ow, &oh);
             if (zgc->x > -1.0)
               {
                  gx = zgc->x * w;
                  gx += (Evas_Coord)(((double)(ww - ow)) * 0.5 * -ax);
               }
             if (zgc->y > -1.0)
               {
                  gy = zgc->y * h;
                  gy += (Evas_Coord)(((double)(hh - oh)) * 0.5 * -ay);
               }
          }
        if (zgs->gravity)
          {
#if 0//FIXME
             if (zgs->gravity % 2)//left/top
               {
                  if (gx < px) gx = px;
               }
             else if (
               {
                  if (gx > px) gx = px;
               }

             if (zgs->gravity % 2)//left/top
               {
                  if (gy < py) gy = py;
               }
             else
               {
                  if (gy > py) gy = py;
               }
#endif
          }

        evas_object_move(zgc->display, gx, gy);
#if 0//FIXME
        if (zgs->gravity is horizontal or something)
          px = gx + (-ax * ow);
        else
          py = gy + (-ay * oh);
#endif
        if (eina_list_count(zgs->gadgets) == 1)
          {
             int mw, mh, sw, sh;

             evas_object_size_hint_min_get(o, &mw, &mh);
             evas_object_size_hint_min_get(zgs->layout, &sw, &sh);
             if ((ow == mw) && (oh == mh) && (sw == sh) && (sw == -1))
               {
                  /* FIXME: https://phab.enlightenment.org/T4747 */
                  evas_object_size_hint_min_set(o, -1, -1);
               }
             evas_object_size_hint_min_set(o, ow, oh);
          }
     }
}

static Eina_Bool
_gadget_mouse_resize(E_Gadget_Config *zgc, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int x, y, w, h;//site geom
   int ox, oy, ow, oh;//gadget geom
   double gw, gh;

   evas_object_geometry_get(zgc->site->layout, &x, &y, &w, &h);
   evas_object_geometry_get(zgc->display, &ox, &oy, &ow, &oh);
   gw = zgc->w * w;
   gh = zgc->h * h;
   gw += (ev->x - zgc->down.x);
   gh += (ev->y - zgc->down.y);
   zgc->w = gw / w;
   zgc->h = gh / h;
   zgc->down.x = ev->x;
   zgc->down.y = ev->y;
   elm_box_recalculate(zgc->site->layout);
   e_config_save_queue();
   return ECORE_CALLBACK_RENEW;
}

static void
_gadget_util_add(E_Gadget_Site *zgs, const char *type, int id)
{
   E_Gadget_Config *zgc;

   zgc = E_NEW(E_Gadget_Config, 1);
   zgc->id = id;
   zgc->type = eina_stringshare_add(type);
   zgc->x = zgc->y = -1;
   zgc->site = zgs;
   if (zgc->site->orient)
     zgc->w = zgc->h = -1;
   else
     {
        int w, h;

        evas_object_geometry_get(zgc->site->layout, NULL, NULL, &w, &h);
        zgc->w = (96 * e_scale) / (double)w;
        zgc->h = (96 * e_scale) / (double)h;
     }
   zgc->site->gadgets = eina_list_append(zgc->site->gadgets, zgc);
   zgs->gadget_list = eina_inlist_append(zgs->gadget_list, EINA_INLIST_GET(zgc));
   _gadget_object_finalize(zgc);
}

static Eina_Bool
_gadget_act_resize_end(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   E_Gadget_Config *zgc;
   Evas_Object *g;

   if (obj->type != E_GADGET_TYPE) return EINA_FALSE;
   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__e_gadget");
   zgc->moving = 0;

   E_FREE_FUNC(zgc->site->move_handler, ecore_event_handler_del);
   evas_object_smart_need_recalculate_set(zgc->site->layout, 1);
   return EINA_TRUE;
}

static Eina_Bool
_gadget_act_move(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   E_Gadget_Config *zgc, *z;
   Evas_Object *g;
   int w, h;

   if (obj->type != E_GADGET_TYPE) return EINA_FALSE;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__e_gadget");
   zgc->moving = 1;
   evas_object_pass_events_set(zgc->site->layout, 1);
   _editor_pointer_site_init(zgc->site->orient, NULL, NULL, 1);
   e_gadget_site_owner_setup(pointer_site, zgc->site->anchor, NULL);
   ZGS_GET(pointer_site);
   _gadget_util_add(zgs, zgc->type, zgc->id);
   z = eina_list_data_get(zgs->gadgets);
   evas_object_geometry_get(g, NULL, NULL, &w, &h);
   evas_object_resize(pointer_site, w, h);
   eina_stringshare_refplace(&z->style.name, zgc->style.name);
   z->orig = zgc;
   return EINA_TRUE;
}

static Eina_Bool
_gadget_act_resize(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   E_Gadget_Config *zgc;
   Evas_Object *g;

   if (obj->type != E_GADGET_TYPE) return EINA_FALSE;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__e_gadget");
   if (zgc->site->orient) return EINA_FALSE;
   zgc->resizing = 1;
   if (!zgc->site->move_handler)
     zgc->site->move_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, (Ecore_Event_Handler_Cb)_gadget_mouse_resize, zgc);
   return EINA_TRUE;
}

static void
_gadget_act_configure_object_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Gadget_Config *zgc = data;

   zgc->cfg_object = NULL;
}

static void
_gadget_configure(E_Gadget_Config *zgc)
{
   if (!zgc->configure) return;
   if (zgc->cfg_object)
     {
        evas_object_raise(zgc->cfg_object);
        evas_object_show(zgc->cfg_object);
        return;
     }
   zgc->cfg_object = zgc->configure(zgc->gadget);
   if (!zgc->cfg_object) return;
   evas_object_event_callback_add(zgc->cfg_object, EVAS_CALLBACK_DEL, _gadget_act_configure_object_del, zgc);
   evas_object_smart_callback_call(zgc->display, "gadget_popup", zgc->cfg_object);
}

static Eina_Bool
_gadget_act_configure(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   E_Gadget_Config *zgc;
   Evas_Object *g;

   if (obj->type != E_GADGET_TYPE) return EINA_FALSE;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__e_gadget");
   _gadget_configure(zgc);
   return EINA_TRUE;
}

static void
_gadget_remove(E_Gadget_Config *zgc)
{
   evas_object_smart_callback_call(zgc->site->layout, "gadget_removed", zgc->gadget);
   zgc->site->gadget_list = eina_inlist_remove(zgc->site->gadget_list, EINA_INLIST_GET(zgc));
   zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
   _gadget_free(zgc);
}

static void
_gadget_menu_remove(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   E_Gadget_Config *zgc = data;

   _gadget_remove(zgc);
   e_config_save_queue();
}

static void
_gadget_menu_configure(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   _gadget_configure(data);
}

static void
_gadget_style_menu_item_del(void *mi)
{
   eina_stringshare_del(e_object_data_get(mi));
}

static void
_gadget_menu_style(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   E_Gadget_Config *zgc = data;
   Eina_Stringshare *style = e_object_data_get(E_OBJECT(mi));

   eina_stringshare_refplace(&zgc->style.name, style);
   if (zgc->site->style_cb)
     zgc->site->style_cb(zgc->site->layout, style, zgc->gadget);
}

static Eina_Bool
_gadget_act_menu(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev)
{
   E_Gadget_Config *zgc;
   Evas_Object *g;
   E_Menu_Item *mi;
   E_Menu *subm;
   int x, y;

   if (obj->type != E_GADGET_TYPE) return EINA_FALSE;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__e_gadget");

   zgc->menu = e_menu_new();
   evas_object_smart_callback_call(g, "gadget_menu", zgc->menu);
   if (zgc->configure)
     {
        mi = e_menu_item_new(zgc->menu);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _gadget_menu_configure, zgc);
     }
   if (zgc->menu->items)
     {
        mi = e_menu_item_new(zgc->menu);
        e_menu_item_separator_set(mi, 1);
     }
   subm = e_menu_new();
   evas_object_smart_callback_call(zgc->site->layout, "gadget_site_style_menu", subm);
   if (e_object_data_get(E_OBJECT(subm)))
     {
        Eina_List *styles = e_object_data_get(E_OBJECT(subm));
        Eina_Stringshare *style;

        mi = e_menu_item_new(zgc->menu);
        e_menu_item_label_set(mi, _("Look"));
        e_util_menu_item_theme_icon_set(mi, "preferences-look");
        e_menu_item_submenu_set(mi, subm);
        e_object_unref(E_OBJECT(subm));

        EINA_LIST_FREE(styles, style)
          {
             char buf[1024];

             if (eina_streq(style, "base"))
               {
                  eina_stringshare_del(style);
                  continue;
               }
             mi = e_menu_item_new(subm);
             strncpy(buf, style, sizeof(buf) - 1);
             buf[0] = toupper(buf[0]);
             e_menu_item_label_set(mi, buf);
             snprintf(buf, sizeof(buf), "enlightenment/%s", style);
             e_util_menu_item_theme_icon_set(mi, buf);
             e_menu_item_radio_group_set(mi, 1);
             e_menu_item_radio_set(mi, 1);
             e_menu_item_toggle_set(mi, style == zgc->style.name);
             e_menu_item_disabled_set(mi, mi->toggle);
             e_object_data_set(E_OBJECT(mi), style);
             E_OBJECT_DEL_SET(mi, _gadget_style_menu_item_del);
             e_menu_item_callback_set(mi, _gadget_menu_style, zgc);
          }
     }
   else
     e_object_del(E_OBJECT(subm));


   mi = e_menu_item_new(zgc->menu);
   evas_object_smart_callback_call(zgc->site->layout, "gadget_site_owner_menu", mi);
   if (mi->label)
     {
        mi = e_menu_item_new(zgc->menu);
        e_menu_item_separator_set(mi, 1);
     }
   else
     e_object_del(E_OBJECT(mi));

   mi = e_menu_item_new(zgc->menu);
   e_menu_item_label_set(mi, _("Remove"));
   e_util_menu_item_theme_icon_set(mi, "list-remove");
   e_menu_item_callback_set(mi, _gadget_menu_remove, zgc);

   evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   e_menu_activate_mouse(zgc->menu,
                         e_zone_current_get(),
                         x, y, 1, 1,
                         E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
   evas_object_smart_callback_call(zgc->site->layout, "gadget_site_popup", zgc->menu->comp_object);
   return EINA_TRUE;
}

static Eina_Bool
_site_mouse_up(E_Gadget_Site *zgs, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   if (e_bindings_mouse_up_ecore_event_handle(E_BINDING_CONTEXT_ANY, zgs->action->e_obj_inherit, ev))
     {
        evas_object_pointer_mode_set(zgs->events, EVAS_OBJECT_POINTER_MODE_NOGRAB);
        zgs->action = NULL;
        E_FREE_FUNC(zgs->mouse_up_handler, ecore_event_handler_del);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_site_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   E_Gadget_Site *zgs = data;
   Evas_Event_Mouse_Down *ev = event_info;
   E_Gadget_Config *zgc;
   E_Action *act;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   zgc = _gadget_at_xy(zgs, ev->output.x, ev->output.y, NULL);
   if (!zgc) return;
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   act = e_bindings_mouse_down_evas_event_handle(E_BINDING_CONTEXT_ANY, zgc->e_obj_inherit, event_info);
   if (!act)
     {
        ev->event_flags &= ~EVAS_EVENT_FLAG_ON_HOLD;
        return;
     }
   if (act->func.end_mouse)
     {
        int x, y;

        evas_object_pointer_mode_set(obj, EVAS_OBJECT_POINTER_MODE_NOGRAB_NO_REPEAT_UPDOWN);
        zgs->action = zgc;
        if (!zgs->mouse_up_handler)
          zgs->mouse_up_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP, (Ecore_Event_Handler_Cb)_site_mouse_up, zgs);

        evas_object_geometry_get(zgc->display, &x, &y, NULL, NULL);
        zgc->offset.x = ev->canvas.x - x;
        zgc->offset.y = ev->canvas.y - y;
        zgc->down.x = ev->canvas.x;
        zgc->down.y = ev->canvas.y;
     }
}

static void
_site_drop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   E_Gadget_Site *zgs = data, *drop;
   Eina_List *l;
   E_Gadget_Config *zgc, *dzgc;
   int mx, my;
   int x, y, w, h;

   drop = evas_object_data_get(event_info, "__e_gadget_site");
   evas_pointer_canvas_xy_get(e_comp->evas, &mx, &my);
   evas_object_geometry_get(zgs->layout, &x, &y, &w, &h);
   if (!E_INSIDE(mx, my, x, y, w, h)) return;
   if (evas_object_smart_need_recalculate_get(event_info))
     evas_object_smart_calculate(event_info);
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     {
        if (!zgc->display) continue;

        evas_object_geometry_get(zgc->display, &x, &y, &w, &h);
        if (E_INSIDE(mx, my, x, y, w, h)) break;
     }
   if (zgc)
     {
        Eina_Bool pre = EINA_FALSE;
        if (IS_HORIZ(zgs->orient))
          {
             if (mx <= x + (w / 2))
               pre = EINA_TRUE;
          }
        else if (IS_VERT(zgs->orient))
          {
             if (my <= y + (h / 2))
               pre = EINA_TRUE;
          }
        else {} //FIXME
        if (zgs->orient)
          {
             Eina_List *ll;

             if (pre)
               EINA_LIST_REVERSE_FOREACH(drop->gadgets, ll, dzgc)
                 {
                    evas_object_smart_callback_call(zgs->layout, "gadget_moved", dzgc->gadget);
                    evas_object_del(dzgc->gadget);
                    zgs->gadget_list = eina_inlist_prepend_relative(zgs->gadget_list,
                      EINA_INLIST_GET(dzgc), EINA_INLIST_GET(zgc));
                    zgs->gadgets = eina_list_prepend_relative_list(zgs->gadgets, dzgc, l);
                    dzgc->site = zgs;
                    if (dzgc->id == -1) dzgc->id = 0;
                    _gadget_object_finalize(dzgc);
                 }
             else
               EINA_LIST_REVERSE_FOREACH(drop->gadgets, ll, dzgc)
                 {
                    evas_object_smart_callback_call(zgs->layout, "gadget_moved", dzgc->gadget);
                    evas_object_del(dzgc->gadget);
                    zgs->gadget_list = eina_inlist_append_relative(zgs->gadget_list,
                      EINA_INLIST_GET(dzgc), EINA_INLIST_GET(zgc));
                    zgs->gadgets = eina_list_append_relative_list(zgs->gadgets, dzgc, l);
                    dzgc->site = zgs;
                    if (dzgc->id == -1) dzgc->id = 0;
                    _gadget_object_finalize(dzgc);
                 }
          }
        else
          {
             int dx, dy, dw, dh, gx, gy, gw, gh;

             /* FIXME: this should place _(around)_ the gadget that got dropped on */
             evas_object_geometry_get(drop->layout, &dx, &dy, &dw, &dh);
             evas_object_geometry_get(zgs->layout, &x, &y, &w, &h);
             EINA_LIST_FOREACH(drop->gadgets, l, dzgc)
               {
                  /* calculate positioning offsets and normalize based on drop point */
                  evas_object_geometry_get(dzgc->display, &gx, &gy, &gw, &gh);
                  evas_object_smart_callback_call(zgs->layout, "gadget_moved", dzgc->display);
                  evas_object_del(dzgc->gadget);
                  zgs->gadget_list = eina_inlist_append(zgs->gadget_list,
                      EINA_INLIST_GET(dzgc));
                  zgs->gadgets = eina_list_append(zgs->gadgets, dzgc);
                  dzgc->x = ((gx - dx) / (double)dw) + ((mx - x) / (double)w);
                  dzgc->y = ((gy - dy) / (double)dh) + ((my - y) / (double)h);
                  dzgc->w = gw / (double)w;
                  dzgc->h = gh / (double)h;
                  dzgc->site = zgs;
                  if (dzgc->id == -1) dzgc->id = 0;
                  _gadget_object_finalize(dzgc);
               }
          }
     }
   else
     {
        if (zgs->orient)
          {
             if (mx >= x) //right of all exiting gadgets
               {
                  EINA_LIST_FOREACH(drop->gadgets, l, dzgc)
                    {
                       evas_object_smart_callback_call(zgs->layout, "gadget_moved", dzgc->gadget);
                       evas_object_del(dzgc->gadget);
                       zgs->gadget_list = eina_inlist_append(zgs->gadget_list,
                         EINA_INLIST_GET(dzgc));
                       zgs->gadgets = eina_list_append(zgs->gadgets, dzgc);
                       dzgc->site = zgs;
                       if (dzgc->id == -1) dzgc->id = 0;
                       _gadget_object_finalize(dzgc);
                    }
               }
             else
               {
                  EINA_LIST_REVERSE_FOREACH(drop->gadgets, l, dzgc)
                    {
                       evas_object_smart_callback_call(zgs->layout, "gadget_moved", dzgc->gadget);
                       evas_object_del(dzgc->gadget);
                       zgs->gadget_list = eina_inlist_prepend(zgs->gadget_list,
                         EINA_INLIST_GET(dzgc));
                       zgs->gadgets = eina_list_prepend(zgs->gadgets, dzgc);
                       dzgc->site = zgs;
                       if (dzgc->id == -1) dzgc->id = 0;
                       _gadget_object_finalize(dzgc);
                    }
               }
          }
        else
          {
             int dx, dy, dw, dh, gx, gy, gw, gh;

             evas_object_geometry_get(drop->layout, &dx, &dy, &dw, &dh);
             evas_object_geometry_get(zgs->layout, &x, &y, &w, &h);
             EINA_LIST_FOREACH(drop->gadgets, l, dzgc)
               {
                  /* calculate positioning offsets and normalize based on drop point */
                  evas_object_geometry_get(dzgc->display, &gx, &gy, &gw, &gh);
                  evas_object_smart_callback_call(zgs->layout, "gadget_moved", dzgc->gadget);
                  evas_object_del(dzgc->gadget);
                  zgs->gadget_list = eina_inlist_append(zgs->gadget_list,
                    EINA_INLIST_GET(dzgc));
                  zgs->gadgets = eina_list_append(zgs->gadgets, dzgc);
                  dzgc->x = ((gx - dx) / (double)dw) + ((mx - x - (mx - gx)) / (double)w);
                  dzgc->y = ((gy - dy) / (double)dh) + ((my - y - (my - gy)) / (double)h);
                  dzgc->w = gw / (double)w;
                  dzgc->h = gh / (double)h;
                  dzgc->site = zgs;
                  if (dzgc->id == -1) dzgc->id = 0;
                  _gadget_object_finalize(dzgc);
               }
          }
     }
   drop->gadget_list = NULL;
   drop->gadgets = eina_list_free(drop->gadgets);
   evas_object_smart_need_recalculate_set(zgs->layout, 1);
   e_config_save_queue();
}

static void
_site_restack(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Gadget_Site *zgs = data;

   if (!evas_object_smart_parent_get(zgs->layout))
     {
        Eina_List *l;
        E_Gadget_Config *zgc;
        int layer;

        layer = evas_object_layer_get(obj);
        if (!zgs->orient)
          {
             EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
               if (zgc->display)
                 evas_object_layer_set(zgc->display, layer);
          }
        evas_object_layer_set(zgs->events, layer);
     }
   evas_object_raise(zgs->events);
}

static void
_site_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Gadget_Site *zgs = data;
   E_Gadget_Config *zgc;
   Eina_List *l;

   E_FREE_FUNC(zgs->events, evas_object_del);
   zgs->layout = NULL;
   zgs->cur_size = 0;
   zgs->action = NULL;
   zgs->style_cb = NULL;
   E_FREE_FUNC(zgs->move_handler, ecore_event_handler_del);
   E_FREE_FUNC(zgs->mouse_up_handler, ecore_event_handler_del);
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     evas_object_del(zgc->display);
   if (zgs->name) return;
   eina_stringshare_del(zgs->name);
   free(zgs);
}

static void
_site_style(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Gadget_Site *zgs = data;
   E_Gadget_Config *zgc;
   Eina_List *l;

   if (!zgs->style_cb) return;
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     if (zgc->display)
       zgc->site->style_cb(zgc->site->layout, zgc->style.name, zgc->gadget);
}

static void
_site_visibility(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Gadget_Site *zgs = data;
   E_Gadget_Config *zgc;
   Eina_List *l;
   Eina_Bool vis = evas_object_visible_get(obj);

   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     if (zgc->display)
       {
          if (vis) evas_object_show(zgc->display);
          else evas_object_hide(zgc->display);
       }
}

static void
_site_create(E_Gadget_Site *zgs)
{
   zgs->layout = elm_box_add(e_comp->elm);
   elm_box_horizontal_set(zgs->layout, zgs->orient == E_GADGET_SITE_ORIENT_HORIZONTAL);
   _gravity_apply(zgs, zgs->gravity);
   if (!zgs->orient)
     {
        /* add dummy content to allow recalc to work */
        elm_box_pack_end(zgs->layout, elm_box_add(zgs->layout));
     }
   elm_box_layout_set(zgs->layout, _site_layout, zgs, NULL);
   evas_object_event_callback_add(zgs->layout, EVAS_CALLBACK_DEL, _site_del, zgs);
   evas_object_event_callback_add(zgs->layout, EVAS_CALLBACK_RESTACK, _site_restack, zgs);

   zgs->events = evas_object_rectangle_add(e_comp->evas);
   evas_object_name_set(zgs->events, "zgs->events");
   evas_object_event_callback_add(zgs->layout, EVAS_CALLBACK_MOVE, _site_move, zgs);
   evas_object_smart_callback_add(zgs->layout, "gadget_site_dropped", _site_drop, zgs);
   evas_object_smart_callback_add(zgs->layout, "gadget_site_style", _site_style, zgs);
   evas_object_pointer_mode_set(zgs->events, EVAS_OBJECT_POINTER_MODE_NOGRAB);
   evas_object_color_set(zgs->events, 0, 0, 0, 0);
   evas_object_repeat_events_set(zgs->events, 1);
   evas_object_show(zgs->events);
   evas_object_event_callback_add(zgs->events, EVAS_CALLBACK_MOUSE_DOWN, _site_mouse_down, zgs);
   if (!zgs->orient)
     {
        evas_object_event_callback_add(zgs->layout, EVAS_CALLBACK_SHOW, _site_visibility, zgs);
        evas_object_event_callback_add(zgs->layout, EVAS_CALLBACK_HIDE, _site_visibility, zgs);
     }

   evas_object_data_set(zgs->layout, "__e_gadget_site", zgs);
   E_LIST_FOREACH(zgs->gadgets, _gadget_object_create);
   evas_object_layer_set(zgs->events, evas_object_layer_get(zgs->layout));
   evas_object_raise(zgs->events);
}

static void
_site_auto_add(E_Gadget_Site *zgs, Evas_Object *comp_object)
{
   int x, y, w, h;

   _site_create(zgs);
   e_comp_object_util_del_list_append(comp_object, zgs->layout);
   evas_object_layer_set(zgs->layout, evas_object_layer_get(comp_object));
   evas_object_stack_above(zgs->layout, comp_object);
   evas_object_geometry_get(comp_object, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->layout, x, y, w, h);
}

static Eina_Bool
_site_auto_comp_object_handler(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Comp_Object *ev)
{
   const char *name;
   Eina_List *l;
   E_Gadget_Site *zgs;

   name = evas_object_name_get(ev->comp_object);
   if (!name) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(sites->sites, l, zgs)
     if (zgs->autoadd && eina_streq(zgs->name, name))
       {
          if (!zgs->layout)
            _site_auto_add(zgs, ev->comp_object);
          break;
       }
   return ECORE_CALLBACK_RENEW;
}

static Evas_Object *
_site_util_add(E_Gadget_Site_Orient orient, const char *name, Eina_Bool autoadd)
{
   E_Gadget_Site *zgs;
   Eina_List *l;
   Evas_Object *parent;

   if (name)
     {
        EINA_LIST_FOREACH(sites->sites, l, zgs)
          if (eina_streq(zgs->name, name))
            {
               if (zgs->layout) return zgs->layout;
               goto out;
            }
     }
   zgs = E_NEW(E_Gadget_Site, 1);

   zgs->name = eina_stringshare_add(name);
   zgs->autoadd = autoadd;

   if (name)
     sites->sites = eina_list_append(sites->sites, zgs);
out:
   zgs->orient = orient;
   switch (orient)
     {
      case E_GADGET_SITE_ORIENT_HORIZONTAL:
        zgs->gravity = E_GADGET_SITE_GRAVITY_LEFT;
        break;
      case E_GADGET_SITE_ORIENT_VERTICAL:
        zgs->gravity = E_GADGET_SITE_GRAVITY_TOP;
        break;
      default: break;
     }
   if (autoadd)
     {
        parent = evas_object_name_find(e_comp->evas, name);
        if (parent)
          _site_auto_add(zgs, parent);
     }
   else
     _site_create(zgs);

   return zgs->layout;
}

E_API Evas_Object *
e_gadget_site_add(E_Gadget_Site_Orient orient, const char *name)
{
   return _site_util_add(orient, name, 0);
}

E_API Evas_Object *
e_gadget_site_auto_add(E_Gadget_Site_Orient orient, const char *name)
{
   return _site_util_add(orient, name, 1);
}

E_API void
e_gadget_site_del(Evas_Object *obj)
{
   ZGS_GET(obj);

   sites->sites = eina_list_remove(sites->sites, zgs);
   evas_object_del(zgs->layout);
   eina_stringshare_del(zgs->name);
   free(zgs);
}

E_API E_Gadget_Site_Anchor
e_gadget_site_anchor_get(Evas_Object *obj)
{
   ZGS_GET(obj);

   return zgs->anchor;
}

E_API void
e_gadget_site_owner_setup(Evas_Object *obj, E_Gadget_Site_Anchor an, E_Gadget_Style_Cb cb)
{
   Evas_Object *parent;
   ZGS_GET(obj);

   zgs->anchor = an;
   zgs->style_cb = cb;
   evas_object_smart_callback_call(zgs->layout, "gadget_site_anchor", NULL);
   parent = evas_object_smart_parent_get(obj);
   if (parent)
     evas_object_smart_member_add(zgs->events, parent);
   else
     evas_object_smart_member_del(zgs->events);
}

E_API E_Gadget_Site_Orient
e_gadget_site_orient_get(Evas_Object *obj)
{
   ZGS_GET(obj);
   return zgs->orient;
}

E_API E_Gadget_Site_Gravity
e_gadget_site_gravity_get(Evas_Object *obj)
{
   ZGS_GET(obj);
   return zgs->gravity;
}

E_API void
e_gadget_site_gravity_set(Evas_Object *obj, E_Gadget_Site_Gravity gravity)
{
   ZGS_GET(obj);
   if (zgs->gravity == gravity) return;
   zgs->gravity = gravity;
   _gravity_apply(zgs, gravity);
   evas_object_smart_need_recalculate_set(zgs->layout, 1);
}

E_API Eina_List *
e_gadget_site_gadgets_list(Evas_Object *obj)
{
   Eina_List *l, *list = NULL;
   E_Gadget_Config *zgc;

   ZGS_GET(obj);
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     if (zgc->display)
       list = eina_list_append(list, zgc->gadget);
   return list;
}

E_API void
e_gadget_site_gadget_add(Evas_Object *obj, const char *type, Eina_Bool demo)
{
   int id = 0;

   demo = !!demo;
   id -= demo;
   EINA_SAFETY_ON_NULL_RETURN(gadget_types);
   ZGS_GET(obj);
   _gadget_util_add(zgs, type, id);
}

E_API Evas_Object *
e_gadget_site_get(Evas_Object *g)
{
   E_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);
   return zgc->site->layout;
}

E_API void
e_gadget_configure_cb_set(Evas_Object *g, E_Gadget_Configure_Cb cb)
{
   E_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN(g);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN(zgc);
   zgc->configure = cb;
}

E_API void
e_gadget_configure(Evas_Object *g)
{
   E_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN(g);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN(zgc);
   _gadget_configure(zgc);
}

E_API Eina_Stringshare *
e_gadget_type_get(Evas_Object *g)
{
   E_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);
   return zgc->type;
}

E_API void
e_gadget_type_add(const char *type, E_Gadget_Create_Cb callback, E_Gadget_Wizard_Cb wizard)
{
   E_Gadget_Type *t;
   Eina_List *l, *ll;
   E_Gadget_Site *zgs;
   E_Gadget_Config *zgc;

   EINA_SAFETY_ON_TRUE_RETURN(!!eina_hash_find(gadget_types, type));
   t = E_NEW(E_Gadget_Type, 1);
   t->cb = callback;
   t->wizard = wizard;
   eina_hash_add(gadget_types, type, t);
   EINA_LIST_FOREACH(sites->sites, l, zgs)
     if (zgs->layout)
       EINA_LIST_FOREACH(zgs->gadgets, ll, zgc)
         if (eina_streq(type, zgc->type))
           _gadget_object_create(zgc);
}

E_API void
e_gadget_type_del(const char *type)
{
   Eina_List *l, *ll;
   E_Gadget_Site *zgs;
   E_Gadget_Config *zgc;
   char buf[1024];

   strncpy(buf, type, sizeof(buf) - 1);

   if (!gadget_types) return;

   EINA_LIST_FOREACH(sites->sites, l, zgs)
     {
        EINA_LIST_FOREACH(zgs->gadgets, ll, zgc)
          if (eina_streq(buf, zgc->type))
            evas_object_del(zgc->gadget);
     }
   eina_hash_del_by_key(gadget_types, type);
}

E_API Eina_Iterator *
e_gadget_type_iterator_get(void)
{
   return gadget_types ? eina_hash_iterator_key_new(gadget_types) : NULL;
}

static void
_gadget_drop_handler_moveresize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int x, y, w, h;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   e_drop_handler_geometry_set(data, x, y, w, h);
}

E_API Evas_Object *
e_gadget_drop_handler_add(Evas_Object *g, void *data,
                                        void (*enter_cb)(void *data, const char *type, void *event),
                                        void (*move_cb)(void *data, const char *type, void *event),
                                        void (*leave_cb)(void *data, const char *type, void *event),
                                        void (*drop_cb)(void *data, const char *type, void *event),
                                        const char **types, unsigned int num_types)
{
   E_Gadget_Config *zgc;
   int x, y, w, h;
   Evas_Object *drop_object;
   E_Drop_Handler *drop_handler;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);

   if (!zgc->drop_handlers)
     zgc->drop_handlers = eina_hash_pointer_new((Eina_Free_Cb)e_drop_handler_del);

   evas_object_geometry_get(zgc->display, &x, &y, &w, &h);
   drop_handler = e_drop_handler_add(zgc->e_obj_inherit, NULL, data,
                        enter_cb, move_cb, leave_cb, drop_cb,
                        types, num_types, x, y, w, h);
   drop_object = evas_object_rectangle_add(e_comp->evas);
   e_comp_object_util_del_list_append(g, drop_object);
   drop_handler->base = drop_object;
   evas_object_color_set(drop_object, 0, 0, 0, 0);
   e_object_data_set(E_OBJECT(drop_handler), drop_object);
   evas_object_data_set(drop_object, "gadget_drop_handler", drop_handler);
   evas_object_geometry_set(drop_object, x, y, w, h);
   evas_object_pass_events_set(drop_object, 1);
   evas_object_layer_set(drop_object, evas_object_layer_get(zgc->display));
   evas_object_event_callback_add(drop_object, EVAS_CALLBACK_MOVE, _gadget_drop_handler_moveresize, drop_handler);
   evas_object_event_callback_add(drop_object, EVAS_CALLBACK_RESIZE, _gadget_drop_handler_moveresize, drop_handler);
   evas_object_event_callback_add(drop_object, EVAS_CALLBACK_DEL, _gadget_drop_handler_del, zgc);
   eina_hash_add(zgc->drop_handlers, &drop_object, drop_handler);
   return drop_object;
}

E_API Evas_Object *
e_gadget_util_layout_style_init(Evas_Object *g, Evas_Object *style)
{
   E_Gadget_Config *zgc;
   Evas_Object *prev;
   const char *grp;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);

   prev = zgc->style.obj;
   zgc->style.obj = style;
   if (style)
     {
        elm_layout_file_get(style, NULL, &grp);
        eina_stringshare_replace(&zgc->style.name, strrchr(grp, '/') + 1);
        evas_object_event_callback_add(style, EVAS_CALLBACK_DEL, _gadget_del, zgc);
     }
   else
     eina_stringshare_replace(&zgc->style.name, NULL);
   if (prev)
     evas_object_event_callback_del_full(prev, EVAS_CALLBACK_DEL, _gadget_del, zgc);
   e_config_save_queue();
   zgc->display = style ?: zgc->gadget;
   E_FILL(zgc->display);
   E_EXPAND(zgc->display);
   if (zgc->site->gravity)
     {
        elm_box_pack_before(zgc->site->layout, zgc->display, prev ?: zgc->gadget);
        if (prev)
          elm_box_unpack(zgc->site->layout, prev);
     }
   evas_object_raise(zgc->site->events);
   if (!style) return prev;

   evas_object_data_set(style, "__e_gadget", zgc);

   elm_layout_sizing_eval(style);
   evas_object_smart_calculate(style);
   evas_object_size_hint_min_get(style, &zgc->style.minw, &zgc->style.minh);
   evas_object_show(style);
   evas_object_smart_callback_add(zgc->display, "gadget_popup", _gadget_popup, zgc->site);
   return prev;
}

static void
_gadget_util_ctxpopup_visibility(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_comp_shape_queue();
}

static void
_gadget_util_ctxpopup_moveresize(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_comp_shape_queue();
}

E_API void
e_gadget_util_ctxpopup_place(Evas_Object *g, Evas_Object *ctx, Evas_Object *pos_obj)
{
   int x, y, w, h;
   E_Layer layer;
   E_Gadget_Config *zgc;
   Evas_Object *content;

   EINA_SAFETY_ON_NULL_RETURN(g);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN(zgc);

   content = elm_object_content_get(ctx);
   elm_ctxpopup_hover_parent_set(ctx, e_comp->elm);
   layer = MAX(evas_object_layer_get(pos_obj ?: g), E_LAYER_POPUP);
   evas_object_layer_set(ctx, layer);

   evas_object_geometry_get(pos_obj ?: g, &x, &y, &w, &h);
   if (zgc->site->anchor & E_GADGET_SITE_ANCHOR_TOP)
     y += h;
   if (zgc->site->anchor & E_GADGET_SITE_ANCHOR_LEFT)
     x += w;
   if (zgc->site->orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
     {
        x += w / 2;
        elm_ctxpopup_direction_priority_set(ctx, ELM_CTXPOPUP_DIRECTION_UP, ELM_CTXPOPUP_DIRECTION_DOWN, 0, 0);
     }
   else if (zgc->site->orient == E_GADGET_SITE_ORIENT_VERTICAL)
     {
        y += h / 2;
        elm_ctxpopup_direction_priority_set(ctx, ELM_CTXPOPUP_DIRECTION_RIGHT, ELM_CTXPOPUP_DIRECTION_LEFT, 0, 0);
     }
   evas_object_move(ctx, x, y);
   evas_object_event_callback_add(ctx, EVAS_CALLBACK_SHOW, _gadget_util_ctxpopup_visibility, NULL);
   evas_object_event_callback_add(ctx, EVAS_CALLBACK_HIDE, _gadget_util_ctxpopup_visibility, NULL);
   if (content)
     {
        evas_object_event_callback_add(content, EVAS_CALLBACK_MOVE, _gadget_util_ctxpopup_moveresize, NULL);
        evas_object_event_callback_add(content, EVAS_CALLBACK_RESIZE, _gadget_util_ctxpopup_moveresize, NULL);
     }
   evas_object_smart_callback_call(zgc->site->layout, "gadget_site_popup", ctx);
   if (evas_object_visible_get(ctx))
     e_comp_shape_queue();
}

static void
_gadget_util_allow(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Gadget_Config *zgc = data;

   zgc->allow_deny.allow(zgc->allow_deny.data, zgc->gadget, NULL);
   _gadget_util_allow_deny_cleanup(zgc);
}

static void
_gadget_util_deny(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Gadget_Config *zgc = data;

   zgc->allow_deny.deny(zgc->allow_deny.data, zgc->gadget, NULL);
   _gadget_util_allow_deny_cleanup(zgc);
}

E_API void
e_gadget_util_allow_deny_ctxpopup(Evas_Object *g, const char *text, Evas_Smart_Cb allow_cb, Evas_Smart_Cb deny_cb, const void *data)
{
   Evas_Object *ctx, *tb, *lbl, *bt;
   E_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN(g);
   zgc = evas_object_data_get(g, "__e_gadget");
   EINA_SAFETY_ON_NULL_RETURN(zgc);

   /* FIXME */
   EINA_SAFETY_ON_TRUE_RETURN(!!zgc->allow_deny.popup);

   EINA_SAFETY_ON_NULL_RETURN(allow_cb);
   EINA_SAFETY_ON_NULL_RETURN(deny_cb);
   EINA_SAFETY_ON_NULL_RETURN(text);

   zgc->allow_deny.allow = allow_cb;
   zgc->allow_deny.deny = deny_cb;
   zgc->allow_deny.data = (void*)data;

   ctx = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(ctx, "noblock");

   tb = elm_table_add(ctx);
   E_EXPAND(tb);
   E_FILL(tb);
   evas_object_show(tb);
   lbl = elm_label_add(ctx);
   E_FILL(lbl);
   elm_object_text_set(lbl, text);
   evas_object_show(lbl);
   elm_table_pack(tb, lbl, 0, 0, 2, 2);

   bt = elm_button_add(ctx);
   evas_object_show(bt);
   E_EXPAND(bt);
   E_FILL(bt);
   elm_object_text_set(bt, _("Deny"));
   evas_object_smart_callback_add(bt, "clicked", _gadget_util_deny, zgc);
   elm_table_pack(tb, bt, 0, 2, 1, 1);

   bt = elm_button_add(ctx);
   evas_object_show(bt);
   E_EXPAND(bt);
   E_FILL(bt);
   elm_object_text_set(bt, _("Allow"));
   evas_object_smart_callback_add(bt, "clicked", _gadget_util_allow, zgc);
   elm_table_pack(tb, bt, 1, 2, 1, 1);

   elm_object_content_set(ctx, tb);

   e_gadget_util_ctxpopup_place(g, ctx, NULL);
   zgc->allow_deny.popup = e_comp_object_util_add(ctx, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(zgc->allow_deny.popup, E_LAYER_POPUP);
   evas_object_show(zgc->allow_deny.popup);
}

EINTERN void
e_gadget_save(void)
{
   e_config_domain_save("e_gadget_sites", edd_sites, sites);
}

EINTERN void
e_gadget_site_rename(const char *name, const char *newname)
{
   Eina_List *l;
   E_Gadget_Site *zgs;

   EINA_LIST_FOREACH(sites->sites, l, zgs)
     if (eina_streq(zgs->name, name))
       {
          eina_stringshare_replace(&zgs->name, newname);
          e_config_save_queue();
          break;
       }
}

EINTERN void
e_gadget_init(void)
{
   gadget_types = eina_hash_string_superfast_new(free);
   edd_gadget = E_CONFIG_DD_NEW("E_Gadget_Config", E_Gadget_Config);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, id, INT);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, type, STR);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, style.name, STR);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, x, DOUBLE);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, y, DOUBLE);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, w, DOUBLE);
   E_CONFIG_VAL(edd_gadget, E_Gadget_Config, h, DOUBLE);

   edd_site = E_CONFIG_DD_NEW("E_Gadget_Site", E_Gadget_Site);
   E_CONFIG_VAL(edd_site, E_Gadget_Site, gravity, UINT);
   E_CONFIG_VAL(edd_site, E_Gadget_Site, orient, UINT);
   E_CONFIG_VAL(edd_site, E_Gadget_Site, anchor, UINT);
   E_CONFIG_VAL(edd_site, E_Gadget_Site, autoadd, UCHAR);
   E_CONFIG_VAL(edd_site, E_Gadget_Site, name, STR);

   E_CONFIG_LIST(edd_site, E_Gadget_Site, gadgets, edd_gadget);

   edd_sites = E_CONFIG_DD_NEW("E_Gadget_Sites", E_Gadget_Sites);
   E_CONFIG_LIST(edd_sites, E_Gadget_Sites, sites, edd_site);
   sites = e_config_domain_load("e_gadget_sites", edd_sites);
   if (sites)
     {
        Eina_List *l;
        E_Gadget_Site *zgs;

        EINA_LIST_FOREACH(sites->sites, l, zgs)
          {
             Eina_List *ll;
             E_Gadget_Config *zgc;

             EINA_LIST_FOREACH(zgs->gadgets, ll, zgc)
               {
                  zgs->gadget_list = eina_inlist_append(zgs->gadget_list, EINA_INLIST_GET(zgc));
                  zgc->site = zgs;
               }
          }
     }
   else
     sites = E_NEW(E_Gadget_Sites, 1);

   move_act = e_action_add("gadget_move");
   e_action_predef_name_set(_("Gadgets"), _("Move gadget"), "gadget_move", NULL, NULL, 0);
   move_act->func.go_mouse = _gadget_act_move;

   resize_act = e_action_add("gadget_resize");
   e_action_predef_name_set(_("Gadgets"), _("Resize gadget"), "gadget_resize", NULL, NULL, 0);
   resize_act->func.go_mouse = _gadget_act_resize;
   resize_act->func.end_mouse = _gadget_act_resize_end;

   configure_act = e_action_add("gadget_configure");
   e_action_predef_name_set(_("Gadgets"), _("Configure gadget"), "gadget_configure", NULL, NULL, 0);
   configure_act->func.go_mouse = _gadget_act_configure;

   menu_act = e_action_add("gadget_menu");
   e_action_predef_name_set(_("Gadgets"), _("Gadget menu"), "gadget_menu", NULL, NULL, 0);
   menu_act->func.go_mouse = _gadget_act_menu;

   comp_add_handler = ecore_event_handler_add(E_EVENT_COMP_OBJECT_ADD, (Ecore_Event_Handler_Cb)_site_auto_comp_object_handler, NULL);

   comp_site = e_comp->canvas->gadget_site = e_gadget_site_add(E_GADGET_SITE_ORIENT_NONE, "__desktop");
   evas_object_event_callback_add(e_comp->canvas->bg_blank_object, EVAS_CALLBACK_RESIZE, _comp_site_resize, comp_site);
   evas_object_layer_set(comp_site, E_LAYER_DESKTOP);
   evas_object_resize(comp_site, e_comp->w, e_comp->h);
}

EINTERN void
e_gadget_shutdown(void)
{
   E_Gadget_Site *zgs;

   E_FREE_FUNC(comp_add_handler, ecore_event_handler_del);
   E_CONFIG_DD_FREE(edd_gadget);
   E_CONFIG_DD_FREE(edd_site);
   E_CONFIG_DD_FREE(edd_sites);
   EINA_LIST_FREE(sites->sites, zgs)
     {
        E_FREE_LIST(zgs->gadgets, _gadget_free);
        evas_object_del(zgs->layout);
        eina_stringshare_del(zgs->name);
        free(zgs);
     }
   E_FREE(sites);

   e_action_del("gadget_move");
   e_action_del("gadget_resize");
   e_action_del("gadget_configure");
   e_action_del("gadget_menu");
   e_action_predef_name_del(_("Gadgets"), _("Move gadget"));
   e_action_predef_name_del(_("Gadgets"), _("Resize gadget"));
   e_action_predef_name_del(_("Gadgets"), _("Configure gadget"));
   e_action_predef_name_del(_("Gadgets"), _("Gadget menu"));
   move_act = resize_act = configure_act = menu_act = NULL;
   E_FREE_FUNC(gadget_types, eina_hash_free);
}

//////////////////////////////////////////////////////
////         EDITOR                              /////
//////////////////////////////////////////////////////

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *list = data;
   E_FREE_LIST(list, free);
}

static void
_editor_pointer_site_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   e_comp_ungrab_input(1, 1);
   free(data);
   pointer_site = NULL;
   E_FREE_LIST(handlers, ecore_event_handler_del);
}

static void
_editor_site_hints(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int x, y, w, h;

   evas_object_size_hint_min_get(obj, &w, &h);
   evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   evas_object_geometry_set(pointer_site, x - (w / 2), y - (h / 2), w, h);
}

static Eina_Bool
_editor_pointer_button(Gadget_Item *active, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   int x, y, w, h;
   E_Gadget_Site *zgs, *zzgs = NULL;

   if (active->site)
     {
        evas_object_geometry_get(active->site, &x, &y, &w, &h);
        if ((ev->buttons == 1) && E_INSIDE(ev->x, ev->y, x, y, w, h))
          evas_object_smart_callback_call(active->site, "gadget_site_dropped", pointer_site);
        evas_object_pass_events_set(active->site, 0);
        elm_object_disabled_set(active->editor, 1);
        e_comp_object_util_del_list_remove(active->editor, pointer_site);
     }
   else
     {
        E_Gadget_Config *zgc, *z;
        Eina_List *l;
        
        if (ev->buttons == 1)
          {
             EINA_LIST_FOREACH(sites->sites, l, zgs)
               {
                  if (!zgs->layout) continue;
                  if (!evas_object_visible_get(zgs->events)) continue;
                  evas_object_geometry_get(zgs->layout, &x, &y, &w, &h);
                  if (!E_INSIDE(ev->x, ev->y, x, y, w, h)) continue;
                  /* FIXME: technically not accurate since objects on the same layer
                   * will have different stacking, but walking object trees sucks
                   */
                  if ((!zzgs) ||
                    (evas_object_layer_get(zzgs->layout) < evas_object_layer_get(zgs->layout)))
                    zzgs = zgs;
               }
          }
        zgs = evas_object_data_get(pointer_site, "__e_gadget_site");
        zgc = eina_list_data_get(zgs->gadgets);
        evas_object_pass_events_set(zgc->orig->site->layout, 0);
        if (zzgs)
          {
             /* fake the moving gadget as being on the pointer site */
             z = zgc->orig;
             zgc->site->gadget_list = eina_inlist_remove(zgc->site->gadget_list, EINA_INLIST_GET(zgc));
             zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
             evas_object_geometry_get(zgc->display, &x, &y, NULL, NULL);
             evas_object_move(z->display, x, y);
             _gadget_free(zgc);
             z->site->gadget_list = eina_inlist_remove(z->site->gadget_list, EINA_INLIST_GET(z));
             z->site->gadgets = eina_list_remove(z->site->gadgets, z);
             zgs->gadgets = eina_list_append(zgs->gadgets, z);
             z->site = zgs;
             evas_object_smart_callback_call(zzgs->layout, "gadget_site_dropped", pointer_site);
          }
          
     }
   E_FREE_FUNC(pointer_site, evas_object_del);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_editor_pointer_move(Gadget_Item *active EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int w, h;

   evas_object_geometry_get(pointer_site, NULL, NULL, &w, &h);
   evas_object_move(pointer_site, ev->x - (w / 2), ev->y - (h / 2));
   if (!e_gadget_site_orient_get(pointer_site))
     evas_object_smart_need_recalculate_set(pointer_site, 1);
   return ECORE_CALLBACK_RENEW;
}

static void
_editor_pointer_site_init(E_Gadget_Site_Orient orient, Evas_Object *site, Evas_Object *editor, Eina_Bool up)
{
   Gadget_Item *active;
   Evas_Object *rect;
   int size;

   pointer_site = e_gadget_site_add(orient, NULL);
   if (site && (orient == E_GADGET_SITE_ORIENT_HORIZONTAL))
     evas_object_geometry_get(site, NULL, NULL, NULL, &size);
   else if (site && (orient == E_GADGET_SITE_ORIENT_VERTICAL))
     evas_object_geometry_get(site, NULL, NULL, &size, NULL);
   else
     size = 96 * e_scale;
   evas_object_resize(pointer_site, size, size);
   evas_object_layer_set(pointer_site, E_LAYER_MENU);
   evas_object_pass_events_set(pointer_site, 1);
   evas_object_show(pointer_site);
   active = E_NEW(Gadget_Item, 1);
   active->editor = editor;
   active->site = site;
   if (active->site)
     evas_object_pass_events_set(active->site, 1);
   evas_object_event_callback_add(pointer_site, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _editor_site_hints, active);
   evas_object_event_callback_add(pointer_site, EVAS_CALLBACK_DEL, _editor_pointer_site_del, active);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_MOVE, _editor_pointer_move, active);
   E_LIST_HANDLER_APPEND(handlers,
     up ? ECORE_EVENT_MOUSE_BUTTON_UP : ECORE_EVENT_MOUSE_BUTTON_DOWN, _editor_pointer_button, active);

   rect = evas_object_rectangle_add(e_comp->evas);
   evas_object_resize(rect, e_comp->w, e_comp->h);
   evas_object_color_set(rect, 0, 0, 0, 0);
   evas_object_layer_set(rect, E_LAYER_MENU + 1);
   e_comp_object_util_del_list_append(pointer_site, rect);
   evas_object_show(rect);
   e_comp_grab_input(1, 1);
}

static void
_editor_gadget_new(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Gadget_Item *gi = data;
   E_Gadget_Site_Orient orient;

   orient = e_gadget_site_orient_get(gi->site);
   _editor_pointer_site_init(orient, gi->site, gi->editor, 0);
   e_comp_object_util_del_list_append(gi->editor, pointer_site);
   
   e_gadget_site_gadget_add(pointer_site, e_gadget_type_get(gi->gadget), 1);
   elm_object_disabled_set(gi->editor, 1);
   elm_list_item_selected_set(event_info, 0);
}

E_API Evas_Object *
e_gadget_editor_add(Evas_Object *parent, Evas_Object *site)
{
   Evas_Object *list, *tempsite, *g;
   Eina_Iterator *it;
   Eina_List *gadgets, *items = NULL;
   const char *type;

   list = elm_list_add(parent);
   elm_scroller_content_min_limit(list, 1, 1);
   tempsite = e_gadget_site_add(E_GADGET_SITE_ORIENT_HORIZONTAL, NULL);
   e_gadget_site_gravity_set(tempsite, E_GADGET_SITE_GRAVITY_NONE);

   it = e_gadget_type_iterator_get();
   /* FIXME: no types available */
   EINA_ITERATOR_FOREACH(it, type)
     e_gadget_site_gadget_add(tempsite, type, 1);
   eina_iterator_free(it);

   gadgets = e_gadget_site_gadgets_list(tempsite);
   EINA_LIST_FREE(gadgets, g)
     {
        Evas_Object *box, *button = NULL;
        char buf[1024];
        Gadget_Item *gi;

        gi = E_NEW(Gadget_Item, 1);
        gi->editor = list;
        gi->gadget = g;
        gi->site = site;
        items = eina_list_append(items, gi);
        box = elm_box_add(list);
        elm_box_horizontal_set(box, 1);
        E_EXPAND(g);
        E_FILL(g);
        elm_box_pack_end(box, g);
        evas_object_pass_events_set(g, 1);
        strncpy(buf, e_gadget_type_get(g), sizeof(buf) - 1);
        buf[0] = toupper(buf[0]);
        elm_list_item_append(list, buf, box, button, _editor_gadget_new, gi);
        elm_box_recalculate(box);
     }
   evas_object_data_set(list, "__gadget_items", items);
   evas_object_event_callback_add(list, EVAS_CALLBACK_DEL, _editor_del, items);
   elm_list_go(list);
   return list;
}

E_API Evas_Object *
e_gadget_site_edit(Evas_Object *site)
{
   Evas_Object *comp_object, *popup, *editor;
   E_Zone *zone, *czone;

   zone = e_comp_object_util_zone_get(site);
   czone = e_zone_current_get();
   if (zone != czone)
     {
        int x, y, w, h;

        evas_object_geometry_get(site, &x, &y, &w, &h);
        if (E_INTERSECTS(x, y, w, h, czone->x, czone->y, czone->w, czone->h))
          zone = czone;
     }

   popup = elm_popup_add(e_comp->elm);
   elm_popup_allow_events_set(popup, 1);

   editor = e_gadget_editor_add(e_comp->elm, site);
   evas_object_show(editor);
   E_FILL(editor);
   elm_object_content_set(popup, editor);
   comp_object = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(comp_object, E_LAYER_POPUP);
   evas_object_show(comp_object);
   evas_object_resize(comp_object, zone->w / 2, zone->h / 2);
   e_comp_object_util_center_on_zone(comp_object, zone);
   return comp_object;
}

static void
_edit_end()
{
   if (desktop_editor)
     {
        E_Action *act;

        act = e_action_find("desk_deskshow_toggle");
        if (act) act->func.go(E_OBJECT(e_comp_object_util_zone_get(desktop_editor)), NULL);
        evas_object_hide(desktop_editor);
        E_FREE_FUNC(desktop_editor, evas_object_del);
     }
   E_FREE_FUNC(desktop_rect, evas_object_del);
   E_FREE_LIST(desktop_handlers, ecore_event_handler_del);
   e_comp_ungrab_input(1, 1);
}

static void
_gadget_desklock_del(void)
{
   e_desklock_hide();
   _edit_end();
}

static void
_gadget_desklock_clear(void)
{
   Eina_List *l;
   E_Gadget_Site *zgs;

   EINA_LIST_FOREACH(sites->sites, l, zgs)
     if (zgs->autoadd && zgs->layout && strstr(zgs->name, "desklock."))
       {
          E_LIST_FOREACH(zgs->gadgets, _gadget_remove);
       }
   e_config_save_queue();
}

static Eina_Bool
_gadget_desklock_key_handler(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Key *ev)
{
   if (eina_streq(ev->key, "Escape"))
     _gadget_desklock_del();
   else if (eina_streq(ev->key, "Delete") || eina_streq(ev->key, "Backspace"))
     _gadget_desklock_clear();
   return ECORE_CALLBACK_DONE;
}

static void
_gadget_desklock_mouse_up_handler()
{
   if (!added)
     _gadget_desklock_del();
   added = 0;
}

static void
_gadget_moved()
{
   added = 1;
}

static Eina_Bool
_gadget_desklock_handler(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Comp_Object *ev)
{
   E_Notification_Notify n;
   const char *name;
   Evas_Object *site, *comp_object;

   name = evas_object_name_get(ev->comp_object);
   if (!name) return ECORE_CALLBACK_RENEW;
   if (strncmp(name, "desklock", 8)) return ECORE_CALLBACK_RENEW;
   evas_object_layer_set(ev->comp_object, DESKLOCK_DEMO_LAYER - 1);
   site = e_gadget_site_auto_add(E_GADGET_SITE_ORIENT_NONE, name);
   evas_object_smart_callback_add(site, "gadget_moved", _gadget_moved, NULL);
   evas_object_layer_set(site, DESKLOCK_DEMO_LAYER);
   comp_object = e_gadget_site_edit(site);
   e_comp_object_util_del_list_append(ev->comp_object, comp_object);

   memset(&n, 0, sizeof(E_Notification_Notify));
   n.timeout = 3000;
   n.summary = _("Lockscreen Gadgets");
   n.body = _("Press Escape or click the background to exit.<ps/>"
              "Use Backspace or Delete to remove all gadgets from this screen");
   n.urgency = E_NOTIFICATION_NOTIFY_URGENCY_NORMAL;
   e_notification_client_send(&n, NULL, NULL);
   return ECORE_CALLBACK_RENEW;
}

E_API void
e_gadget_site_desklock_edit(void)
{
   desktop_rect = evas_object_rectangle_add(e_comp->evas);
   evas_object_color_set(desktop_rect, 0, 0, 0, 0);
   evas_object_resize(desktop_rect, e_comp->w, e_comp->h);
   evas_object_layer_set(desktop_rect, DESKLOCK_DEMO_LAYER);
   evas_object_show(desktop_rect);
   E_LIST_HANDLER_APPEND(desktop_handlers, E_EVENT_COMP_OBJECT_ADD, _gadget_desklock_handler, NULL);
   E_LIST_HANDLER_APPEND(desktop_handlers, ECORE_EVENT_KEY_DOWN, _gadget_desklock_key_handler, NULL);
   E_LIST_HANDLER_APPEND(desktop_handlers, ECORE_EVENT_MOUSE_BUTTON_UP, _gadget_desklock_mouse_up_handler, NULL);
   e_desklock_demo();
   e_comp_grab_input(1, 1);
}

static Eina_Bool
_gadget_desktop_key_handler(void *data, int t EINA_UNUSED, Ecore_Event_Key *ev)
{
   if (eina_streq(ev->key, "Escape"))
     _edit_end();
   else if (eina_streq(ev->key, "Delete") || eina_streq(ev->key, "Backspace"))
     {
        E_Gadget_Site *zgs = data;
        E_LIST_FOREACH(zgs->gadgets, _gadget_remove);
     }
   return ECORE_CALLBACK_DONE;
}

static void
_gadget_desktop_mouse_up_handler()
{
   if (!added)
     _edit_end();
   added = 0;
}

E_API void
e_gadget_site_desktop_edit(Evas_Object *site)
{
   E_Action *act;
   E_Notification_Notify n;

   ZGS_GET(site);

   desktop_rect = evas_object_rectangle_add(e_comp->evas);
   evas_object_event_callback_add(desktop_rect, EVAS_CALLBACK_DEL, _edit_end, NULL);
   evas_object_color_set(desktop_rect, 0, 0, 0, 0);
   evas_object_resize(desktop_rect, e_comp->w, e_comp->h);
   evas_object_layer_set(desktop_rect, E_LAYER_DESKTOP);
   evas_object_show(desktop_rect);
   E_LIST_HANDLER_APPEND(desktop_handlers, ECORE_EVENT_KEY_DOWN, _gadget_desktop_key_handler, zgs);
   E_LIST_HANDLER_APPEND(desktop_handlers, ECORE_EVENT_MOUSE_BUTTON_UP, _gadget_desktop_mouse_up_handler, NULL);
   evas_object_event_callback_add(site, EVAS_CALLBACK_DEL, _edit_end, NULL);

   desktop_editor = e_gadget_site_edit(site);
   evas_object_smart_callback_add(site, "gadget_moved", _gadget_moved, NULL);
   evas_object_show(desktop_editor);

   act = e_action_find("desk_deskshow_toggle");
   if (act) act->func.go(E_OBJECT(e_comp_object_util_zone_get(desktop_editor)), NULL);
   e_comp_grab_input(1, 1);

   memset(&n, 0, sizeof(E_Notification_Notify));
   n.timeout = 3000;
   n.summary = _("Desktop Gadgets");
   n.body = _("Press Escape or click the background to exit.<ps/>"
              "Use Backspace or Delete to remove all gadgets from this site");
   n.urgency = E_NOTIFICATION_NOTIFY_URGENCY_NORMAL;
   e_notification_client_send(&n, NULL, NULL);
}
