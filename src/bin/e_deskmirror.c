#include "e.h"

#define INTERNAL_ENTRY E_Smart_Data * sd; sd = evas_object_smart_data_get(obj); if (!sd) return;

typedef struct E_Smart_Data
{
   Evas *e;
   Evas_Object *obj;
   Evas_Object *clip;
   Evas_Object *bgpreview;
   Evas_Object *layout;
   Eina_Inlist *mirrors;
   Eina_Hash *mirror_hash;

   Eina_List *handlers;

   Evas_Coord x, y;
   int w, h;

   E_Desk *desk;
   E_Object_Delfn *desk_delfn;

   Eina_Bool pager : 1;
   Eina_Bool taskbar : 1;

   Eina_Bool resize : 1;
} E_Smart_Data;

typedef struct Mirror
{
   EINA_INLIST;
   E_Smart_Data *sd;
   E_Comp_Win *cw;
   Evas_Object *mirror;
   int x, y, w, h;
   Eina_Bool frame : 1;
} Mirror;

typedef struct Mirror_Border
{
   Mirror *m;
   Evas_Object *mirror;
   Evas_Object *frame;
   Evas_Object *obj;
} Mirror_Border;

/* local subsystem globals */
static Evas_Smart *_e_deskmirror_smart = NULL;
static Evas_Smart *_mirror_border_smart = NULL;

/* local subsystem functions */
static void
_mirror_scale_set(Mirror *m, float sc)
{
   Edje_Message_Float_Set msg;
   Mirror_Border *mb;

   if (!m->frame) return;

   mb = evas_object_smart_data_get(m->mirror);
   msg.count = 1;
   msg.val[0] = sc;
   edje_object_message_send(mb->frame, EDJE_MESSAGE_FLOAT_SET, 0, &msg);
}

static void
_e_deskmirror_smart_reconfigure(E_Smart_Data *sd)
{
   e_layout_freeze(sd->layout);
   evas_object_move(sd->clip, sd->x, sd->y);
   evas_object_move(sd->bgpreview, sd->x, sd->y);
   evas_object_move(sd->layout, sd->x, sd->y);

   if (sd->resize)
     {
        Mirror *m;

        evas_object_resize(sd->clip, sd->w, sd->h);
        evas_object_resize(sd->bgpreview, sd->w, sd->h);
        evas_object_resize(sd->layout, sd->w, sd->h);
        EINA_INLIST_FOREACH(sd->mirrors, m)
          _mirror_scale_set(m, (float)sd->h / (float)sd->desk->zone->h);
     }
   e_layout_thaw(sd->layout);
   sd->resize = 0;
}

///////////////////////////////////////////////

static void
_e_deskmirror_smart_add(Evas_Object *obj)
{
   E_Smart_Data *sd;

   sd = E_NEW(E_Smart_Data, 1);
   if (!sd) return;
   sd->obj = obj;
   sd->e = evas_object_evas_get(obj);
   sd->x = sd->y = sd->w = sd->h = 0;
   sd->clip = evas_object_rectangle_add(sd->e);
   evas_object_smart_member_add(sd->clip, sd->obj);
   evas_object_smart_data_set(obj, sd);
}

static void
_e_deskmirror_smart_del(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   if (sd->desk_delfn)
     {
        e_object_delfn_del(E_OBJECT(sd->desk), sd->desk_delfn);
        sd->desk_delfn = NULL;
        sd->desk = NULL;
     }
   E_FREE_LIST(sd->handlers, ecore_event_handler_del);
   eina_hash_free(sd->mirror_hash);
   free(sd);
}

static void
_e_deskmirror_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   INTERNAL_ENTRY;
   sd->x = x;
   sd->y = y;
   _e_deskmirror_smart_reconfigure(sd);
}

static void
_e_deskmirror_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   INTERNAL_ENTRY;
   sd->w = w;
   sd->h = h;
   sd->resize = 1;
   _e_deskmirror_smart_reconfigure(sd);
}

static void
_e_deskmirror_smart_show(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   evas_object_show(sd->clip);
}

static void
_e_deskmirror_smart_hide(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   evas_object_hide(sd->clip);
}

static void
_e_deskmirror_smart_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   INTERNAL_ENTRY;
   evas_object_color_set(sd->clip, r, g, b, a);
}

static void
_e_deskmirror_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   INTERNAL_ENTRY;
   evas_object_clip_set(sd->clip, clip);
}

static void
_e_deskmirror_smart_clip_unset(Evas_Object *obj)
{
   INTERNAL_ENTRY;
   evas_object_clip_unset(sd->clip);
}

////////////////////////////////////////////////////////

static void
_e_deskmirror_mirror_frame_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Mirror_Border *mb = data;

   if (mb->m->cw->bd && (!e_object_is_del(E_OBJECT(mb->m->cw->bd))))
     {
        evas_object_smart_member_del(mb->mirror);
        mb->m->mirror = mb->mirror;
        mb->m->frame = 0;
     }
   else
     mb->m->cw = NULL;
   evas_object_del(mb->obj);
}

static void
_mirror_border_smart_add(Evas_Object *obj)
{
   Mirror_Border *mb;

   mb = E_NEW(Mirror_Border, 1);
   mb->obj = obj;
   evas_object_smart_data_set(obj, mb);
}

static void
_mirror_border_signal_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission, const char *src)
{
   Mirror_Border *mb = data;
   edje_object_signal_emit(mb->frame, emission, src);
   edje_object_message_signal_process(mb->frame);
   edje_object_calc_force(mb->frame);
}

static void
_mirror_border_smart_del(Evas_Object *obj)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   if (mb->m->cw && mb->m->cw->bd)
     {
        evas_object_event_callback_del_full(mb->m->cw->bd->bg_object, EVAS_CALLBACK_DEL, _e_deskmirror_mirror_frame_del_cb, mb);
        edje_object_signal_callback_del_full(mb->m->cw->bd->bg_object, "*", "*", _mirror_border_signal_cb, mb);
     }
   free(mb);
}

static void
_mirror_border_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);

   evas_object_move(mb->frame, x, y);
}

static void
_mirror_border_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   evas_object_resize(mb->frame, w, h);
}

static void
_mirror_border_smart_show(Evas_Object *obj)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   evas_object_show(mb->frame);
   evas_object_show(mb->mirror);
}

static void
_mirror_border_smart_hide(Evas_Object *obj)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   evas_object_hide(mb->frame);
   evas_object_hide(mb->mirror);
}

static void
_mirror_border_smart_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   evas_object_color_set(mb->frame, r, g, b, a);
   evas_object_color_set(mb->mirror, r, g, b, a);
}

static void
_mirror_border_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   evas_object_clip_set(mb->frame, clip);
   evas_object_clip_set(mb->mirror, clip);
}

static void
_mirror_border_smart_clip_unset(Evas_Object *obj)
{
   Mirror_Border *mb = evas_object_smart_data_get(obj);
   evas_object_clip_unset(mb->frame);
   evas_object_clip_unset(mb->mirror);
}

static void
_mirror_border_smart_init(void)
{
   static const Evas_Smart_Class sc =
     {
        "mirror_border", EVAS_SMART_CLASS_VERSION,
        _mirror_border_smart_add, _mirror_border_smart_del, _mirror_border_smart_move, _mirror_border_smart_resize,
        _mirror_border_smart_show, _mirror_border_smart_hide, _mirror_border_smart_color_set, _mirror_border_smart_clip_set,
        _mirror_border_smart_clip_unset, NULL, NULL, NULL, NULL, NULL, NULL, NULL
     };
   if (_mirror_border_smart) return;
   _mirror_border_smart = evas_smart_class_new(&sc);
}

static void
_e_deskmirror_smart_init(void)
{
   static const Evas_Smart_Class sc =
     {
        "e_deskmirror", EVAS_SMART_CLASS_VERSION,
        _e_deskmirror_smart_add, _e_deskmirror_smart_del, _e_deskmirror_smart_move, _e_deskmirror_smart_resize,
        _e_deskmirror_smart_show, _e_deskmirror_smart_hide, _e_deskmirror_smart_color_set, _e_deskmirror_smart_clip_set,
        _e_deskmirror_smart_clip_unset, NULL, NULL, NULL, NULL, NULL, NULL, NULL
     };
   if (_e_deskmirror_smart) return;
   _e_deskmirror_smart = evas_smart_class_new(&sc);
}

static void
_e_deskmirror_delfn(E_Smart_Data *sd, void *desk EINA_UNUSED)
{
   sd->desk_delfn = NULL;
   sd->desk = NULL;
   evas_object_del(sd->obj);
}

static Eina_Bool
_e_deskmirror_win_visible_get(E_Smart_Data *sd, E_Comp_Win *cw)
{
   Eina_Bool visible = cw->visible;
   if (cw->bd)
     {
        if (visible)
          {
             if (sd->pager)
               visible = !cw->bd->client.netwm.state.skip_pager;
             if (visible && sd->taskbar)
               visible = !cw->bd->client.netwm.state.skip_taskbar;
          }
     }
   return visible;
}

static void
_e_deskmirror_mirror_del(Mirror *m)
{
   eina_hash_del_by_key(m->sd->mirror_hash, &m->cw);
}

static void
_e_deskmirror_mirror_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _e_deskmirror_mirror_del(data);
}

static void
_e_deskmirror_mirror_del_hash(Mirror *m)
{
   m->sd->mirrors = eina_inlist_remove(m->sd->mirrors, EINA_INLIST_GET(m));
   evas_object_event_callback_del_full(m->mirror, EVAS_CALLBACK_DEL, _e_deskmirror_mirror_del_cb, m);
   evas_object_del(m->mirror);
   free(m);
}

static void
_e_deskmirror_mirror_geometry_get(Mirror *m)
{
   if (m->cw->bd)
     {
        m->x = m->cw->bd->x;
        m->y = m->cw->bd->y;
        m->w = m->cw->bd->w;
        if (m->cw->bd->shaded)
          m->h = m->cw->bd->client_inset.t;
        else
          m->h = m->cw->bd->h;
     }
   else if (m->cw->not_in_layout)
     evas_object_geometry_get(m->cw->effect_obj, &m->x, &m->y, &m->w, &m->h);
   else
     e_layout_child_geometry_get(m->cw->effect_obj, &m->x, &m->y, &m->w, &m->h);
   /* double check here if we get zeroes */
   if ((!m->w) || (!m->h))
     {
        m->w = m->cw->w, m->h = m->cw->h;
        m->x = m->cw->x, m->y = m->cw->y;
     }
}

static void
_e_deskmirror_mirror_reconfigure(Mirror *m)
{
   _e_deskmirror_mirror_geometry_get(m);
   e_layout_child_move(m->mirror, m->x, m->y);
   e_layout_child_resize(m->mirror, m->w, m->h);
   if (_e_deskmirror_win_visible_get(m->sd, m->cw) && m->w && m->h)
     evas_object_show(m->mirror);
   else
     evas_object_hide(m->mirror);
}

static Evas_Object *
_mirror_border_new(Mirror *m)
{
   Evas_Object *o;
   Mirror_Border *mb;
   char buf[4096];

   _mirror_border_smart_init();
   o = evas_object_smart_add(m->sd->e, _mirror_border_smart);
   mb = evas_object_smart_data_get(o);
   mb->m = m;
   mb->frame = edje_object_add(m->sd->e);
   evas_object_name_set(mb->frame, "mirror_border");
   snprintf(buf, sizeof(buf), "e/deskmirror/frame/%s", m->cw->bd->client.border.name);
   e_theme_edje_object_set(mb->frame, "base/theme/borders", buf);
   if (e_util_border_shadow_state_get(m->cw->bd))
     edje_object_signal_emit(mb->frame, "e,state,shadow,on", "e");
   else
     edje_object_signal_emit(mb->frame, "e,state,shadow,off", "e");
   edje_object_signal_callback_add(mb->m->cw->bd->bg_object, "*", "*", _mirror_border_signal_cb, mb);
   if (e_border_focused_get() == mb->m->cw->bd)
     edje_object_signal_emit(mb->frame, "e,state,focused", "e");
   if (mb->m->cw->bd->shaded)
     edje_object_signal_emit(mb->frame, "e,state,shaded", "e");
   if (mb->m->cw->bd->maximized)
     edje_object_signal_emit(mb->frame, "e,action,maximize", "e");
   if (mb->m->cw->bd->sticky)
     edje_object_signal_emit(mb->frame, "e,state,sticky", "e");

   mb->mirror = m->mirror;
   evas_object_smart_member_add(mb->frame, o);
   evas_object_name_set(mb->mirror, "mirror");
   evas_object_smart_member_add(mb->mirror, o);
   edje_object_part_swallow(mb->frame, "e.swallow.client", m->mirror);
   edje_object_part_text_set(mb->frame, "e.text.title", m->cw->bd->client.netwm.name ?: m->cw->bd->client.icccm.name);
   _mirror_scale_set(m, (double)m->sd->h / (double)m->sd->desk->zone->h);
   evas_object_event_callback_add(m->cw->bd->bg_object, EVAS_CALLBACK_DEL, _e_deskmirror_mirror_frame_del_cb, mb);
   return o;
}

static void
_e_deskmirror_mirror_setup(Mirror *m)
{
   if (!m->mirror) return;
   if (m->cw->bd && m->cw->bd->bg_object)
     {
        m->mirror = _mirror_border_new(m);
        m->frame = 1;
     }
   else
     evas_object_pass_events_set(m->mirror, 1);
   e_layout_pack(m->sd->layout, m->mirror);
   evas_object_event_callback_add(m->mirror, EVAS_CALLBACK_DEL, _e_deskmirror_mirror_del_cb, m);
   _e_deskmirror_mirror_reconfigure(m);
}

static Mirror *
_e_deskmirror_mirror_add(E_Smart_Data *sd, E_Comp_Win *cw)
{
   Mirror *m;
   Evas_Object *o = NULL;

   if (cw->bd)
     {
        if ((cw->bd->zone != sd->desk->zone) || ((cw->bd->desk != sd->desk) && (!cw->bd->sticky)))
          return NULL;
     }
   else
     {
        int x, y;

        if (!sd->desk->visible) return NULL;
        if (cw->not_in_layout)
          evas_object_geometry_get(cw->effect_obj, &x, &y, NULL, NULL);
        else
          e_layout_child_geometry_get(cw->effect_obj, &x, &y, NULL, NULL);
        if (!E_INSIDE(x, y, sd->desk->zone->x, sd->desk->zone->y, sd->desk->zone->w, sd->desk->zone->h)) return NULL;
     }
   if ((cw->w > 1) && (cw->h > 1))
     {
        o = e_comp_win_image_mirror_add(cw);
        if (!o) return NULL;
     }
   m = calloc(1, sizeof(Mirror));
   m->cw = cw;
   m->sd = sd;
   m->mirror = o;
   sd->mirrors = eina_inlist_append(sd->mirrors, EINA_INLIST_GET(m));
   eina_hash_direct_add(sd->mirror_hash, &m->cw, m);
   _e_deskmirror_mirror_setup(m);
   return m;
}

static Eina_Bool
_comp_source_add(E_Smart_Data *sd, int type EINA_UNUSED, E_Event_Comp *ev)
{
   if (eina_hash_find(sd->mirror_hash, &ev->cw)) return ECORE_CALLBACK_RENEW;
   _e_deskmirror_mirror_add(sd, ev->cw);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_comp_source_visible(E_Smart_Data *sd, int type EINA_UNUSED, E_Event_Comp *ev)
{
   Mirror *m;

   m = eina_hash_find(sd->mirror_hash, &ev->cw);
   if (!m) return ECORE_CALLBACK_RENEW;
   if (!m->mirror)
     {
        if ((m->cw->w < 2) || (m->cw->h < 2)) return ECORE_CALLBACK_RENEW;
        m->mirror = e_comp_win_image_mirror_add(m->cw);
        _e_deskmirror_mirror_setup(m);
     }
   if (_e_deskmirror_win_visible_get(m->sd, m->cw))
     evas_object_show(m->mirror);
   else
     evas_object_hide(m->mirror);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_comp_source_stack(E_Smart_Data *sd, int type EINA_UNUSED, E_Event_Comp *ev)
{
   Mirror *m, *m2;
   E_Comp_Win *cw;

   m = eina_hash_find(sd->mirror_hash, &ev->cw);
   if (!m) return ECORE_CALLBACK_RENEW;
   if (!m->mirror)
     {
        if ((m->cw->w < 2) || (m->cw->h < 2)) return ECORE_CALLBACK_RENEW;
        m->mirror = e_comp_win_image_mirror_add(m->cw);
        _e_deskmirror_mirror_setup(m);
     }
   if (!EINA_INLIST_GET(ev->cw)->next)
     e_layout_child_raise(m->mirror);
   else if (!EINA_INLIST_GET(ev->cw)->prev)
     e_layout_child_lower(m->mirror);
   else
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(ev->cw)->next, cw)
          {
             m2 = eina_hash_find(sd->mirror_hash, &cw);
             if ((!m2) || (!m2->mirror)) continue;
             e_layout_child_lower_below(m->mirror, m2->mirror);
             return ECORE_CALLBACK_RENEW;
          }
        e_layout_child_raise(m->mirror);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_comp_source_configure(E_Smart_Data *sd, int type EINA_UNUSED, E_Event_Comp *ev)
{
   Mirror *m;

   m = eina_hash_find(sd->mirror_hash, &ev->cw);
   if (m)
     {
        if (!m->mirror)
          {
             if ((m->cw->w < 2) || (m->cw->h < 2)) return ECORE_CALLBACK_RENEW;
             m->mirror = e_comp_win_image_mirror_add(m->cw);
             _e_deskmirror_mirror_setup(m);
          }
        _e_deskmirror_mirror_reconfigure(m);
     }
   return ECORE_CALLBACK_RENEW;
}

/* externally accessible functions */
EAPI Evas_Object *
e_deskmirror_add(E_Desk *desk)
{
   E_Smart_Data *sd;
   Evas_Object *o;
   Evas *e;
   E_Comp_Win *cw;

   e = e_comp_get(desk)->evas;
   _e_deskmirror_smart_init();
   o = evas_object_smart_add(e, _e_deskmirror_smart);
   sd = evas_object_smart_data_get(o);
   sd->desk = desk;
   sd->mirror_hash = eina_hash_pointer_new((Eina_Free_Cb)_e_deskmirror_mirror_del_hash);
   sd->desk_delfn = e_object_delfn_add(E_OBJECT(desk), (Ecore_End_Cb)_e_deskmirror_delfn, sd);
   sd->bgpreview = e_widget_bgpreview_desk_add(e, desk->zone, desk->x, desk->y);
   evas_object_clip_set(sd->bgpreview, sd->clip);
   evas_object_smart_member_add(sd->bgpreview, o);
   evas_object_show(sd->bgpreview);
   sd->layout = e_layout_add(e);
   evas_object_clip_set(sd->layout, sd->clip);
   e_layout_virtual_size_set(sd->layout, desk->zone->w, desk->zone->h);
   evas_object_smart_member_add(sd->layout, o);
   evas_object_show(sd->layout);

   e_layout_freeze(sd->layout);

   EINA_INLIST_FOREACH(e_comp_get(desk)->wins, cw)
     {
        Mirror *m;

        m = _e_deskmirror_mirror_add(sd, cw);
        if (m) e_layout_child_raise(m->mirror);
     }

   e_layout_thaw(sd->layout);

   E_LIST_HANDLER_APPEND(sd->handlers, E_EVENT_COMP_SOURCE_ADD, _comp_source_add, sd);
   E_LIST_HANDLER_APPEND(sd->handlers, E_EVENT_COMP_SOURCE_CONFIGURE, _comp_source_configure, sd);
   E_LIST_HANDLER_APPEND(sd->handlers, E_EVENT_COMP_SOURCE_STACK, _comp_source_stack, sd);
   E_LIST_HANDLER_APPEND(sd->handlers, E_EVENT_COMP_SOURCE_VISIBILITY, _comp_source_visible, sd);
   return o;
}

EAPI void
e_deskmirror_util_wins_print(Evas_Object *obj)
{
   E_Smart_Data *sd;
   Mirror *m;

   EINA_SAFETY_ON_NULL_RETURN(obj);
   sd = evas_object_smart_data_get(obj);
   EINA_INLIST_FOREACH(sd->mirrors, m)
     {
        if (m->cw->bd)
          fprintf(stderr, "MIRROR BD:  %p - %u '%s:%s'\n", m->cw, m->cw->win, m->cw->bd->client.icccm.name, m->cw->bd->client.icccm.class);
        else if (m->cw->pop)
          fprintf(stderr, "MIRROR POP: %p - %s\n", m->cw, m->cw->pop->name);
        else if (m->cw->menu)
          fprintf(stderr, "MIRROR MENU: %p - %s\n", m->cw, m->cw->menu->header.title);
        else if (m->cw->real_obj)
          fprintf(stderr, "MIRROR OBJ: %p - %s\n", m->cw, evas_object_name_get(m->cw->obj));
        else
          fprintf(stderr, "MIRROR WIN: %p - %u%s\n", m->cw, m->cw->win, m->cw->input_only ? " INPUT" : "");
     }
}
