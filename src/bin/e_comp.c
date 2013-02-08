#include "e.h"
#ifdef HAVE_WAYLAND_CLIENTS
#include "e_comp_wl.h"
#endif

#define ACTION_TIMEOUT 30.0
#define OVER_FLOW 1
//#define SHAPE_DEBUG

//////////////////////////////////////////////////////////////////////////
//
// TODO (no specific order):
//   1. abstract evas object and compwin so we can duplicate the object N times
//      in N canvases - for winlist, everything, pager etc. too
//   2. implement "unmapped composite cache" -> N pixels worth of unmapped
//      windows to be fully composited. only the most active/recent.
//   3. for unmapped windows - when window goes out of unmapped comp cache
//      make a miniature copy (1/4 width+height?) and set property on window
//      with pixmap id
//   8. obey transparent property
//   9. shortcut lots of stuff to draw inside the compositor - shelf,
//      wallpaper, efm - hell even menus and anything else in e (this is what
//      e18 was mostly about)
//  10. fullscreen windows need to be able to bypass compositing *seems buggy*
//
//////////////////////////////////////////////////////////////////////////

static Eina_List *handlers = NULL;
static Eina_List *compositors = NULL;
static Eina_Hash *windows = NULL;
static Eina_Hash *borders = NULL;
static Eina_Hash *damages = NULL;

static E_Comp_Config *conf = NULL;
static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_match_edd = NULL;

static Ecore_Timer *action_timeout = NULL;
static Eina_Bool gl_avail = EINA_FALSE;

EAPI int E_EVENT_COMP_SOURCE_VISIBILITY = -1;
EAPI int E_EVENT_COMP_SOURCE_ADD = -1;
EAPI int E_EVENT_COMP_SOURCE_DEL = -1;
EAPI int E_EVENT_COMP_SOURCE_CONFIGURE = -1;

static int _e_comp_log_dom = -1;

//////////////////////////////////////////////////////////////////////////
#undef DBG
#undef INF
#undef WRN
#undef ERR
#undef CRI

#if 1
# ifdef SHAPE_DEBUG
#  define SHAPE_DBG(...)            EINA_LOG_DOM_DBG(_e_comp_log_dom, __VA_ARGS__)
#  define SHAPE_INF(...)            EINA_LOG_DOM_INFO(_e_comp_log_dom, __VA_ARGS__)
#  define SHAPE_WRN(...)            EINA_LOG_DOM_WARN(_e_comp_log_dom, __VA_ARGS__)
#  define SHAPE_ERR(...)            EINA_LOG_DOM_ERR(_e_comp_log_dom, __VA_ARGS__)
#  define SHAPE_CRI(...)            EINA_LOG_DOM_CRIT(_e_comp_log_dom, __VA_ARGS__)
# else
#  define SHAPE_DBG(f, x ...)
#  define SHAPE_INF(f, x ...)
#  define SHAPE_WRN(f, x ...)
#  define SHAPE_ERR(f, x ...)
#  define SHAPE_CRI(f, x ...)
# endif

#define DBG(...)            EINA_LOG_DOM_DBG(_e_comp_log_dom, __VA_ARGS__)
#define INF(...)            EINA_LOG_DOM_INFO(_e_comp_log_dom, __VA_ARGS__)
#define WRN(...)            EINA_LOG_DOM_WARN(_e_comp_log_dom, __VA_ARGS__)
#define ERR(...)            EINA_LOG_DOM_ERR(_e_comp_log_dom, __VA_ARGS__)
#define CRI(...)            EINA_LOG_DOM_CRIT(_e_comp_log_dom, __VA_ARGS__)
#else
#define DBG(f, x ...)
#define INF(f, x ...)
#define WRN(f, x ...)
#define ERR(f, x ...)
#define CRI(f, x ...)
#endif

static void _e_comp_win_ready_timeout_setup(E_Comp_Win *cw);
static void _e_comp_render_queue(E_Comp *c);
static void _e_comp_win_damage(E_Comp_Win *cw, int x, int y, int w, int h, Eina_Bool dmg);
static void _e_comp_win_render_queue(E_Comp_Win *cw);
static void _e_comp_win_release(E_Comp_Win *cw);
static void _e_comp_win_adopt(E_Comp_Win *cw);
static void _e_comp_win_del(E_Comp_Win *cw);
static void _e_comp_win_show(E_Comp_Win *cw);
static void _e_comp_win_real_hide(E_Comp_Win *cw);
static void _e_comp_win_hide(E_Comp_Win *cw);
static void _e_comp_win_configure(E_Comp_Win *cw, int x, int y, int w, int h, int border);
static void _e_comp_shapes_update(void *data, E_Container_Shape *es, E_Container_Shape_Change ch);

static void
_e_comp_event_end(void *d EINA_UNUSED, E_Event_Comp *ev)
{
   ev->cw->pending_count--;
   if (ev->cw->delete_pending && (!ev->cw->pending_count))
     free(ev->cw);
   free(ev);
}

/* FIXME: external ? */
static void
_e_comp_event_source_visibility(E_Comp_Win *cw)
{
   E_Event_Comp *ev;

   ev = E_NEW(E_Event_Comp, 1);
   ev->cw = cw;
   ecore_event_add(E_EVENT_COMP_SOURCE_VISIBILITY, ev, (Ecore_End_Cb)_e_comp_event_end, NULL);
}

static void
_e_comp_event_source_add(E_Comp_Win *cw)
{
   E_Event_Comp *ev;

   ev = E_NEW(E_Event_Comp, 1);
   ev->cw = cw;
   ecore_event_add(E_EVENT_COMP_SOURCE_VISIBILITY, ev, (Ecore_End_Cb)_e_comp_event_end, NULL);
}

static void
_e_comp_event_source_del(E_Comp_Win *cw)
{
   E_Event_Comp *ev;

   ev = E_NEW(E_Event_Comp, 1);
   ev->cw = cw;
   ecore_event_add(E_EVENT_COMP_SOURCE_VISIBILITY, ev, (Ecore_End_Cb)_e_comp_event_end, NULL);
}

static void
_e_comp_event_source_configure(E_Comp_Win *cw)
{
   E_Event_Comp *ev;

   ev = E_NEW(E_Event_Comp, 1);
   ev->cw = cw;
   ecore_event_add(E_EVENT_COMP_SOURCE_VISIBILITY, ev, (Ecore_End_Cb)_e_comp_event_end, NULL);
}

static void
_e_comp_child_show(E_Comp_Win *cw)
{
   evas_object_show(cw->shobj);
   if (cw->bd)
     {
        Eina_List *l;
        E_Border *tmp;

        EINA_LIST_FOREACH(cw->bd->client.e.state.video_child, l, tmp)
          {
             E_Comp_Win *tcw;

             tcw = eina_hash_find(borders, e_util_winid_str_get(tmp->client.win));
             if (!tcw) continue;

             evas_object_show(tcw->shobj);
          }
     }
}

static void
_e_comp_child_hide(E_Comp_Win *cw)
{
   evas_object_hide(cw->shobj);
   if (cw->bd)
     {
        Eina_List *l;
        E_Border *tmp;

        EINA_LIST_FOREACH(cw->bd->client.e.state.video_child, l, tmp)
          {
             E_Comp_Win *tcw;

             tcw = eina_hash_find(borders, e_util_winid_str_get(tmp->client.win));
             if (!tcw) continue;

             evas_object_hide(tcw->shobj);
          }
     }
}

static E_Comp_Win *
_e_comp_fullscreen_check(E_Comp *c)
{
   E_Comp_Win *cw;

   if (!c->wins) return NULL;
   EINA_INLIST_REVERSE_FOREACH(c->wins, cw)
     {
        if ((!cw->visible) || (cw->input_only) || (cw->invalid))
          continue;
        if ((cw->x == 0) && (cw->y == 0) &&
            ((cw->x + cw->w) >= c->man->w) &&
            ((cw->y + cw->h) >= c->man->h) &&
            (!cw->argb) && (!cw->shaped) && (!cw->bg_win)
            )
          {
             return cw;
          }
        return NULL;
     }
   return NULL;
}

static inline Eina_Bool
_e_comp_shaped_check(int w, int h, const Ecore_X_Rectangle *rects, int num)
{
   if ((!rects) || (num < 1)) return EINA_FALSE;
   if (num > 1) return EINA_TRUE;
   if ((rects[0].x == 0) && (rects[0].y == 0) &&
       ((int)rects[0].width == w) && ((int)rects[0].height == h))
     return EINA_FALSE;
   return EINA_TRUE;
}

static inline Eina_Bool
_e_comp_win_shaped_check(const E_Comp_Win *cw, const Ecore_X_Rectangle *rects, int num)
{
   return _e_comp_shaped_check(cw->w, cw->h, rects, num);
}

static void
_e_comp_win_shape_rectangles_apply(E_Comp_Win *cw, const Ecore_X_Rectangle *rects, int num)
{
   Eina_List *l;
   Evas_Object *o;
   int i;

   DBG("SHAPE [0x%x] change, rects=%p (%d)", cw->win, rects, num);
   if (!_e_comp_win_shaped_check(cw, rects, num))
     {
        rects = NULL;
     }
   if (rects)
     {
        unsigned int *pix, *p;
        unsigned char *spix, *sp;
        int w, h, px, py;

        evas_object_image_size_get(cw->obj, &w, &h);
        if ((w > 0) && (h > 0))
          {
             if (cw->native)
               {
                  ERR("BUGGER: shape with native surface? cw=%p", cw);
                  return;
               }

             evas_object_image_native_surface_set(cw->obj, NULL);
             evas_object_image_alpha_set(cw->obj, 1);
             EINA_LIST_FOREACH(cw->obj_mirror, l, o)
               {
                  evas_object_image_native_surface_set(o, NULL);
                  evas_object_image_alpha_set(o, 1);
               }
             pix = evas_object_image_data_get(cw->obj, 1);
             if (pix)
               {
                  spix = calloc(w * h, sizeof(unsigned char));
                  if (spix)
                    {
                       DBG("SHAPE [0x%x] rects %i", cw->win, num);
                       for (i = 0; i < num; i++)
                         {
                            int rx, ry, rw, rh;

                            rx = rects[i].x; ry = rects[i].y;
                            rw = rects[i].width; rh = rects[i].height;
                            E_RECTS_CLIP_TO_RECT(rx, ry, rw, rh, 0, 0, w, h);
                            sp = spix + (w * ry) + rx;
                            for (py = 0; py < rh; py++)
                              {
                                 for (px = 0; px < rw; px++)
                                   {
                                      *sp = 0xff; sp++;
                                   }
                                 sp += w - rw;
                              }
                         }
                       sp = spix;
                       p = pix;
                       for (py = 0; py < h; py++)
                         {
                            for (px = 0; px < w; px++)
                              {
                                 unsigned int mask, imask;

                                 mask = ((unsigned int)(*sp)) << 24;
                                 imask = mask >> 8;
                                 imask |= imask >> 8;
                                 imask |= imask >> 8;
                                 *p = mask | (*p & imask);
                                 //if (*sp) *p = 0xff000000 | *p;
                                 //else *p = 0x00000000;
                                 sp++;
                                 p++;
                              }
                         }
                       free(spix);
                    }
                  evas_object_image_data_set(cw->obj, pix);
                  evas_object_image_data_update_add(cw->obj, 0, 0, w, h);
                  EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                    {
                       evas_object_image_data_set(o, pix);
                       evas_object_image_data_update_add(o, 0, 0, w, h);
                    }
               }
          }
     }
   else
     {
        if (cw->shaped)
          {
             unsigned int *pix, *p;
             int w, h, px, py;

             evas_object_image_size_get(cw->obj, &w, &h);
             if ((w > 0) && (h > 0))
               {
                  if (cw->native)
                    {
                       ERR("BUGGER: shape with native surface? cw=%p", cw);
                       return;
                    }

                  evas_object_image_alpha_set(cw->obj, 0);
                  EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                    {
                       evas_object_image_alpha_set(o, 1);
                    }
                  pix = evas_object_image_data_get(cw->obj, 1);
                  if (pix)
                    {
                       p = pix;
                       for (py = 0; py < h; py++)
                         {
                            for (px = 0; px < w; px++)
                              *p |= 0xff000000;
                         }
                    }
                  evas_object_image_data_set(cw->obj, pix);
                  evas_object_image_data_update_add(cw->obj, 0, 0, w, h);
                  EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                    {
                       evas_object_image_data_set(o, pix);
                       evas_object_image_data_update_add(o, 0, 0, w, h);
                    }
               }
          }
        // dont need to fix alpha chanel as blending
        // should be totally off here regardless of
        // alpha channel content
     }
}

static Eina_Bool
_e_comp_cb_win_show_ready_timeout(void *data)
{
   E_Comp_Win *cw = data;
   cw->show_ready = 1;
   if (cw->visible)
     {
        if (!cw->update)
          {
             if (cw->update_timeout)
               {
                  ecore_timer_del(cw->update_timeout);
                  cw->update_timeout = NULL;
               }
             cw->update = 1;
             cw->c->updates = eina_list_append(cw->c->updates, cw);
          }
        _e_comp_win_render_queue(cw);
     }
   cw->ready_timeout = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_comp_win_ready_timeout_setup(E_Comp_Win *cw)
{
   if (cw->ready_timeout)
     {
        ecore_timer_del(cw->ready_timeout);
        cw->ready_timeout = NULL;
     }
   if (cw->show_ready) return;
   if (cw->counter) return;
   // FIXME: make show_ready option
   if (0)
     {
        cw->show_ready = 1;
     }
   else
     cw->ready_timeout = ecore_timer_add(conf->first_draw_delay, _e_comp_cb_win_show_ready_timeout, cw);
}

static void
_e_comp_win_layout_populate(E_Comp_Win *cw)
{
   e_layout_pack(cw->c->layout, cw->shobj);
}

static void
_e_comp_win_restack(E_Comp_Win *cw)
{
   Eina_Inlist *prev, *next;
   Eina_List *l;
   E_Comp_Win *cwp = NULL, *cwn = NULL;

   next = EINA_INLIST_GET(cw)->next;
   if (next) cwn = EINA_INLIST_CONTAINER_GET(next, E_Comp_Win);
   prev = EINA_INLIST_GET(cw)->prev;
   if (prev) cwp = EINA_INLIST_CONTAINER_GET(prev, E_Comp_Win);

   if (cwp)
     {
        e_layout_child_raise_above(cw->shobj, cwp->shobj);
        cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(cw));
        cw->c->wins = eina_inlist_append_relative(cw->c->wins, EINA_INLIST_GET(cw), EINA_INLIST_GET(cwp));
     }
   else if (cwn)
     {
        e_layout_child_raise_above(cw->shobj, cwn->shobj);
        cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(cw));
        cw->c->wins = eina_inlist_append_relative(cw->c->wins, EINA_INLIST_GET(cw), EINA_INLIST_GET(cwn));
     }
   if (cw->bd)
     {
        E_Border *tmp;

        EINA_LIST_FOREACH(cw->bd->client.e.state.video_child, l, tmp)
          {
             E_Comp_Win *tcw;

             tcw = eina_hash_find(borders, e_util_winid_str_get(tmp->client.win));
             if (!tcw) continue;
             e_layout_child_lower_below(tcw->shobj, cw->shobj);
             cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(tcw));
             cw->c->wins = eina_inlist_prepend_relative(cw->c->wins, EINA_INLIST_GET(tcw), EINA_INLIST_GET(cw));
          }
     }
}

static void
_e_comp_win_geometry_update(E_Comp_Win *cw)
{
   e_layout_child_move(cw->shobj, cw->x, cw->y);
   e_layout_child_resize(cw->shobj, cw->pw, cw->ph);
}

static void
_e_comp_win_update(E_Comp_Win *cw)
{
   Eina_List *l;
   Evas_Object *o;
   E_Comp_Render_Update_Rect *r;
   int i;
   int pshaped = cw->shaped;

   DBG("UPDATE [0x%x] pm = %x", cw->win, cw->pixmap);
   if (conf->grab) ecore_x_grab();
   cw->update = 0;

   if (cw->argb)
     {
        if (cw->rects)
          {
             free(cw->rects);
             cw->rects = NULL;
             cw->rects_num = 0;
          }
     }
   else
     {
        if (cw->shape_changed)
          {
             if (cw->rects)
               {
                  free(cw->rects);
                  cw->rects = NULL;
                  cw->rects_num = 0;
               }
             ecore_x_pixmap_geometry_get(cw->win, NULL, NULL, &(cw->w), &(cw->h));
             cw->rects = ecore_x_window_shape_rectangles_get(cw->win, &(cw->rects_num));
             if (cw->rects)
               {
                  for (i = 0; i < cw->rects_num; i++)
                    {
                       E_RECTS_CLIP_TO_RECT(cw->rects[i].x, cw->rects[i].y,
                         cw->rects[i].width, cw->rects[i].height, 0, 0, (int)cw->w, (int)cw->h);
                    }
               }
             if (!_e_comp_win_shaped_check(cw, cw->rects, cw->rects_num))
               {
                  E_FREE(cw->rects);
                  cw->rects_num = 0;
               }
             if ((cw->rects) && (!cw->shaped))
               {
                  cw->shaped = 1;
               }
             else if ((!cw->rects) && (cw->shaped))
               {
                  cw->shaped = 0;
               }
          }
     }

   if (((!cw->pixmap) || (cw->needpix)) &&
       (!cw->real_hid) && (!cw->nocomp) && (!cw->c->nocomp))
     {
        Ecore_X_Pixmap pm = 0;

        /* #ifdef HAVE_WAYLAND_CLIENTS */
        /*         if ((cw->bd) && (cw->bd->borderless)) */
        /*           pm = e_comp_wl_pixmap_get(cw->win); */
        /* #endif */
        if (!pm) pm = ecore_x_composite_name_window_pixmap_get(cw->win);
        if (pm)
          {
             Ecore_X_Pixmap oldpm;

             cw->needpix = 0;
             if (cw->xim) cw->needxim = 1;
             oldpm = cw->pixmap;
             cw->pixmap = pm;
             if (cw->pixmap)
               {
                  ecore_x_pixmap_geometry_get(cw->pixmap, NULL, NULL, &(cw->pw), &(cw->ph));
                  _e_comp_win_ready_timeout_setup(cw);
                  if ((cw->pw > 0) && (cw->ph > 0))
                    _e_comp_win_geometry_update(cw);
               }
             else
               {
                  cw->pw = 0;
                  cw->ph = 0;
               }
             DBG("REND [0x%x] pixmap = [0x%x], %ix%i", cw->win, cw->pixmap, cw->pw, cw->ph);
             if ((cw->pw <= 0) || (cw->ph <= 0))
               {
                  if (cw->native)
                    {
                       DBG("  [0x%x] free native", cw->win);
                       evas_object_image_native_surface_set(cw->obj, NULL);
                       cw->native = 0;
                       EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                         {
                            evas_object_image_native_surface_set(o, NULL);
                         }
                    }
                  if (cw->pixmap)
                    {
                       DBG("  [0x%x] free pixmap", cw->win);
                       ecore_x_pixmap_free(cw->pixmap);
                       cw->pixmap = 0;
                       //cw->show_ready = 0; // hmm maybe not needed?
                    }
                  cw->pw = 0;
                  cw->ph = 0;
               }
             ecore_x_e_comp_pixmap_set(cw->win, cw->pixmap);
             cw->native = 0;
             DBG("  [0x%x] up resize %ix%i", cw->win, cw->pw, cw->ph);
             e_comp_render_update_resize(cw->up, cw->pw, cw->ph);
             e_comp_render_update_add(cw->up, 0, 0, cw->pw, cw->ph);
             if (oldpm)
               {
                  DBG("  [0x%x] free pm %x", cw->win, oldpm);
                  // XXX the below is unreachable code! :)
                  /*
                     if (cw->native)
                     {
                     cw->native = 0;
                     if (!((cw->pw > 0) && (cw->ph > 0)))
                     {
                     evas_object_image_native_surface_set(cw->obj, NULL);
                     EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                     {
                     evas_object_image_native_surface_set(o, NULL);
                     }
                     }
                     }
                   */
                  ecore_x_pixmap_free(oldpm);
               }
          }
     }
   if (!((cw->pw > 0) && (cw->ph > 0)))
     {
        if (conf->grab) ecore_x_ungrab();
        return;
     }

   //   evas_object_move(cw->shobj, cw->x, cw->y);
   // was cw->w / cw->h
   //   evas_object_resize(cw->shobj, cw->pw, cw->ph);
   _e_comp_win_geometry_update(cw);

   if ((cw->c->gl) && (conf->texture_from_pixmap) &&
       (!cw->shaped) && (!cw->rects) && (cw->pixmap))
     {
        /* #ifdef HAVE_WAYLAND_CLIENTS */
        /*         DBG("DEBUG - pm now %x", e_comp_wl_pixmap_get(cw->win)); */
        /* #endif */
        /* DBG("DEBUG - pm now %x", ecore_x_composite_name_window_pixmap_get(cw->win)); */
        evas_object_image_size_set(cw->obj, cw->pw, cw->ph);
        EINA_LIST_FOREACH(cw->obj_mirror, l, o)
          {
             evas_object_image_size_set(o, cw->pw, cw->ph);
          }
        if (!cw->native)
          {
             Evas_Native_Surface ns;

             ns.version = EVAS_NATIVE_SURFACE_VERSION;
             ns.type = EVAS_NATIVE_SURFACE_X11;
             ns.data.x11.visual = cw->vis;
             ns.data.x11.pixmap = cw->pixmap;
             evas_object_image_native_surface_set(cw->obj, &ns);
             DBG("NATIVE [0x%x] %x %ix%i", cw->win, cw->pixmap, cw->pw, cw->ph);
             cw->native = 1;
             EINA_LIST_FOREACH(cw->obj_mirror, l, o)
               {
                  evas_object_image_native_surface_set(o, &ns);
               }
          }
        r = e_comp_render_update_rects_get(cw->up);
        if (r)
          {
             e_comp_render_update_clear(cw->up);
             for (i = 0; r[i].w > 0; i++)
               {
                  int x, y, w, h;

                  x = r[i].x; y = r[i].y;
                  w = r[i].w; h = r[i].h;
                  DBG("UPDATE [0x%x] pm [0x%x] %i %i %ix%i", cw->win, cw->pixmap, x, y, w, h);
                  evas_object_image_data_update_add(cw->obj, x, y, w, h);
                  EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                    {
                       evas_object_image_data_update_add(o, x, y, w, h);
                    }
               }
             free(r);
          }
        else
          {
             DBG("UPDATE [0x%x] NO RECTS!!! %i %i - %i %i",
                 cw->win, cw->up->w, cw->up->h, cw->up->tw, cw->up->th);
             //             cw->update = 1;
          }
     }
   else if (cw->pixmap)
     {
        if (cw->native)
          {
             evas_object_image_native_surface_set(cw->obj, NULL);
             EINA_LIST_FOREACH(cw->obj_mirror, l, o)
               {
                  evas_object_image_native_surface_set(o, NULL);
               }
             cw->native = 0;
          }
        if (cw->needxim)
          {
             cw->needxim = 0;
             if (cw->xim)
               {
                  evas_object_image_size_set(cw->obj, 1, 1);
                  evas_object_image_data_set(cw->obj, NULL);
                  EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                    {
                       evas_object_image_size_set(o, 1, 1);
                       evas_object_image_data_set(o, NULL);
                    }
                  ecore_x_image_free(cw->xim);
                  cw->xim = NULL;
               }
          }
        if (!cw->xim)
          {
             if ((cw->xim = ecore_x_image_new(cw->pw, cw->ph, cw->vis, cw->depth)))
               e_comp_render_update_add(cw->up, 0, 0, cw->pw, cw->ph);
          }
        r = e_comp_render_update_rects_get(cw->up);
        if (r)
          {
             if (cw->xim)
               {
                  if (ecore_x_image_is_argb32_get(cw->xim))
                    {
                       unsigned int *pix;

                       pix = ecore_x_image_data_get(cw->xim, NULL, NULL, NULL);
                       evas_object_image_data_set(cw->obj, pix);
                       evas_object_image_size_set(cw->obj, cw->pw, cw->ph);
                       EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                         {
                            evas_object_image_data_set(o, pix);
                            evas_object_image_size_set(o, cw->pw, cw->ph);
                         }

                       e_comp_render_update_clear(cw->up);
                       for (i = 0; r[i].w > 0; i++)
                         {
                            int x, y, w, h;

                            x = r[i].x; y = r[i].y;
                            w = r[i].w; h = r[i].h;
                            if (!ecore_x_image_get(cw->xim, cw->pixmap, x, y, x, y, w, h))
                              {
                                 WRN("UPDATE [0x%x] %i %i %ix%i FAIL!!!!!!!!!!!!!!!!!", cw->win, x, y, w, h);
                                 e_comp_render_update_add(cw->up, x, y, w, h);
                                 cw->update = 1;
                              }
                            else
                              {
                                 // why do we neeed these 2? this smells wrong
                                 pix = ecore_x_image_data_get(cw->xim, NULL, NULL, NULL);
                                 DBG("UPDATE [0x%x] %i %i %ix%i -- pix = %p", cw->win, x, y, w, h, pix);
                                 evas_object_image_data_set(cw->obj, pix);
                                 evas_object_image_data_update_add(cw->obj, x, y, w, h);
                                 EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                                   {
                                      evas_object_image_data_set(o, pix);
                                      evas_object_image_data_update_add(o, x, y, w, h);
                                   }
                              }
                         }
                    }
                  else
                    {
                       unsigned int *pix;
                       int stride;

                       evas_object_image_size_set(cw->obj, cw->pw, cw->ph);
                       pix = evas_object_image_data_get(cw->obj, EINA_TRUE);
                       stride = evas_object_image_stride_get(cw->obj);
                       EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                         {
                            evas_object_image_data_set(o, pix);
                            evas_object_image_size_set(o, cw->pw, cw->ph);
                         }

                       e_comp_render_update_clear(cw->up);
                       for (i = 0; r[i].w > 0; i++)
                         {
                            int x, y, w, h;

                            x = r[i].x; y = r[i].y;
                            w = r[i].w; h = r[i].h;
                            if (!ecore_x_image_get(cw->xim, cw->pixmap, x, y, x, y, w, h))
                              {
                                 WRN("UPDATE [0x%x] %i %i %ix%i FAIL!!!!!!!!!!!!!!!!!", cw->win, x, y, w, h);
                                 e_comp_render_update_add(cw->up, x, y, w, h);
                                 cw->update = 1;
                              }
                            else
                              {
                                 unsigned int *srcpix;
                                 int srcbpp = 0, srcbpl = 0;
                                 // why do we neeed these 2? this smells wrong
                                 srcpix = ecore_x_image_data_get(cw->xim, &srcbpl, NULL, &srcbpp);
                                 ecore_x_image_to_argb_convert(srcpix, srcbpp, srcbpl,
                                                               cw->cmap, cw->vis, x, y, w, h, pix, stride, x, y);
                                 DBG("UPDATE [0x%x] %i %i %ix%i -- pix = %p", cw->win, x, y, w, h, pix);
                                 evas_object_image_data_update_add(cw->obj, x, y, w, h);
                                 EINA_LIST_FOREACH(cw->obj_mirror, l, o)
                                   {
                                      evas_object_image_data_update_add(o, x, y, w, h);
                                   }
                              }
                         }
                       evas_object_image_data_set(cw->obj, pix);
                    }
               }
             free(r);
             if (cw->shaped)
               {
                  _e_comp_win_shape_rectangles_apply(cw, cw->rects, cw->rects_num);
               }
             else
               {
                  if (cw->shape_changed)
                    _e_comp_win_shape_rectangles_apply(cw, cw->rects, cw->rects_num);
               }
             cw->shape_changed = 0;
          }
        else
          {
             DBG("UPDATE [0x%x] NO RECTS!!! %i %i - %i %i",
                 cw->win, cw->up->w, cw->up->h, cw->up->tw, cw->up->th);
             // causes updates to be flagged when not needed - disabled
             //             cw->update = 1;
          }
     }
   // FIXME: below cw update check screws with show
   if (/*(!cw->update) &&*/ (cw->visible) && (cw->dmg_updates >= 1) &&
                            (cw->show_ready) && (!cw->bd || cw->bd->visible))
     {
        if (!evas_object_visible_get(cw->shobj))
          {
             if (!cw->hidden_override)
               {
                  _e_comp_child_show(cw);
                  _e_comp_win_render_queue(cw);
               }

             if (!cw->show_anim)
               {
                  edje_object_signal_emit(cw->shobj, "e,state,visible,on", "e");
                  if (!cw->animating)
                    {
                       cw->c->animating++;
                    }

                  cw->animating = 1;

                  cw->pending_count++;
                  _e_comp_event_source_visibility(cw);
                  cw->show_anim = EINA_TRUE;
               }
          }
     }
   if ((cw->shobj) && (cw->obj))
     {
        if (pshaped != cw->shaped)
          {
             if (cw->shaped)
               edje_object_signal_emit(cw->shobj, "e,state,shadow,off", "e");
             else
               edje_object_signal_emit(cw->shobj, "e,state,shadow,on", "e");
          }
     }

   if (conf->grab) ecore_x_ungrab();
}

static void
_e_comp_pre_swap(void *data, Evas *e EINA_UNUSED)
{
   E_Comp *c = data;

   if (conf->grab)
     {
        if (c->grabbed)
          {
             ecore_x_ungrab();
             c->grabbed = 0;
          }
     }
}

static Eina_Bool
_e_comp_cb_delayed_update_timer(void *data)
{
   E_Comp *c = data;
   _e_comp_render_queue(c);
   c->new_up_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_comp_fps_update(E_Comp *c)
{
   if (conf->fps_show)
     {
        if (!c->fps_bg)
          {
             c->fps_bg = evas_object_rectangle_add(c->evas);
             evas_object_color_set(c->fps_bg, 0, 0, 0, 128);
             E_LAYER_SET_ABOVE(c->fps_bg, E_COMP_CANVAS_LAYER_MAX);
             evas_object_show(c->fps_bg);

             c->fps_fg = evas_object_text_add(c->evas);
             evas_object_text_font_set(c->fps_fg, "Sans", 10);
             evas_object_text_text_set(c->fps_fg, "???");
             evas_object_color_set(c->fps_fg, 255, 255, 255, 255);
             E_LAYER_SET_ABOVE(c->fps_fg, E_COMP_CANVAS_LAYER_MAX);
             evas_object_show(c->fps_fg);
          }
     }
   else
     {
        if (c->fps_fg)
          {
             evas_object_del(c->fps_fg);
             c->fps_fg = NULL;
          }
        if (c->fps_bg)
          {
             evas_object_del(c->fps_bg);
             c->fps_bg = NULL;
          }
     }
}

static void
_e_comp_win_release(E_Comp_Win *cw)
{
   Eina_List *l;
   Evas_Object *o;

   if (cw->xim)
     {
        evas_object_image_size_set(cw->obj, 1, 1);
        evas_object_image_data_set(cw->obj, NULL);
        ecore_x_image_free(cw->xim);
        cw->xim = NULL;
     }
   evas_object_image_native_surface_set(cw->obj, NULL);
   cw->native = 0;
   EINA_LIST_FOREACH(cw->obj_mirror, l, o)
     {
        if (cw->xim)
          {
             evas_object_image_size_set(o, 1, 1);
             evas_object_image_data_set(o, NULL);
          }
        evas_object_image_native_surface_set(o, NULL);
     }
   if (cw->pixmap)
     {
        ecore_x_pixmap_free(cw->pixmap);
        cw->pixmap = 0;
        cw->pw = 0;
        cw->ph = 0;
        ecore_x_e_comp_pixmap_set(cw->win, cw->pixmap);
        cw->show_ready = 0; // hmm maybe not needed?
     }
   if (cw->redirected)
     {
        // we redirect all subwindows anyway
        //        ecore_x_composite_unredirect_window(cw->win, ECORE_X_COMPOSITE_UPDATE_MANUAL);
        cw->redirected = 0;
     }
   if (cw->damage)
     {
        Ecore_X_Region parts;

        eina_hash_del(damages, e_util_winid_str_get(cw->damage), cw);
        parts = ecore_x_region_new(NULL, 0);
        ecore_x_damage_subtract(cw->damage, 0, parts);
        ecore_x_region_free(parts);
        ecore_x_damage_free(cw->damage);
        cw->damage = 0;
     }
}

static void
_e_comp_win_adopt(E_Comp_Win *cw)
{
   if (!cw->damage)
     {
        cw->damage = ecore_x_damage_new
            (cw->win, ECORE_X_DAMAGE_REPORT_DELTA_RECTANGLES);
        eina_hash_add(damages, e_util_winid_str_get(cw->damage), cw);
     }
   if (!cw->update)
     {
        cw->update = 1;
        cw->c->updates = eina_list_append(cw->c->updates, cw);
     }
   cw->redirected = 1;
   e_comp_render_update_resize(cw->up, cw->pw, cw->ph);
   e_comp_render_update_add(cw->up, 0, 0, cw->pw, cw->ph);
   _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
   _e_comp_win_render_queue(cw);
}

static void
_e_comp_cb_nocomp_begin(E_Comp *c)
{
   E_Comp_Win *cw, *cwf;

   if (c->nocomp) return;

   if (c->nocomp_delay_timer)
     {
        ecore_timer_del(c->nocomp_delay_timer);
        c->nocomp_delay_timer = NULL;
     }

   cwf = _e_comp_fullscreen_check(c);
   if (!cwf) return;

   EINA_INLIST_FOREACH(c->wins, cw)
     {
        _e_comp_win_release(cw);
     }
   cw = cwf;

   INF("NOCOMP win %x shobj %p", cw->win, cw->shobj);

   _e_comp_win_release(cw);

   ecore_x_composite_unredirect_subwindows
     (c->man->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);
   c->nocomp = 1;
   c->render_overflow = OVER_FLOW;
   ecore_x_window_hide(c->win);
   ecore_evas_manual_render_set(c->ee, EINA_TRUE);
   ecore_evas_resize(c->ee, 1, 1);
   edje_file_cache_flush();
   edje_collection_cache_flush();
   evas_image_cache_flush(c->evas);
   evas_font_cache_flush(c->evas);
   evas_render_dump(c->evas);
   cw->nocomp = 1;
   if (cw->redirected)
     {
        //        ecore_x_composite_unredirect_window(cw->win, ECORE_X_COMPOSITE_UPDATE_MANUAL);
        cw->redirected = 0;
     }
   if (cw->update_timeout)
     {
        ecore_timer_del(cw->update_timeout);
        cw->update_timeout = NULL;
     }
   if (cw->update)
     {
        cw->update = 0;
        cw->c->updates = eina_list_remove(cw->c->updates, cw);
     }
   if (cw->counter)
     {
        if (cw->bd)
          ecore_x_e_comp_sync_cancel_send(cw->bd->client.win);
        else
          ecore_x_e_comp_sync_cancel_send(cw->win);
        ecore_x_sync_counter_inc(cw->counter, 1);
     }
   DBG("JOB2...");
   _e_comp_render_queue(c);
}

static void
_e_comp_cb_nocomp_end(E_Comp *c)
{
   E_Comp_Win *cw;

   if (!c->nocomp) return;

   ecore_x_composite_redirect_subwindows
     (c->man->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);

   INF("COMP RESUME!");
   c->nocomp = 0;
   c->render_overflow = OVER_FLOW;
   //   ecore_evas_manual_render_set(c->ee, conf->lock_fps);
   ecore_evas_manual_render_set(c->ee, EINA_FALSE);
   ecore_evas_resize(c->ee, c->man->w, c->man->h);
   ecore_x_window_show(c->win);
   EINA_INLIST_FOREACH(c->wins, cw)
     {
        if (!cw->nocomp)
          {
             if ((cw->input_only) || (cw->invalid)) continue;

             if (cw->nocomp_need_update)
               {
                  cw->nocomp_need_update = EINA_FALSE;

                  e_comp_render_update_resize(cw->up, cw->pw, cw->ph);
                  e_comp_render_update_add(cw->up, 0, 0, cw->pw, cw->ph);
               }
             _e_comp_win_adopt(cw);
             continue;
          }
        cw->nocomp = 0;

        _e_comp_win_adopt(cw);

        INF("restore comp %x --- %p", cw->win, cw->shobj);

        if (cw->visible)
          {
             if (!cw->hidden_override) _e_comp_child_show(cw);
             cw->pending_count++;
             _e_comp_event_source_visibility(cw);
             // no need for effect
          }
        if (cw->counter)
          {
             if (cw->bd)
               ecore_x_e_comp_sync_begin_send(cw->bd->client.win);
             else
               ecore_x_e_comp_sync_begin_send(cw->win);
          }
     }
}

static Eina_Bool
_e_comp_cb_nocomp_begin_timeout(void *data)
{
   E_Comp *c = data;

   c->nocomp_delay_timer = NULL;
   if (c->nocomp_override == 0)
     {
        if (_e_comp_fullscreen_check(c)) c->nocomp_want = 1;
        _e_comp_cb_nocomp_begin(c);
     }
   return EINA_FALSE;
}

static Eina_Bool
_e_comp_cb_update(E_Comp *c)
{
   E_Comp_Win *cw;
   Eina_List *new_updates = NULL; // for failed pixmap fetches - get them next frame
   Eina_List *update_done = NULL;
   //   static int doframeinfo = -1;

   if (!c) return EINA_FALSE;
   c->update_job = NULL;
   DBG("UPDATE ALL");
   if (c->nocomp) goto nocomp;
   if (conf->grab)
     {
        ecore_x_grab();
        ecore_x_sync();
        c->grabbed = 1;
     }
   EINA_LIST_FREE(c->updates, cw)
     {
        if (conf->efl_sync)
          {
             if (((cw->counter) && (cw->drawme)) || (!cw->counter))
               {
                  _e_comp_win_update(cw);
                  if (cw->drawme)
                    {
                       update_done = eina_list_append(update_done, cw);
                       cw->drawme = 0;
                    }
               }
             else
               cw->update = 0;
          }
        else
          _e_comp_win_update(cw);
        if (cw->update)
          {
             new_updates = eina_list_append(new_updates, cw);
          }
     }
   _e_comp_fps_update(c);
   if (conf->fps_show)
     {
        char buf[128];
        double fps = 0.0, t, dt;
        int i;
        Evas_Coord x = 0, y = 0, w = 0, h = 0;
        E_Zone *z;

        t = ecore_time_get();
        if (conf->fps_average_range < 1)
          conf->fps_average_range = 30;
        else if (conf->fps_average_range > 120)
          conf->fps_average_range = 120;
        dt = t - c->frametimes[conf->fps_average_range - 1];
        if (dt > 0.0) fps = (double)conf->fps_average_range / dt;
        else fps = 0.0;
        if (fps > 0.0) snprintf(buf, sizeof(buf), "FPS: %1.1f", fps);
        else snprintf(buf, sizeof(buf), "N/A");
        for (i = 121; i >= 1; i--)
          c->frametimes[i] = c->frametimes[i - 1];
        c->frametimes[0] = t;
        c->frameskip++;
        if (c->frameskip >= conf->fps_average_range)
          {
             c->frameskip = 0;
             evas_object_text_text_set(c->fps_fg, buf);
          }
        evas_object_geometry_get(c->fps_fg, NULL, NULL, &w, &h);
        w += 8;
        h += 8;
        z = e_util_zone_current_get(c->man);
        if (z)
          {
             switch (conf->fps_corner)
               {
                case 3: // bottom-right
                  x = z->x + z->w - w;
                  y = z->y + z->h - h;
                  break;

                case 2: // bottom-left
                  x = z->x;
                  y = z->y + z->h - h;
                  break;

                case 1: // top-right
                  x = z->x + z->w - w;
                  y = z->y;
                  break;

                default: // 0 // top-left
                  x = z->x;
                  y = z->y;
                  break;
               }
          }
        evas_object_move(c->fps_bg, x, y);
        evas_object_resize(c->fps_bg, w, h);
        evas_object_move(c->fps_fg, x + 4, y + 4);
     }
   if (conf->lock_fps)
     {
        DBG("MANUAL RENDER...");
        //        if (!c->nocomp) ecore_evas_manual_render(c->ee);
     }
   if (conf->efl_sync)
     {
        EINA_LIST_FREE(update_done, cw)
          {
             ecore_x_sync_counter_inc(cw->counter, 1);
          }
     }
   if (conf->grab)
     {
        if (c->grabbed)
          {
             c->grabbed = 0;
             ecore_x_ungrab();
          }
     }
   if (new_updates)
     {
        DBG("JOB1...");
        if (c->new_up_timer) ecore_timer_del(c->new_up_timer);
        c->new_up_timer =
          ecore_timer_add(0.001, _e_comp_cb_delayed_update_timer, c);
        //        _e_comp_render_queue(c);
     }
   c->updates = new_updates;
   if (!c->animating) c->render_overflow--;
   /*
      if (doframeinfo == -1)
      {
      doframeinfo = 0;
      if (getenv("DFI")) doframeinfo = 1;
      }
      if (doframeinfo)
      {
      static double t0 = 0.0;
      double td, t;

      t = ecore_time_get();
      td = t - t0;
      if (td > 0.0)
      {
      int fps, i;

      fps = 1.0 / td;
      for (i = 0; i < fps; i+= 2) putchar('=');
      printf(" : %3.3f", 1.0 / td);
      }
      t0 = t;
      }
    */
nocomp:
   cw = _e_comp_fullscreen_check(c);
   if (cw)
     {
        if (conf->nocomp_fs)
          {
             if ((!c->nocomp) && (!c->nocomp_override > 0))
               {
                  if (!c->nocomp_delay_timer)
                    c->nocomp_delay_timer = ecore_timer_add
                        (1.0, _e_comp_cb_nocomp_begin_timeout, c);
               }
          }
     }
   else
     {
        c->nocomp_want = 0;
        if (c->nocomp_delay_timer)
          {
             ecore_timer_del(c->nocomp_delay_timer);
             c->nocomp_delay_timer = NULL;
          }
        if (c->nocomp)
          _e_comp_cb_nocomp_end(c);
     }

   DBG("UPDATE ALL DONE: overflow = %i", c->render_overflow);
   if (c->render_overflow <= 0)
     {
        c->render_overflow = 0;
        if (c->render_animator) c->render_animator = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_cb_job(void *data)
{
   DBG("UPDATE ALL JOB...");
   _e_comp_cb_update(data);
}

static Eina_Bool
_e_comp_cb_animator(void *data)
{
   return _e_comp_cb_update(data);
}

static void
_e_comp_render_queue(E_Comp *c)
{
   /* FIXME workaround */
   if (!c) return;

   if (conf->lock_fps)
     {
        if (c->render_animator)
          {
             c->render_overflow = OVER_FLOW;
             return;
          }
        c->render_animator = ecore_animator_add(_e_comp_cb_animator, c);
     }
   else
     {
        if (c->update_job)
          {
             DBG("UPDATE JOB DEL...");
             ecore_job_del(c->update_job);
             c->update_job = NULL;
             c->render_overflow = 0;
          }
        DBG("UPDATE JOB ADD...");
        c->update_job = ecore_job_add(_e_comp_cb_job, c);
     }
}

static void
_e_comp_win_render_queue(E_Comp_Win *cw)
{
   DBG("JOB3...");
   _e_comp_render_queue(cw->c);
}

static E_Comp *
_e_comp_find(Ecore_X_Window root)
{
   Eina_List *l;
   E_Comp *c;

   // fixme: use hash if compositors list > 4
   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (c->man->root == root) return c;
     }
   return NULL;
}

static E_Comp_Win *
_e_comp_win_find(Ecore_X_Window win)
{
   return eina_hash_find(windows, e_util_winid_str_get(win));
}

static E_Comp_Win *
_e_comp_border_client_find(Ecore_X_Window win)
{
   return eina_hash_find(borders, e_util_winid_str_get(win));
}

static E_Comp_Win *
_e_comp_win_damage_find(Ecore_X_Damage damage)
{
   return eina_hash_find(damages, e_util_winid_str_get(damage));
}

static Eina_Bool
_e_comp_win_is_borderless(E_Comp_Win *cw)
{
   if (!cw->bd) return 1;
   if ((cw->bd->client.border.name) &&
       (!strcmp(cw->bd->client.border.name, "borderless")))
     return 1;
   return 0;
}

static Eina_Bool
_e_comp_win_do_shadow(E_Comp_Win *cw)
{
   if (cw->shaped) return 0;
   if (cw->argb)
     {
        if (_e_comp_win_is_borderless(cw)) return 0;
     }
   return 1;
}

static Eina_Bool
_e_comp_win_damage_timeout(void *data)
{
   E_Comp_Win *cw = data;

   if (!cw->update)
     {
        if (cw->update_timeout)
          {
             ecore_timer_del(cw->update_timeout);
             cw->update_timeout = NULL;
          }
        cw->update = 1;
        cw->c->updates = eina_list_append(cw->c->updates, cw);
     }
   cw->drawme = 1;
   _e_comp_win_render_queue(cw);
   cw->update_timeout = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_comp_object_del(void *data, void *obj)
{
   E_Comp_Win *cw = data;

   if (!cw) return;

   _e_comp_win_render_queue(cw);
   if (obj == cw->bd)
     {
        if (cw->counter)
          {
             if (cw->bd)
               ecore_x_e_comp_sync_cancel_send(cw->bd->client.win);
             else
               ecore_x_e_comp_sync_cancel_send(cw->win);
             ecore_x_sync_counter_inc(cw->counter, 1);
          }
        if (cw->bd) eina_hash_del(borders, e_util_winid_str_get(cw->bd->client.win), cw);
        cw->bd = NULL;
        evas_object_data_del(cw->shobj, "border");
        // hmm - lockup?
        //        cw->counter = 0;
     }
   else if (obj == cw->pop)
     {
        cw->pop = NULL;
        evas_object_data_del(cw->shobj, "popup");
     }
   else if (obj == cw->menu)
     {
        cw->menu = NULL;
        evas_object_data_del(cw->shobj, "menu");
     }
   if (cw->dfn)
     {
        e_object_delfn_del(obj, cw->dfn);
        cw->dfn = NULL;
     }
}

static void
_e_comp_done_defer(E_Comp_Win *cw)
{
   if (cw->animating)
     {
        cw->c->animating--;
     }
   cw->animating = 0;
   _e_comp_win_render_queue(cw);
   cw->force = 1;
   if (cw->defer_hide) _e_comp_win_hide(cw);
   cw->force = 1;
   if (cw->delete_me) _e_comp_win_del(cw);
   else cw->force = 0;
}

static void
_e_comp_show_done(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   E_Comp_Win *cw = data;
   _e_comp_done_defer(cw);
}

static void
_e_comp_hide_done(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   E_Comp_Win *cw = data;
   _e_comp_done_defer(cw);
}

static void
_e_comp_win_sync_setup(E_Comp_Win *cw, Ecore_X_Window win)
{
   if (!conf->efl_sync) return;

   if (cw->bd)
     {
        if (_e_comp_win_is_borderless(cw) ||
            (conf->loose_sync))
          cw->counter = ecore_x_e_comp_sync_counter_get(win);
        else
          ecore_x_e_comp_sync_cancel_send(win);
     }
   else
     cw->counter = ecore_x_e_comp_sync_counter_get(win);
   if (cw->counter)
     {
        ecore_x_e_comp_sync_begin_send(win);
        ecore_x_sync_counter_inc(cw->counter, 1);
     }
}

static void
_e_comp_win_shadow_setup(E_Comp_Win *cw)
{
   Evas_Object *o;
   int ok = 0;
   char buf[4096];
   Eina_List *list = NULL, *l;
   E_Comp_Match *m;
   Eina_Bool focus = EINA_FALSE, urgent = EINA_FALSE, skip = EINA_FALSE, fast = EINA_FALSE;
   const char *title = NULL, *name = NULL, *clas = NULL, *role = NULL;
   Ecore_X_Window_Type primary_type = ECORE_X_WINDOW_TYPE_UNKNOWN;

   evas_object_image_smooth_scale_set(cw->obj, conf->smooth_windows);
   EINA_LIST_FOREACH(cw->obj_mirror, l, o)
     {
        evas_object_image_smooth_scale_set(o, conf->smooth_windows);
     }
   if (cw->bd)
     {
        list = conf->match.borders;
        title = cw->bd->client.icccm.title;
        if (cw->bd->client.netwm.name) title = cw->bd->client.netwm.name;
        name = cw->bd->client.icccm.name;
        clas = cw->bd->client.icccm.class;
        role = cw->bd->client.icccm.window_role;
        primary_type = cw->bd->client.netwm.type;
        skip = (conf->match.disable_borders);
        fast = conf->fast_borders;
     }
   else if (cw->pop)
     {
        // FIXME: i only added "shelf" as a name for popups that are shelves
        // ... need more nmes like for pager popup, evertything, exebuf
        // etc. etc.
        list = conf->match.popups;
        name = cw->pop->name;
        skip = (conf->match.disable_popups);
        fast = conf->fast_popups;
     }
   else if (cw->menu)
     {
        // FIXME: e has no way to tell e menus apart... need naming
        list = conf->match.menus;
        skip = (conf->match.disable_menus);
        fast = conf->fast_menus;
     }
   else
     {
        list = conf->match.overrides;
        title = cw->title;
        name = cw->name;
        clas = cw->clas;
        role = cw->role;
        primary_type = cw->primary_type;
        skip = (conf->match.disable_overrides);
        fast = conf->fast_overrides;
     }

   if (!skip)
     {
        EINA_LIST_FOREACH(list, l, m)
          {
             if (((m->title) && (!title)) ||
                 ((title) && (m->title) && (!e_util_glob_match(title, m->title))))
               continue;
             if (((m->name) && (!name)) ||
                 ((name) && (m->name) && (!e_util_glob_match(name, m->name))))
               continue;
             if (((m->clas) && (!clas)) ||
                 ((clas) && (m->clas) && (!e_util_glob_match(clas, m->clas))))
               continue;
             if (((m->role) && (!role)) ||
                 ((role) && (m->role) && (!e_util_glob_match(role, m->role))))
               continue;
             if ((primary_type != ECORE_X_WINDOW_TYPE_UNKNOWN) &&
                 (m->primary_type != ECORE_X_WINDOW_TYPE_UNKNOWN) &&
                 ((int)primary_type != m->primary_type))
               continue;
             if (cw->bd)
               {
                  if (m->borderless != 0)
                    {
                       int borderless = 0;

                       if ((cw->bd->client.mwm.borderless) || (cw->bd->borderless))
                         borderless = 1;
                       if (!(((m->borderless == -1) && (!borderless)) ||
                             ((m->borderless == 1) && (borderless))))
                         continue;
                    }
                  if (m->dialog != 0)
                    {
                       int dialog = 0;

                       if (((cw->bd->client.icccm.transient_for != 0) ||
                            (cw->bd->client.netwm.type == ECORE_X_WINDOW_TYPE_DIALOG)))
                         dialog = 1;
                       if (!(((m->dialog == -1) && (!dialog)) ||
                             ((m->dialog == 1) && (dialog))))
                         continue;
                    }
                  if (m->accepts_focus != 0)
                    {
                       int accepts_focus = 0;

                       if (cw->bd->client.icccm.accepts_focus)
                         accepts_focus = 1;
                       if (!(((m->accepts_focus == -1) && (!accepts_focus)) ||
                             ((m->accepts_focus == 1) && (accepts_focus))))
                         continue;
                    }
                  if (m->vkbd != 0)
                    {
                       int vkbd = 0;

                       if (cw->bd->client.vkbd.vkbd)
                         vkbd = 1;
                       if (!(((m->vkbd == -1) && (!vkbd)) ||
                             ((m->vkbd == 1) && (vkbd))))
                         continue;
                    }
                  if (m->quickpanel != 0)
                    {
                       int quickpanel = 0;

                       if (cw->bd->client.illume.quickpanel.quickpanel)
                         quickpanel = 1;
                       if (!(((m->quickpanel == -1) && (!quickpanel)) ||
                             ((m->quickpanel == 1) && (quickpanel))))
                         continue;
                    }
                  if (m->argb != 0)
                    {
                       if (!(((m->argb == -1) && (!cw->argb)) ||
                             ((m->argb == 1) && (cw->argb))))
                         continue;
                    }
                  if (m->fullscreen != 0)
                    {
                       int fullscreen = 0;

                       if (cw->bd->client.netwm.state.fullscreen)
                         fullscreen = 1;
                       if (!(((m->fullscreen == -1) && (!fullscreen)) ||
                             ((m->fullscreen == 1) && (fullscreen))))
                         continue;
                    }
                  if (m->modal != 0)
                    {
                       int modal = 0;

                       if (cw->bd->client.netwm.state.modal)
                         modal = 1;
                       if (!(((m->modal == -1) && (!modal)) ||
                             ((m->modal == 1) && (modal))))
                         continue;
                    }
               }
             focus = m->focus;
             urgent = m->urgent;
             if (m->shadow_style)
               {
                  if (fast)
                    {
                       snprintf(buf, sizeof(buf), "e/comp/%s/fast", m->shadow_style);
                       ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", buf);
                    }
                  if (!ok)
                    {
                       snprintf(buf, sizeof(buf), "e/comp/%s", m->shadow_style);
                       ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", buf);
                    }
                  if (ok) break;
               }
          }
     }
   while (!ok)
     {
        if (skip || (cw->bd && cw->bd->client.e.state.video))
          ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", "e/comp/none");
        if (ok) break;
        if (conf->shadow_style)
          {
             if (fast)
               {
                  snprintf(buf, sizeof(buf), "e/comp/%s/fast", conf->shadow_style);
                  ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", buf);
               }
             if (!ok)
               {
                  snprintf(buf, sizeof(buf), "e/comp/%s", conf->shadow_style);
                  ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", buf);
               }
          }
        if (!ok)
          {
             if (fast)
               ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", "e/comp/default/fast");
             if (!ok)
               ok = e_theme_edje_object_set(cw->shobj, "base/theme/borders", "e/comp/default");
          }
        break;
     }
   edje_object_part_swallow(cw->shobj, "e.swallow.content", cw->obj);
   if (cw->bd && cw->bd->client.e.state.video)
     edje_object_signal_emit(cw->shobj, "e,state,shadow,off", "e");
   else
     {
        if (_e_comp_win_do_shadow(cw))
          edje_object_signal_emit(cw->shobj, "e,state,shadow,on", "e");
        else
          edje_object_signal_emit(cw->shobj, "e,state,shadow,off", "e");
     }

   if (cw->bd)
     {
        if (focus || (cw->bd && cw->bd->focused))
          edje_object_signal_emit(cw->shobj, "e,state,focus,on", "e");
        else
          edje_object_signal_emit(cw->shobj, "e,state,focus,off", "e");
        if (urgent || (cw->bd && cw->bd->client.icccm.urgent))
          edje_object_signal_emit(cw->shobj, "e,state,urgent,on", "e");
        else
          edje_object_signal_emit(cw->shobj, "e,state,urgent,off", "e");
     }
   if (cw->visible)
     edje_object_signal_emit(cw->shobj, "e,state,visible,on", "e");
   else
     edje_object_signal_emit(cw->shobj, "e,state,visible,off", "e");

   if (!cw->animating)
     {
        cw->c->animating++;
     }
   cw->animating = 1;
   _e_comp_win_render_queue(cw);
}

static void
_e_comp_cb_win_mirror_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Comp_Win *cw;

   if (!(cw = data)) return;
   cw->obj_mirror = eina_list_remove(cw->obj_mirror, obj);
}

static Evas_Object *
_e_comp_win_mirror_add(E_Comp_Win *cw)
{
   Evas_Object *o;

   if (!cw->c) return NULL;

   o = evas_object_image_filled_add(cw->c->evas);
   evas_object_image_colorspace_set(o, EVAS_COLORSPACE_ARGB8888);
   cw->obj_mirror = eina_list_append(cw->obj_mirror, o);
   evas_object_image_smooth_scale_set(o, conf->smooth_windows);

   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _e_comp_cb_win_mirror_del, cw);

   if ((cw->pixmap) && (cw->pw > 0) && (cw->ph > 0))
     {
        unsigned int *pix;
        Eina_Bool alpha;
        int w, h;

        alpha = evas_object_image_alpha_get(cw->obj);
        evas_object_image_size_get(cw->obj, &w, &h);

        evas_object_image_alpha_set(o, alpha);

        if (cw->shaped)
          {
             pix = evas_object_image_data_get(cw->obj, 0);
             evas_object_image_data_set(o, pix);
             evas_object_image_size_set(o, w, h);
             evas_object_image_data_set(o, pix);
             evas_object_image_data_update_add(o, 0, 0, w, h);
          }
        else
          {
             if (cw->native)
               {
                  Evas_Native_Surface ns;

                  ns.version = EVAS_NATIVE_SURFACE_VERSION;
                  ns.type = EVAS_NATIVE_SURFACE_X11;
                  ns.data.x11.visual = cw->vis;
                  ns.data.x11.pixmap = cw->pixmap;
                  evas_object_image_size_set(o, w, h);
                  evas_object_image_native_surface_set(o, &ns);
                  evas_object_image_data_update_add(o, 0, 0, w, h);
               }
             else if (cw->xim)
               {
                  if (ecore_x_image_is_argb32_get(cw->xim))
                    {
                       pix = ecore_x_image_data_get(cw->xim, NULL, NULL, NULL);
                       evas_object_image_data_set(o, pix);
                       evas_object_image_size_set(o, w, h);
                    }
                  else
                    {
                       pix = evas_object_image_data_get(cw->obj, EINA_TRUE);
                       evas_object_image_data_set(o, pix);
                       evas_object_image_size_set(o, w, h);
                       evas_object_image_data_set(cw->obj, pix);
                    }
                  evas_object_image_data_update_add(o, 0, 0, w, h);
               }
          }
        evas_object_image_size_set(o, w, h);
        evas_object_image_data_update_add(o, 0, 0, w, h);
     }
   return o;
}

static E_Comp_Win *
_e_comp_win_add(E_Comp *c, Ecore_X_Window win)
{
   Ecore_X_Window_Attributes att;
   E_Comp_Win *cw;

   cw = calloc(1, sizeof(E_Comp_Win));
   if (!cw) return NULL;
   cw->win = win;
   cw->c = c;
   cw->real_hid = 1;
   cw->opacity = 255.0;
   cw->bd = e_border_find_by_window(cw->win);
   if (conf->grab) ecore_x_grab();
   if (cw->bd)
     {
        eina_hash_add(borders, e_util_winid_str_get(cw->bd->client.win), cw);
        cw->dfn = e_object_delfn_add(E_OBJECT(cw->bd), _e_comp_object_del, cw);
        cw->shape = cw->bd->shape;
        // setup on show
        // _e_comp_win_sync_setup(cw, cw->bd->client.win);
     }
   else if ((cw->pop = e_popup_find_by_window(cw->win)))
     {
        cw->dfn = e_object_delfn_add(E_OBJECT(cw->pop), _e_comp_object_del, cw);
        cw->show_ready = 1;
        cw->shape = cw->pop->shape;
     }
   else if ((cw->menu = e_menu_find_by_window(cw->win)))
     {
        cw->dfn = e_object_delfn_add(E_OBJECT(cw->menu), _e_comp_object_del, cw);
        cw->show_ready = 1;
        cw->shape = cw->menu->shape;
     }
   else
     {
        char *netwm_title = NULL;

        cw->title = ecore_x_icccm_title_get(cw->win);
        if (ecore_x_netwm_name_get(cw->win, &netwm_title))
          {
             free(cw->title);
             cw->title = netwm_title;
          }
        ecore_x_icccm_name_class_get(cw->win, &cw->name, &cw->clas);
        cw->role = ecore_x_icccm_window_role_get(cw->win);
        if (!ecore_x_netwm_window_type_get(cw->win, &cw->primary_type))
          cw->primary_type = ECORE_X_WINDOW_TYPE_UNKNOWN;
        {
           E_Container *con;

           con = e_util_container_current_get();
           cw->bg_win = (con && (cw->win == con->bg_win));
        }
        cw->free_shape = 1;
        // setup on show
        // _e_comp_win_sync_setup(cw, cw->win);
     }

   if (!cw->counter)
     {
        // FIXME: config - disable ready timeout for non-counter wins
        //        cw->show_ready = 1;
     }
   // fixme: could use bd/pop/menu for this too
   memset((&att), 0, sizeof(Ecore_X_Window_Attributes));
   if (!ecore_x_window_attributes_get(cw->win, &att))
     {
        free(cw->name);
        free(cw->clas);
        free(cw->role);
        free(cw);
        if (conf->grab) ecore_x_ungrab();
        return NULL;
     }
   if ((!att.input_only) &&
       ((att.depth != 24) && (att.depth != 32)))
     {
        //        printf("WARNING: window 0x%x not 24/32bpp -> %ibpp", cw->win, att.depth);
        //        cw->invalid = 1;
     }
   cw->input_only = att.input_only;
   cw->override = att.override;
   cw->vis = att.visual;
   cw->cmap = att.colormap;
   cw->depth = att.depth;
   cw->argb = ecore_x_window_argb_get(cw->win);
   eina_hash_add(windows, e_util_winid_str_get(cw->win), cw);
   cw->inhash = 1;
   if ((!cw->input_only) && (!cw->invalid))
     {
        Ecore_X_Rectangle *rects;
        int num;

        cw->damage = ecore_x_damage_new
            (cw->win, ECORE_X_DAMAGE_REPORT_DELTA_RECTANGLES);
        eina_hash_add(damages, e_util_winid_str_get(cw->damage), cw);
        cw->shobj = edje_object_add(c->evas);
        cw->obj = evas_object_image_filled_add(c->evas);
        evas_object_image_colorspace_set(cw->obj, EVAS_COLORSPACE_ARGB8888);
        if (cw->argb) evas_object_image_alpha_set(cw->obj, 1);
        else evas_object_image_alpha_set(cw->obj, 0);

        if (cw->override && !(att.event_mask.mine & ECORE_X_EVENT_MASK_WINDOW_PROPERTY))
          ecore_x_event_mask_set(cw->win, ECORE_X_EVENT_MASK_WINDOW_PROPERTY);

        _e_comp_win_shadow_setup(cw);

        edje_object_signal_callback_add(cw->shobj, "e,action,show,done", "e", _e_comp_show_done, cw);
        edje_object_signal_callback_add(cw->shobj, "e,action,hide,done", "e", _e_comp_hide_done, cw);

        _e_comp_win_layout_populate(cw);

        evas_object_show(cw->obj);
        ecore_x_window_shape_events_select(cw->win, 1);
        rects = ecore_x_window_shape_rectangles_get(cw->win, &num);
        if (rects)
          {
             int i;

             for (i = 0; i < num; i++)
               E_RECTS_CLIP_TO_RECT(rects[i].x, rects[i].y,
                                    rects[i].width, rects[i].height, 0, 0, (int)att.w, (int)att.h);

             if (_e_comp_shaped_check(att.w, att.h, rects, num))
               cw->shape_changed = 1;

             free(rects);
          }

        if (cw->bd) evas_object_data_set(cw->shobj, "border", cw->bd);
        else if (cw->pop)
          evas_object_data_set(cw->shobj, "popup", cw->pop);
        else if (cw->menu)
          evas_object_data_set(cw->shobj, "menu", cw->menu);

        evas_object_pass_events_set(cw->obj, 1);

        cw->pending_count++;
        _e_comp_event_source_add(cw);
     }
   else
     {
        cw->shobj = evas_object_rectangle_add(c->evas);

        _e_comp_win_layout_populate(cw);

        evas_object_color_set(cw->shobj, 0, 0, 0, 0);
     }
   evas_object_pass_events_set(cw->shobj, 1);
   evas_object_data_set(cw->shobj, "win", (void *)((unsigned long)cw->win));
   evas_object_data_set(cw->shobj, "comp_win", cw);

   c->wins_invalid = 1;
   c->wins = eina_inlist_append(c->wins, EINA_INLIST_GET(cw));
   cw->up = e_comp_render_update_new();
   e_comp_render_update_tile_size_set(cw->up, 32, 32);
   // for software:
   e_comp_render_update_policy_set
     (cw->up, E_COMP_RENDER_UPDATE_POLICY_HALF_WIDTH_OR_MORE_ROUND_UP_TO_FULL_WIDTH);
   if (((!cw->input_only) && (!cw->invalid)) && (cw->override))
     {
        cw->redirected = 1;
        // we redirect all subwindows anyway
        // ecore_x_composite_redirect_window(cw->win, ECORE_X_COMPOSITE_UPDATE_MANUAL);
        cw->dmg_updates = 0;
     }
   DBG("  [0x%x] add", cw->win);
   if (conf->grab) ecore_x_ungrab();
   return cw;
}

static void
_e_comp_win_del(E_Comp_Win *cw)
{
   Evas_Object *o;
   int pending_count;

   if (cw->animating)
     {
        cw->c->animating--;
     }
   cw->animating = 0;
   if ((!cw->input_only) && (!cw->invalid))
     {
        cw->pending_count++;
        _e_comp_event_source_del(cw);
     }

   e_comp_render_update_free(cw->up);
   DBG("  [0x%x] del", cw->win);
   E_FREE(cw->rects);
   if (cw->update_timeout)
     {
        ecore_timer_del(cw->update_timeout);
        cw->update_timeout = NULL;
     }
   if (cw->ready_timeout)
     {
        ecore_timer_del(cw->ready_timeout);
        cw->ready_timeout = NULL;
     }
   if (cw->dfn)
     {
        if (cw->bd)
          {
             eina_hash_del(borders, e_util_winid_str_get(cw->bd->client.win), cw);
             e_object_delfn_del(E_OBJECT(cw->bd), cw->dfn);
             cw->bd = NULL;
          }
        else if (cw->pop)
          {
             e_object_delfn_del(E_OBJECT(cw->pop), cw->dfn);
             cw->pop = NULL;
          }
        else if (cw->menu)
          {
             e_object_delfn_del(E_OBJECT(cw->menu), cw->dfn);
             cw->menu = NULL;
          }
        cw->dfn = NULL;
     }
   if (cw->update)
     {
        cw->update = 0;
        cw->c->updates = eina_list_remove(cw->c->updates, cw);
     }

   _e_comp_win_release(cw);

   if (cw->obj_mirror)
     {
        EINA_LIST_FREE(cw->obj_mirror, o)
          {
             if (cw->xim) evas_object_image_data_set(o, NULL);
             evas_object_event_callback_del(o, EVAS_CALLBACK_DEL, _e_comp_cb_win_mirror_del);
             evas_object_del(o);
          }
     }
   if (cw->obj)
     {
        evas_object_del(cw->obj);
        cw->obj = NULL;
     }
   if (cw->shobj)
     {
        evas_object_del(cw->shobj);
        cw->shobj = NULL;
     }

   if (cw->free_shape) E_FN_DEL(e_object_del, cw->shape);

   if (cw->inhash)
     eina_hash_del(windows, e_util_winid_str_get(cw->win), cw);

   free(cw->title);
   free(cw->name);
   free(cw->clas);
   free(cw->role);
   cw->c->wins_invalid = 1;
   cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(cw));
   pending_count = cw->pending_count;
   memset(cw, 0, sizeof(E_Comp_Win));
   cw->pending_count = pending_count;
   cw->delete_pending = 1;
   if (cw->pending_count > 0) return;
   free(cw);
}

static void
_e_comp_win_show(E_Comp_Win *cw)
{
   Eina_List *l;
   Evas_Object *o;

   if (cw->visible) return;
   cw->visible = 1;
   DBG("  [0x%x] sho ++ [redir=%i, pm=%x, dmg_up=%i]",
       cw->win, cw->redirected, cw->pixmap, cw->dmg_updates);
   _e_comp_win_configure(cw, cw->hidden.x, cw->hidden.y, cw->w, cw->h, cw->border);
   if ((cw->input_only) || (cw->invalid)) return;

   cw->show_anim = EINA_FALSE;

   // setup on show
   if (cw->bd)
     _e_comp_win_sync_setup(cw, cw->bd->client.win);
   else
     _e_comp_win_sync_setup(cw, cw->win);

   if (cw->real_hid)
     {
        DBG("  [0x%x] real hid - fix", cw->win);
        cw->real_hid = 0;
        if (cw->native)
          {
             evas_object_image_native_surface_set(cw->obj, NULL);
             cw->native = 0;
             EINA_LIST_FOREACH(cw->obj_mirror, l, o)
               {
                  evas_object_image_native_surface_set(o, NULL);
               }
          }
        if (cw->pixmap)
          {
             ecore_x_pixmap_free(cw->pixmap);
             cw->pixmap = 0;
             cw->pw = 0;
             cw->ph = 0;
             ecore_x_e_comp_pixmap_set(cw->win, cw->pixmap);
          }
        if (cw->xim)
          {
             evas_object_image_size_set(cw->obj, 1, 1);
             evas_object_image_data_set(cw->obj, NULL);
             ecore_x_image_free(cw->xim);
             cw->xim = NULL;
             EINA_LIST_FOREACH(cw->obj_mirror, l, o)
               {
                  evas_object_image_size_set(o, 1, 1);
                  evas_object_image_data_set(o, NULL);
               }
          }
        if (cw->redirected)
          {
             cw->redirected = 0;
             cw->pw = 0;
             cw->ph = 0;
          }
        if (cw->pop)
          cw->dmg_updates = 1;
        else
          cw->dmg_updates = 0;
     }
   else
     cw->dmg_updates = 1;

   if ((!cw->redirected) || (!cw->pixmap))
     {
        // we redirect all subwindows anyway
        //        ecore_x_composite_redirect_window(cw->win, ECORE_X_COMPOSITE_UPDATE_MANUAL);
        /* #ifdef HAVE_WAYLAND_CLIENTS */
        /*         if ((cw->bd) && (cw->bd->borderless)) */
        /*           cw->pixmap = e_comp_wl_pixmap_get(cw->win); */
        /* #endif */
        if (!cw->pixmap)
          cw->pixmap = ecore_x_composite_name_window_pixmap_get(cw->win);
        if (cw->pixmap)
          {
             ecore_x_pixmap_geometry_get(cw->pixmap, NULL, NULL, &(cw->pw), &(cw->ph));
             _e_comp_win_ready_timeout_setup(cw);
          }
        else
          {
             cw->pw = 0;
             cw->ph = 0;
          }
        if ((cw->pw <= 0) || (cw->ph <= 0))
          {
             if (cw->pixmap)
               {
                  ecore_x_pixmap_free(cw->pixmap);
                  cw->pixmap = 0;
               }
             //             cw->show_ready = 0; // hmm maybe not needed?
          }
        cw->redirected = 1;
        DBG("  [0x%x] up resize %ix%i", cw->win, cw->pw, cw->ph);
        e_comp_render_update_resize(cw->up, cw->pw, cw->ph);
        e_comp_render_update_add(cw->up, 0, 0, cw->pw, cw->ph);
        evas_object_image_size_set(cw->obj, cw->pw, cw->ph);
        EINA_LIST_FOREACH(cw->obj_mirror, l, o)
          {
             evas_object_image_size_set(o, cw->pw, cw->ph);
          }
        ecore_x_e_comp_pixmap_set(cw->win, cw->pixmap);
     }
   if ((cw->dmg_updates >= 1) && (cw->show_ready))
     {
        cw->defer_hide = 0;
        if (!cw->hidden_override) _e_comp_child_show(cw);
        edje_object_signal_emit(cw->shobj, "e,state,visible,on", "e");
        if (!cw->animating)
          {
             cw->c->animating++;
          }
        cw->animating = 1;
        _e_comp_win_render_queue(cw);

        cw->pending_count++;
        _e_comp_event_source_visibility(cw);
     }
   _e_comp_win_render_queue(cw);
   if (!cw->shape) return;
   cw->shape->visible = 0;
   e_container_shape_show(cw->shape);
}

static void
_e_comp_win_real_hide(E_Comp_Win *cw)
{
   if (cw->bd)
     {
        _e_comp_win_hide(cw);
        return;
     }
   cw->real_hid = 1;
   _e_comp_win_hide(cw);
}

static void
_e_comp_win_hide(E_Comp_Win *cw)
{
   Eina_List *l;
   Evas_Object *o;

   if ((!cw->visible) && (!cw->defer_hide)) return;
   cw->visible = 0;
   if ((cw->input_only) || (cw->invalid)) return;
   DBG("  [0x%x] hid --", cw->win);
   if (!cw->force)
     {
        cw->defer_hide = 1;
        edje_object_signal_emit(cw->shobj, "e,state,visible,off", "e");
        if (!cw->animating)
          {
             cw->c->animating++;
          }
        cw->animating = 1;
        _e_comp_win_render_queue(cw);

        cw->pending_count++;
        _e_comp_event_source_visibility(cw);
        if (cw->shape) e_container_shape_hide(cw->shape);
        return;
     }
   cw->defer_hide = 0;
   cw->force = 0;
   _e_comp_child_hide(cw);

   if (cw->update_timeout)
     {
        ecore_timer_del(cw->update_timeout);
        cw->update_timeout = NULL;
     }
   if (conf->keep_unmapped)
     {
        if (conf->send_flush)
          {
             if (cw->bd) ecore_x_e_comp_flush_send(cw->bd->client.win);
             else ecore_x_e_comp_flush_send(cw->win);
          }
        if (conf->send_dump)
          {
             if (cw->bd) ecore_x_e_comp_dump_send(cw->bd->client.win);
             else ecore_x_e_comp_dump_send(cw->win);
          }
        return;
     }
   if (cw->ready_timeout)
     {
        ecore_timer_del(cw->ready_timeout);
        cw->ready_timeout = NULL;
     }

   if (cw->native)
     {
        evas_object_image_native_surface_set(cw->obj, NULL);
        cw->native = 0;
        EINA_LIST_FOREACH(cw->obj_mirror, l, o)
          {
             evas_object_image_native_surface_set(o, NULL);
          }
     }
   if (cw->pixmap)
     {
        ecore_x_pixmap_free(cw->pixmap);
        cw->pixmap = 0;
        cw->pw = 0;
        cw->ph = 0;
        ecore_x_e_comp_pixmap_set(cw->win, cw->pixmap);
        //        cw->show_ready = 0; // hmm maybe not needed?
     }
   if (cw->xim)
     {
        evas_object_image_size_set(cw->obj, 1, 1);
        evas_object_image_data_set(cw->obj, NULL);
        ecore_x_image_free(cw->xim);
        cw->xim = NULL;
        EINA_LIST_FOREACH(cw->obj_mirror, l, o)
          {
             evas_object_image_size_set(o, 1, 1);
             evas_object_image_data_set(o, NULL);
          }
     }
   if (cw->redirected)
     {
        // we redirect all subwindows anyway
        //        ecore_x_composite_unredirect_window(cw->win, ECORE_X_COMPOSITE_UPDATE_MANUAL);
        cw->redirected = 0;
        cw->pw = 0;
        cw->ph = 0;
     }
   _e_comp_win_render_queue(cw);
   if (conf->send_flush)
     {
        if (cw->bd) ecore_x_e_comp_flush_send(cw->bd->client.win);
        else ecore_x_e_comp_flush_send(cw->win);
     }
   if (conf->send_dump)
     {
        if (cw->bd) ecore_x_e_comp_dump_send(cw->bd->client.win);
        else ecore_x_e_comp_dump_send(cw->win);
     }
}

static void
_e_comp_win_raise_above(E_Comp_Win *cw, E_Comp_Win *cw2)
{
   DBG("  [0x%x] abv [0x%x]", cw->win, cw2->win);
   cw->c->wins_invalid = 1;
   cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(cw));
   cw->c->wins = eina_inlist_append_relative(cw->c->wins, EINA_INLIST_GET(cw), EINA_INLIST_GET(cw2));
   _e_comp_win_restack(cw);
   _e_comp_win_render_queue(cw);
   cw->pending_count++;
   _e_comp_event_source_configure(cw);
}

static void
_e_comp_win_raise(E_Comp_Win *cw)
{
   DBG("  [0x%x] rai", cw->win);
   cw->c->wins_invalid = 1;
   cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(cw));
   cw->c->wins = eina_inlist_append(cw->c->wins, EINA_INLIST_GET(cw));
   _e_comp_win_restack(cw);
   _e_comp_win_render_queue(cw);
   cw->pending_count++;
   _e_comp_event_source_configure(cw);
}

static void
_e_comp_win_lower(E_Comp_Win *cw)
{
   DBG("  [0x%x] low", cw->win);
   cw->c->wins_invalid = 1;
   cw->c->wins = eina_inlist_remove(cw->c->wins, EINA_INLIST_GET(cw));
   cw->c->wins = eina_inlist_prepend(cw->c->wins, EINA_INLIST_GET(cw));
   _e_comp_win_restack(cw);
   _e_comp_win_render_queue(cw);
   cw->pending_count++;
   _e_comp_event_source_configure(cw);
}

static void
_e_comp_win_configure(E_Comp_Win *cw, int x, int y, int w, int h, int border)
{
   Eina_Bool moved = EINA_FALSE, resized = EINA_FALSE;

   if (!cw->visible)
     {
        cw->hidden.x = x;
        cw->hidden.y = y;
        cw->border = border;
     }
   else
     {
        if (!((x == cw->x) && (y == cw->y)))
          {
             DBG("  [0x%x] mov %4i %4i", cw->win, x, y);
             cw->x = x;
             cw->y = y;
             //             evas_object_move(cw->shobj, cw->x, cw->y);
             moved = EINA_TRUE;
          }
        cw->hidden.x = x;
        cw->hidden.y = y;
     }
   cw->hidden.w = w;
   cw->hidden.h = h;
   if (cw->counter)
     {
        if (!((w == cw->w) && (h == cw->h)))
          {
#if 1
             cw->w = w;
             cw->h = h;
             cw->needpix = 1;
             // was cw->w / cw->h
             //             evas_object_resize(cw->shobj, cw->pw, cw->ph);
             resized = EINA_TRUE;
             _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
#else
             if (cw->bd)
               {
                  if ((cw->bd->shading) || (cw->bd->shaded))
                    {
                       cw->needpix = 1;
                       // was cw->w / cw->h
                       //                       evas_object_resize(cw->shobj, cw->pw, cw->ph);
                       resized = EINA_TRUE;
                       _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
                    }
                  else
                    cw->update = 0;
               }
             else
               {
                  cw->update = 0;
                  //if (cw->ready_timeout) ecore_timer_del(cw->ready_timeout);
                  //cw->ready_timeout = ecore_timer_add(conf->first_draw_delay, _e_comp_cb_win_show_ready_timeout, cw);
               }
#endif
          }
        if (cw->border != border)
          {
             cw->border = border;
             cw->needpix = 1;
             // was cw->w / cw->h
             //             evas_object_resize(cw->shobj, cw->pw, cw->ph);
             resized = EINA_TRUE;
             _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
          }
        if ((cw->input_only) || (cw->invalid)) return;
     }
   else
     {
        if (!((w == cw->w) && (h == cw->h)))
          {
             DBG("  [0x%x] rsz %4ix%4i", cw->win, w, h);
             cw->w = w;
             cw->h = h;
             cw->needpix = 1;
             // was cw->w / cw->h
             //             evas_object_resize(cw->shobj, cw->pw, cw->ph);
             resized = EINA_TRUE;
             _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
          }
        if (cw->border != border)
          {
             cw->border = border;
             cw->needpix = 1;
             //             evas_object_resize(cw->shobj, cw->pw, cw->ph);
             resized = EINA_TRUE;
             _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
          }
        if ((cw->input_only) || (cw->invalid)) return;
        _e_comp_win_render_queue(cw);
     }
   if (moved || resized) _e_comp_win_geometry_update(cw);
   // add pending manager comp event count to match below config send
   cw->pending_count++;
   _e_comp_event_source_configure(cw);
}

static void
_e_comp_win_damage(E_Comp_Win *cw, int x, int y, int w, int h, Eina_Bool dmg)
{
   if ((cw->input_only) || (cw->invalid)) return;
   DBG("  [0x%x] dmg [%x] %4i %4i %4ix%4i", cw->win, cw->damage, x, y, w, h);
   if ((dmg) && (cw->damage))
     {
        Ecore_X_Region parts;

        parts = ecore_x_region_new(NULL, 0);
        ecore_x_damage_subtract(cw->damage, 0, parts);
        ecore_x_region_free(parts);
        cw->dmg_updates++;
     }
   if (cw->nocomp) return;
   if (cw->c->nocomp)
     {
        cw->nocomp_need_update = EINA_TRUE;
        return;
     }
   e_comp_render_update_add(cw->up, x, y, w, h);
   if (dmg)
     {
        if (cw->counter)
          {
             if (!cw->update_timeout)
               cw->update_timeout = ecore_timer_add
                   (ecore_animator_frametime_get() * 2, _e_comp_win_damage_timeout, cw);
             return;
          }
     }
   if (!cw->update)
     {
        cw->update = 1;
        cw->c->updates = eina_list_append(cw->c->updates, cw);
     }
   _e_comp_win_render_queue(cw);
}

static void
_e_comp_win_reshape(E_Comp_Win *cw)
{
   if (cw->shape_changed) return;
   cw->shape_changed = 1;
   if (cw->c->nocomp)
     {
        cw->nocomp_need_update = EINA_TRUE;
        return;
     }
   if (!cw->update)
     {
        cw->update = 1;
        cw->c->updates = eina_list_append(cw->c->updates, cw);
     }
   e_comp_render_update_add(cw->up, 0, 0, cw->w, cw->h);
   _e_comp_win_render_queue(cw);
}

//////////////////////////////////////////////////////////////////////////
static Eina_Bool
_e_comp_create(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Create *ev = event;
   E_Comp_Win *cw;
   E_Comp *c = _e_comp_find(ev->parent);
   if (!c) return ECORE_CALLBACK_PASS_ON;
   if (_e_comp_win_find(ev->win)) return ECORE_CALLBACK_PASS_ON;
   if (c->win == ev->win) return ECORE_CALLBACK_PASS_ON;
   if (c->ee_win == ev->win) return ECORE_CALLBACK_PASS_ON;
   cw = _e_comp_win_add(c, ev->win);
   if (!cw) return ECORE_CALLBACK_RENEW;
   _e_comp_win_configure(cw, ev->x, ev->y, ev->w, ev->h, ev->border);
   
   if (cw->free_shape)
     {
        Eina_List *l;
        E_Container *con;

        EINA_LIST_FOREACH(c->man->containers, l, con)
          {
             if (!E_INSIDE(ev->x, ev->y, con->x, con->y, con->w, con->h)) continue;
             cw->shape = e_container_shape_add(con);
             break;
          }
        if (!cw->shape) cw->shape = e_container_shape_add(eina_list_data_get(c->man->containers));
     }
   cw->shape->comp_win = cw;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_destroy(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Destroy *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (cw->animating) cw->delete_me = 1;
   else _e_comp_win_del(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Show *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   cw->defer_hide = 0;
   if (cw->visible) return ECORE_CALLBACK_PASS_ON;
   _e_comp_win_show(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_hide(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Hide *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (!cw->visible) return ECORE_CALLBACK_PASS_ON;
   _e_comp_win_real_hide(cw);
   if (cw->free_shape) e_container_shape_hide(cw->shape);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_reparent(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Reparent *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   DBG("== repar [0x%x] to [0x%x]", ev->win, ev->parent);
   if (ev->parent != cw->c->man->root)
     _e_comp_win_del(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_configure(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Configure *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;

   if (ev->abovewin == 0)
     {
        if (EINA_INLIST_GET(cw)->prev) _e_comp_win_lower(cw);
     }
   else
     {
        E_Comp_Win *cw2 = _e_comp_win_find(ev->abovewin);

        if (cw2)
          {
             E_Comp_Win *cw3 = (E_Comp_Win *)(EINA_INLIST_GET(cw)->prev);

             if (cw3 != cw2) _e_comp_win_raise_above(cw, cw2);
          }
     }

   if (!((cw->x == ev->x) && (cw->y == ev->y) &&
         (cw->w == ev->w) && (cw->h == ev->h) &&
         (cw->border == ev->border)))
     {
        _e_comp_win_configure(cw, ev->x, ev->y, ev->w, ev->h, ev->border);
        if (cw->free_shape)
          {
             if (cw->visible)
               e_container_shape_move(cw->shape, cw->x, cw->y);
             else
               e_container_shape_move(cw->shape, cw->hidden.x, cw->hidden.y);
             e_container_shape_resize(cw->shape, cw->w, cw->h);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_stack(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Stack *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (ev->detail == ECORE_X_WINDOW_STACK_ABOVE) _e_comp_win_raise(cw);
   else _e_comp_win_lower(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_comp_win_opacity_set(E_Comp_Win *cw)
{
   unsigned int val;

   if (ecore_x_window_prop_card32_get(cw->win, ECORE_X_ATOM_NET_WM_WINDOW_OPACITY, &val, 1) > 0)
     {
        cw->opacity = (val >> 24);
        evas_object_color_set(cw->shobj, cw->opacity, cw->opacity, cw->opacity, cw->opacity);
     }
}

static Eina_Bool
_e_comp_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ecore_X_Event_Window_Property *ev = event;

   if (ev->atom == ECORE_X_ATOM_NET_WM_WINDOW_OPACITY)
     {
        E_Comp_Win *cw = _e_comp_win_find(ev->win);
        if (!cw) return ECORE_CALLBACK_PASS_ON;
        _e_comp_win_opacity_set(cw);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_message(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Client_Message *ev = event;
   E_Comp_Win *cw = NULL;
   int version, w = 0, h = 0;
   Eina_Bool force = 0;

   if (ev->message_type == ECORE_X_ATOM_NET_WM_WINDOW_OPACITY)
     {
        cw = _e_comp_win_find(ev->win);
        if (!cw) return ECORE_CALLBACK_PASS_ON;
        _e_comp_win_opacity_set(cw);
        return ECORE_CALLBACK_PASS_ON;
     }

   if ((ev->message_type != ECORE_X_ATOM_E_COMP_SYNC_DRAW_DONE) ||
       (ev->format != 32)) return ECORE_CALLBACK_PASS_ON;
   version = ev->data.l[1];
   cw = _e_comp_border_client_find(ev->data.l[0]);
   if (cw)
     {
        if (!cw->bd) return ECORE_CALLBACK_PASS_ON;
        if (ev->data.l[0] != (int)cw->bd->client.win) return ECORE_CALLBACK_PASS_ON;
     }
   else
     {
        cw = _e_comp_win_find(ev->data.l[0]);
        if (!cw) return ECORE_CALLBACK_PASS_ON;
        if (ev->data.l[0] != (int)cw->win) return ECORE_CALLBACK_PASS_ON;
     }
   if (version == 1) // v 0 was first, v1 added size params
     {
        w = ev->data.l[2];
        h = ev->data.l[3];
        if (cw->bd)
          {
             int clw, clh;

             if ((cw->bd->shading) || (cw->bd->shaded)) force = 1;
             clw = cw->hidden.w -
               cw->bd->client_inset.l -
               cw->bd->client_inset.r;
             clh = cw->hidden.h -
               cw->bd->client_inset.t -
               cw->bd->client_inset.b;
             DBG("  [0x%x] sync draw done @%4ix%4i, bd %4ix%4i",
                 cw->win, w, h, cw->bd->client.w, cw->bd->client.h);
             if ((w != clw) || (h != clh))
               {
                  cw->misses++;
                  if (cw->misses > 1)
                    {
                       cw->misses = 0;
                       force = 1;
                    }
                  else return ECORE_CALLBACK_PASS_ON;
               }
             cw->misses = 0;
          }
        else
          {
             DBG("  [0x%x] sync draw done @%4ix%4i, cw %4ix%4i",
                 cw->win, w, h, cw->hidden.w, cw->hidden.h);
             if ((w != cw->hidden.w) || (h != cw->hidden.h))
               {
                  if (cw->misses > 1)
                    {
                       cw->misses = 0;
                       force = 1;
                    }
                  else return ECORE_CALLBACK_PASS_ON;
               }
             cw->misses = 0;
          }
     }
   DBG("  [0x%x] sync draw done %4ix%4i", cw->win, cw->w, cw->h);
   //   if (cw->bd)
   {
      if (cw->counter)
        {
           DBG("  [0x%x] have counter", cw->win);
           cw->show_ready = 1;
           if (!cw->update)
             {
                DBG("  [0x%x] set update", cw->win);
                if (cw->update_timeout)
                  {
                     DBG("  [0x%x] del timeout", cw->win);
                     ecore_timer_del(cw->update_timeout);
                     cw->update_timeout = NULL;
                  }
                cw->update = 1;
                cw->c->updates = eina_list_append(cw->c->updates, cw);
             }
           if ((cw->w != cw->hidden.w) ||
               (cw->h != cw->hidden.h) ||
               (force))
             {
                DBG("  [0x%x] rsz done msg %4ix%4i", cw->win, cw->hidden.w, cw->hidden.h);
                cw->w = cw->hidden.w;
                cw->h = cw->hidden.h;
                cw->needpix = 1;
                // was cw->w / cw->h
                //                evas_object_resize(cw->shobj, cw->pw, cw->ph);
                _e_comp_win_geometry_update(cw);
                _e_comp_win_damage(cw, 0, 0, cw->w, cw->h, 0);
             }
           cw->drawme = 1;
           _e_comp_win_render_queue(cw);
        }
   }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_shape(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Shape *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (ev->type != ECORE_X_SHAPE_BOUNDING) return ECORE_CALLBACK_PASS_ON;
   _e_comp_win_reshape(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_damage(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Damage *ev = event;
   E_Comp_Win *cw = _e_comp_win_damage_find(ev->damage);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   _e_comp_win_damage(cw, ev->area.x, ev->area.y, ev->area.width, ev->area.height, 1);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_damage_win(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Damage *ev = event;
   Eina_List *l;
   E_Comp *c;

   // fixme: use hash if compositors list > 4
   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (ev->win == c->ee_win)
          {
             // expose on comp win - init win or some other bypass win did it
             DBG("JOB4...");
             _e_comp_render_queue(c);
             break;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_override_expire(void *data)
{
   E_Comp *c = data;

   c->nocomp_override_timer = NULL;
   c->nocomp_override--;

   if (c->nocomp_override <= 0)
     {
        c->nocomp_override = 0;
        if (c->nocomp_want) _e_comp_cb_nocomp_begin(c);
     }
   return EINA_FALSE;
}

static void
_e_comp_override_timed_pop(E_Comp *c)
{
   if (c->nocomp_override <= 0) return;
   if (c->nocomp_override_timer)
     ecore_timer_del(c->nocomp_override_timer);
   c->nocomp_override_timer =
     ecore_timer_add(5.0, _e_comp_override_expire, c);
}

/* here for completeness
   static void
   _e_comp_override_pop(E_Comp *c)
   {
   c->nocomp_override--;
   if (c->nocomp_override <= 0)
   {
   c->nocomp_override = 0;
   if (c->nocomp_want) _e_comp_cb_nocomp_begin(c);
   }
   }
 */

static void
_e_comp_override_push(E_Comp *c)
{
   c->nocomp_override++;
   if ((c->nocomp_override > 0) && (c->nocomp)) _e_comp_cb_nocomp_end(c);
}

static void
_e_comp_fade_handle(E_Comp_Zone *cz, int out, double tim)
{
   if (out == 1)
     {
        if (e_backlight_exists())
          {
             e_backlight_update();
             cz->bloff = EINA_TRUE;
             cz->bl = e_backlight_level_get(cz->zone);
             e_backlight_level_set(cz->zone, 0.0, tim);
          }
     }
   else
     {
        if (e_backlight_exists())
          {
             cz->bloff = EINA_FALSE;
             e_backlight_update();
             if (e_backlight_mode_get(cz->zone) != E_BACKLIGHT_MODE_NORMAL)
               e_backlight_mode_set(cz->zone, E_BACKLIGHT_MODE_NORMAL);
             else
               e_backlight_level_set(cz->zone, e_config->backlight.normal, tim);
          }
     }
}

static Eina_Bool
_e_comp_screensaver_on(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l, *ll;
   E_Comp_Zone *cz;
   E_Comp *c;

   // fixme: use hash if compositors list > 4
   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (!c->saver)
          {
             c->saver = EINA_TRUE;
             EINA_LIST_FOREACH(c->zones, ll, cz)
               {
                  _e_comp_override_push(c);
                  _e_comp_fade_handle(cz, 1, 3.0);
                  edje_object_signal_emit(cz->base, "e,state,screensaver,on", "e");
                  edje_object_signal_emit(cz->over, "e,state,screensaver,on", "e");
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_screensaver_off(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l, *ll;
   E_Comp_Zone *cz;
   E_Comp *c;

   // fixme: use hash if compositors list > 4
   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (c->saver)
          {
             c->saver = EINA_FALSE;
             EINA_LIST_FOREACH(c->zones, ll, cz)
               {
                  edje_object_signal_emit(cz->base, "e,state,screensaver,off", "e");
                  edje_object_signal_emit(cz->over, "e,state,screensaver,off", "e");
                  _e_comp_fade_handle(cz, 0, 0.5);
                  _e_comp_override_timed_pop(c);
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_comp_screens_eval(E_Comp *c)
{
   Eina_List *l, *ll;
   E_Container *con;
   E_Zone *zone;
   E_Comp_Zone *cz;

   EINA_LIST_FREE(c->zones, cz)
     {
        evas_object_del(cz->base);
        evas_object_del(cz->over);
        if (cz->bloff)
          {
             if (e_backlight_mode_get(cz->zone) != E_BACKLIGHT_MODE_NORMAL)
               e_backlight_mode_set(cz->zone, E_BACKLIGHT_MODE_NORMAL);
             e_backlight_level_set(cz->zone, e_config->backlight.normal, 0.0);
          }
        if (cz->zone) cz->zone->comp_zone = NULL;
        free(cz);
     }
   EINA_LIST_FOREACH(c->man->containers, l, con)
     {
        EINA_LIST_FOREACH(con->zones, ll, zone)
          {
             cz = E_NEW(E_Comp_Zone, 1);
             cz->zone = zone;
             cz->comp = c;
             zone->comp_zone = cz;
             cz->container_num = zone->container->num;
             cz->zone_num = zone->num;
             cz->x = zone->x;
             cz->y = zone->y;
             cz->w = zone->w;
             cz->h = zone->h;
             e_comp_zone_update(cz);
             c->zones = eina_list_append(c->zones, cz);
          }
     }
   e_layout_freeze(c->layout);
   evas_object_move(c->layout, 0, 0);
   evas_object_resize(c->layout, c->man->w, c->man->h);
   e_layout_virtual_size_set(c->layout, c->man->w, c->man->h);
   e_layout_thaw(c->layout);
}

static void
_e_comp_screen_change(void *data)
{
   E_Comp *c = data;

   c->screen_job = NULL;
   if (!c->nocomp) ecore_evas_resize(c->ee, c->man->w, c->man->h);
   _e_comp_screens_eval(c);
}

static Eina_Bool
_e_comp_randr(void *data EINA_UNUSED, int type EINA_UNUSED, EINA_UNUSED void *event)
{
   Eina_List *l;
   E_Comp *c;

   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (c->screen_job) ecore_job_del(c->screen_job);
        c->screen_job = ecore_job_add(_e_comp_screen_change, c);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_zonech(void *data EINA_UNUSED, int type EINA_UNUSED, EINA_UNUSED void *event)
{
   Eina_List *l;
   E_Comp *c;

   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (c->screen_job) ecore_job_del(c->screen_job);
        c->screen_job = ecore_job_add(_e_comp_screen_change, c);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Add *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   // fimxe: add/enable compositing here not in show event for borders
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Remove *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (cw->bd == ev->border) _e_comp_object_del(cw, ev->border);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Show *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (cw->visible) return ECORE_CALLBACK_PASS_ON;
   _e_comp_win_show(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_hide(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Hide *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (!cw->visible) return ECORE_CALLBACK_PASS_ON;
   _e_comp_win_hide(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Move *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   // fimxe: do move here for composited bd
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Resize *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   // fimxe: do resize here instead of conf notify
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_iconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Iconify *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   // fimxe: special iconfiy anim
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_uniconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Uniconify *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   // fimxe: special uniconfiy anim
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_urgent_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Urgent_Change *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   if (cw->bd->client.icccm.urgent)
     edje_object_signal_emit(cw->shobj, "e,state,urgent,on", "e");
   else
     edje_object_signal_emit(cw->shobj, "e,state,urgent,off", "e");
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_focus_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Focus_In *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   edje_object_signal_emit(cw->shobj, "e,state,focus,on", "e");
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_focus_out(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Focus_Out *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   edje_object_signal_emit(cw->shobj, "e,state,focus,off", "e");
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Property *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   // fimxe: other properties?
   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_comp_reshadow(E_Comp_Win *cw)
{
   if (cw->visible) evas_object_hide(cw->shobj);
   _e_comp_win_shadow_setup(cw);
   //   evas_object_move(cw->shobj, cw->x, cw->y);
   //   evas_object_resize(cw->shobj, cw->pw, cw->ph);
   _e_comp_win_geometry_update(cw);
   if (cw->visible)
     {
        evas_object_show(cw->shobj);
        if (cw->show_ready)
          {
             cw->defer_hide = 0;
             if (!cw->hidden_override) _e_comp_child_show(cw);
             edje_object_signal_emit(cw->shobj, "e,state,visible,on", "e");
             if (!cw->animating)
               {
                  cw->c->animating++;
               }
             cw->animating = 1;
             _e_comp_win_render_queue(cw);
          }
     }
}

static Eina_Bool
_e_comp_bd_fullscreen(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Property *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   _e_comp_reshadow(cw);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_bd_unfullscreen(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Property *ev = event;
   E_Comp_Win *cw = _e_comp_win_find(ev->border->win);
   if (!cw) return ECORE_CALLBACK_PASS_ON;
   _e_comp_reshadow(cw);
   return ECORE_CALLBACK_PASS_ON;
}

#ifdef SHAPE_DEBUG
static void
_e_comp_shape_debug_rect(E_Comp *c, Ecore_X_Rectangle *rect, E_Color *color)
{
   Evas_Object *o;

#define COLOR_INCREMENT 30
   o = evas_object_rectangle_add(c->evas);
   if (color->r < 256 - COLOR_INCREMENT)
     evas_object_color_set(o, (color->r += COLOR_INCREMENT), 0, 0, 255);
   else if (color->g < 256 - COLOR_INCREMENT)
     evas_object_color_set(o, 0, (color->g += COLOR_INCREMENT), 0, 255);
   else
     evas_object_color_set(o, 0, 0, (color->b += COLOR_INCREMENT), 255);
   evas_object_repeat_events_set(o, 1);
   evas_object_raise(o);
   e_layout_pack(c->layout, o);
   e_layout_child_move(o, rect->x, rect->y);
   e_layout_child_resize(o, rect->width, rect->height);
   e_layout_child_raise(o);
   c->debug_rects = eina_list_append(c->debug_rects, o);
   evas_object_show(o);
}
#endif

static Eina_Bool
_e_comp_shapes_update_object_checker_function_thingy(E_Comp *c, Evas_Object *o)
{
   Eina_List *l, *ll;
   E_Container *con;
   E_Zone *zone;
   E_Comp_Zone *cz;

   EINA_LIST_FOREACH(c->zones, l, cz)
     if ((o == cz->over) || (o == cz->base)) return EINA_TRUE;
   EINA_LIST_FOREACH(c->man->containers, l, con)
     {
        if (o == con->bg_blank_object) return EINA_TRUE;
        EINA_LIST_FOREACH(con->zones, ll, zone)
          if ((o == zone->bg_object) || (o == zone->bg_event_object) ||
              (o == zone->bg_clip_object) || (o == zone->prev_bg_object) ||
              (o == zone->transition_object))
            return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_e_comp_shapes_update_job(E_Comp *c)
{
   Eina_Tiler *tb;
   Evas_Object *o;
   Eina_Rectangle *tr;
   Eina_Iterator *ti;
   Ecore_X_Rectangle *exr;
   unsigned int i, tile_count;
   int x, y, w, h;
   Eina_Bool layout = EINA_FALSE;
#ifdef SHAPE_DEBUG
   Eina_Rectangle *r;
   Eina_List *rl = NULL;
   E_Color color = {0};
#endif

   E_FREE_LIST(c->debug_rects, evas_object_del);
   tb = eina_tiler_new(c->man->w, c->man->h);
   eina_tiler_tile_size_set(tb, 1, 1);
   eina_tiler_rect_add(tb, &(Eina_Rectangle){c->man->x, c->man->y, c->man->w, c->man->h});
   o = evas_object_bottom_get(c->evas);
   for (; o; o = evas_object_above_get(o))
     {
        if (o == c->layout)
          {
             Evas_Object *ch;
             Eina_List *l;

             layout = EINA_TRUE; //ignore all objects under layout
             l = e_layout_children_get(o);
             EINA_LIST_FREE(l, ch)
               {
                  E_Comp_Win *cw;

                  cw = evas_object_data_get(ch, "comp_win");
                  if (cw)
                    {
                       /* ignore deleted shapes */
                       if (!cw->shape)
                         {
                            SHAPE_INF("IGNORING DELETED: %u", cw->win);
                            continue;
                         }
                       if (cw->invalid || cw->real_hid || (!cw->visible) || (!cw->shape->visible)) continue;
#ifdef SHAPE_DEBUG
                       if (cw->bd)
                         INF("COMP BD: %u", cw->win);
                       else if (cw->pop)
                         INF("COMP POP: %u", cw->win);
                       else if (cw->menu)
                         INF("COMP MENU: %u", cw->win);
#endif

                       if (cw->rects)
                         {
                            int num;
                            Ecore_X_Rectangle *rect;
                          
                            for (num = 0, rect = cw->rects; num < cw->rects_num; num++, rect++)
                              {
                                 x = rect->x, y = rect->y, w = rect->width, h = rect->height;
                                 if (cw->bd)
                                   x += cw->bd->x, y += cw->bd->y;
                                 else
                                   x += cw->x, y += cw->y;
//#ifdef SHAPE_DEBUG not sure we can shape check these?
                                 //r = E_NEW(Eina_Rectangle, 1);
                                 //EINA_RECTANGLE_SET(r, x, y, w, h);
                                 //rl = eina_list_append(rl, r);
//#endif
                                 eina_tiler_rect_del(tb, &(Eina_Rectangle){x, y, w, h});
                                 SHAPE_INF("DEL: %d,%d@%dx%d", x, y, w, h);
                              }
                            continue;
                         }
                       /* borders and popups sometimes call shape changes before the changes have
                        * propagated to the comp_win :/
                        */
                       if (cw->bd)
                         x = cw->bd->x, y = cw->bd->y, w = cw->bd->w, h = cw->bd->h;
                       else if (cw->pop)
                         x = cw->pop->x + cw->pop->zone->x, y = cw->pop->y + cw->pop->zone->y, w = cw->pop->w, h = cw->pop->h;
/*
                       else if (cw->menu)
                         x = cw->menu->cur.x, y = cw->menu->cur.y, w = cw->menu->cur.w, h = cw->menu->cur.h;
*/
                       else
                         x = cw->x, y = cw->y, w = cw->w, h = cw->h;
#ifdef SHAPE_DEBUG
                       r = E_NEW(Eina_Rectangle, 1);
                       EINA_RECTANGLE_SET(r, x, y, w, h);
                       rl = eina_list_append(rl, r);
#endif
                       eina_tiler_rect_del(tb, &(Eina_Rectangle){x, y, w, h});
                       SHAPE_INF("DEL: %d,%d@%dx%d", x, y, w, h);
                    }
                  else if (evas_object_visible_get(ch))
                    {
                       evas_object_geometry_get(o, &x, &y, &w, &h);
                       eina_tiler_rect_add(tb, &(Eina_Rectangle){x, y, w, h});
                       SHAPE_INF("ADD: %d,%d@%dx%d", x, y, w, h);
                    }
                  
               }
          }
        else if (layout && evas_object_visible_get(o))
          {
             if (_e_comp_shapes_update_object_checker_function_thingy(c, o)) continue;
             SHAPE_INF("OBJ: %p:%s", o, evas_object_name_get(o));
             evas_object_geometry_get(o, &x, &y, &w, &h);
             eina_tiler_rect_add(tb, &(Eina_Rectangle){x, y, w, h});
             SHAPE_INF("ADD: %d,%d@%dx%d", x, y, w, h);
          }
     }

   ti = eina_tiler_iterator_new(tb);
   tile_count = 128;
   exr = malloc(sizeof(Ecore_X_Rectangle) * tile_count);
   i = 0;
   EINA_ITERATOR_FOREACH(ti, tr)
     {
        exr[i++] = *(Ecore_X_Rectangle*)((char*)tr);
        if (i == tile_count - 1)
          exr = realloc(exr, sizeof(Ecore_X_Rectangle) * (tile_count *= 2));
#ifdef SHAPE_DEBUG
        Eina_List *l;

        _e_comp_shape_debug_rect(c, &exr[i - 1], &color);
        INF("%d,%d @ %dx%d", exr[i - 1].x, exr[i - 1].y, exr[i - 1].width, exr[i - 1].height);
        EINA_LIST_FOREACH(rl, l, r)
          {
             if (E_INTERSECTS(r->x, r->y, r->w, r->h, tr->x, tr->y, tr->w, tr->h))
               ERR("RECT FAIL!!!!");
          }
#endif
     }
   ecore_x_window_shape_input_rectangles_set(c->win, exr, i);
#ifdef SHAPE_DEBUG
   E_FREE_LIST(rl, free);
   printf("\n");
#endif
   free(exr);
   eina_iterator_free(ti);
   eina_tiler_free(tb);
   c->shape_job = NULL;
}

static void
_e_comp_shapes_update(void *data, E_Container_Shape *es, E_Container_Shape_Change change)
{
   E_Comp *c = data;
#ifdef SHAPE_DEBUG
   const char const *change_text[] =
   {
    "ADD",
    "DEL",
    "SHOW",
    "HIDE",
    "MOVE",
    "RESIZE",
    "RECTS"
   };
#endif

   if (change == E_CONTAINER_SHAPE_ADD) return;
   if (es)
     {
        if (!es->comp_win) return;
        switch (change)
          {
           /* these need to always get through since
            * it's guaranteed input shape will change when they occur
            */
           case E_CONTAINER_SHAPE_SHOW:
           case E_CONTAINER_SHAPE_HIDE:
           case E_CONTAINER_SHAPE_DEL:
             break;
           default:
             /* any other changes only matter if the
              * object is visible
              */
             if (!es->visible) return;
          }
     }
   SHAPE_INF("RESHAPE %u: %s", es->comp_win->win, change_text[change]);
   if (!c->shape_job) c->shape_job = ecore_job_add((Ecore_Cb)_e_comp_shapes_update_job, c);
}

//////////////////////////////////////////////////////////////////////////
static void
_e_comp_fps_toggle(void)
{
   Eina_List *l;
   E_Comp *c;

   if (conf->fps_show)
     {
        conf->fps_show = 0;
        e_config_save_queue();
     }
   else
     {
        conf->fps_show = 1;
        e_config_save_queue();
     }
   EINA_LIST_FOREACH(compositors, l, c)
     _e_comp_cb_update(c);
}

static Eina_Bool
_e_comp_key_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev = event;

   if ((!strcasecmp(ev->keyname, "f")) &&
       (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT) &&
       (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL) &&
       (ev->modifiers & ECORE_EVENT_MODIFIER_ALT))
     {
        _e_comp_fps_toggle();
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_signal_user(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Signal_User *ev = event;

   if (ev->number == 1)
     {
        // e17 uses this to pop up config panel
     }
   else if (ev->number == 2)
     {
        _e_comp_fps_toggle();
     }
   return ECORE_CALLBACK_PASS_ON;
}

//////////////////////////////////////////////////////////////////////////
static void
_e_comp_populate(E_Comp *c)
{
   Ecore_X_Window *wins;
   Eina_List *l;
   E_Container *con;
   int i, num;

   c->layout = e_layout_add(c->evas);
   evas_object_name_set(c->layout, "c->layout");
   E_LAYER_SET_ABOVE(c->layout, E_COMP_CANVAS_LAYER_LAYOUT);
   evas_object_show(c->layout);

   EINA_LIST_FOREACH(c->man->containers, l, con)
     {
        e_container_shape_change_callback_add(con, _e_comp_shapes_update, c);
        ecore_x_window_configure(c->ee_win,
          ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING | ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
          0, 0, 0, 0, 0, con->layers[0].win, ECORE_X_WINDOW_STACK_BELOW);
     }
   wins = ecore_x_window_children_get(c->man->root, &num);
   if (!wins) return;
   for (i = 0; i < num; i++)
     {
        E_Comp_Win *cw;
        int x, y, w, h, border;
        char *wname = NULL, *wclass = NULL;

        ecore_x_icccm_name_class_get(wins[i], &wname, &wclass);
        if ((c->man->initwin == wins[i]) ||
            ((wname) && (wclass) && (!strcmp(wname, "E")) &&
             (!strcmp(wclass, "Init_Window"))))
          {
             free(wname);
             free(wclass);
             ecore_x_window_reparent(wins[i], c->win, 0, 0);
             ecore_x_sync();
             continue;
          }
        free(wname);
        free(wclass);
        wname = wclass = NULL;
        cw = _e_comp_win_add(c, wins[i]);
        if (!cw) continue;
        ecore_x_window_geometry_get(cw->win, &x, &y, &w, &h);
        border = ecore_x_window_border_width_get(cw->win);
        if (wins[i] == c->win) continue;
        _e_comp_win_configure(cw, x, y, w, h, border);
        if (ecore_x_window_visible_get(wins[i]))
          _e_comp_win_show(cw);
        if (cw->free_shape)
          {
             EINA_LIST_FOREACH(c->man->containers, l, con)
               {
                  if (!E_INSIDE(x, y, con->x, con->y, con->w, con->h)) continue;
                  cw->shape = e_container_shape_add(con);
                  break;
               }
             if (!cw->shape) cw->shape = e_container_shape_add(eina_list_data_get(c->man->containers));
          }
        cw->shape->comp_win = cw;
     }
   free(wins);
}

static E_Comp *
_e_comp_add(E_Manager *man)
{
   E_Comp *c;
   Ecore_X_Window_Attributes att;
   Eina_Bool res;

   c = calloc(1, sizeof(E_Comp));
   if (!c) return NULL;

   res = ecore_x_screen_is_composited(man->num);
   if (res)
     {
        e_util_dialog_internal
          (_("Compositor Error"), _("Another compositor is already running<br>"
            "on your display server."));
        free(c);
        return NULL;
     }

   c->cm_selection = ecore_x_window_input_new(man->root, 0, 0, 1, 1);
   if (!c->cm_selection)
     {
        free(c);
        return NULL;
     }
   ecore_x_screen_is_composited_set(man->num, c->cm_selection);

   ecore_x_e_comp_sync_supported_set(man->root, conf->efl_sync);

   c->man = man;
   man->comp = c;
   c->win = ecore_x_composite_render_window_enable(man->root);
   if (!c->win)
     {
        e_util_dialog_internal
          (_("Compositor Error"), _("Your display server does not support the<br>"
            "compositor overlay window. This is needed<br>"
            "for it to function."));
        free(c);
        return NULL;
     }

   memset((&att), 0, sizeof(Ecore_X_Window_Attributes));
   ecore_x_window_attributes_get(c->win, &att);

   if ((att.depth != 24) && (att.depth != 32))
     {
        /*
                  e_util_dialog_internal
                  (_("Compositor Error"), _("Your screen is not in 24/32bit display mode.<br>"
                  "This is required to be your default depth<br>"
                  "setting for the compositor to work properly."));
                  ecore_x_composite_render_window_disable(c->win);
                  free(c);
                  return NULL;
         */
     }

   if (c->man->num == 0) e_alert_composite_win(c->man->root, c->win);

   if (conf->engine == E_COMP_ENGINE_GL)
     {
        int opt[20];
        int opt_i = 0;

        if (conf->indirect)
          {
             opt[opt_i] = ECORE_EVAS_GL_X11_OPT_INDIRECT;
             opt_i++;
             opt[opt_i] = 1;
             opt_i++;
          }
        if (conf->vsync)
          {
             opt[opt_i] = ECORE_EVAS_GL_X11_OPT_VSYNC;
             opt_i++;
             opt[opt_i] = 1;
             opt_i++;
          }
#ifdef ECORE_EVAS_GL_X11_OPT_SWAP_MODE
        if (conf->swap_mode)
          {
             opt[opt_i] = ECORE_EVAS_GL_X11_OPT_SWAP_MODE;
             opt_i++;
             opt[opt_i] = conf->swap_mode;
             opt_i++;
          }
#endif
        if (opt_i > 0)
          {
             opt[opt_i] = ECORE_EVAS_GL_X11_OPT_NONE;
             c->ee = ecore_evas_gl_x11_options_new
                 (NULL, c->win, 0, 0, man->w, man->h, opt);
          }
        if (!c->ee)
          c->ee = ecore_evas_gl_x11_new(NULL, c->win, 0, 0, man->w, man->h);
        if (c->ee)
          {
             c->gl = 1;
             ecore_evas_gl_x11_pre_post_swap_callback_set
               (c->ee, c, _e_comp_pre_swap, NULL);
          }
     }
   if (!c->ee)
     {
#if 0
        if (conf->engine == E_COMP_ENGINE_GL)
          {
             e_util_dialog_internal
               (_("Compositor Warning"), _("Your display driver does not support OpenGL, or<br>"
                 "no OpenGL engines were compiled or installed for<br>"
                 "Evas or Ecore-Evas. Falling back to software engine."));
          }
#endif

        c->ee = ecore_evas_software_x11_new(NULL, c->win, 0, 0, man->w, man->h);
     }

   {
      Ecore_X_Rectangle rect[1] = {{0}};

      rect[0].width = man->w, rect[0].height = man->h;
      ecore_x_window_shape_input_rectangles_set(c->win, rect, 1);
   }
   ecore_evas_comp_sync_set(c->ee, 0);
   ecore_evas_name_class_set(c->ee, "E", "Comp_EE");
   //   ecore_evas_manual_render_set(c->ee, conf->lock_fps);
   c->evas = ecore_evas_get(c->ee);
   ecore_evas_show(c->ee);

   c->ee_win = ecore_evas_window_get(c->ee);
   ecore_x_composite_redirect_subwindows
     (c->man->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);

   /* ensure we're in main loop so managers and containers have been set up */
   ecore_job_add((Ecore_Cb)_e_comp_populate, c);

   ecore_x_window_key_grab(c->man->root, "Home", ECORE_EVENT_MODIFIER_SHIFT |
                           ECORE_EVENT_MODIFIER_CTRL |
                           ECORE_EVENT_MODIFIER_ALT, 0);
   ecore_x_window_key_grab(c->man->root, "F", ECORE_EVENT_MODIFIER_SHIFT |
                           ECORE_EVENT_MODIFIER_CTRL |
                           ECORE_EVENT_MODIFIER_ALT, 0);

   return c;
}

static void
_e_comp_del(E_Comp *c)
{
   E_Comp_Win *cw;
   E_Comp_Zone *cz;
   Eina_List *l, *hide_bd = NULL;
   E_Border *bd;
   E_Container *con;

   c->man->comp = NULL;
   EINA_LIST_FOREACH(c->man->containers, l, con)
     e_container_shape_change_callback_del(con, _e_comp_shapes_update, c);
   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        if (!bd->visible)
          hide_bd = eina_list_append(hide_bd, bd);
     }

   E_FN_DEL(evas_object_del, c->fps_fg);
   E_FN_DEL(evas_object_del, c->fps_bg);
   E_FN_DEL(ecore_job_del, c->shape_job);

   ecore_x_window_key_ungrab(c->man->root, "F", ECORE_EVENT_MODIFIER_SHIFT |
                             ECORE_EVENT_MODIFIER_CTRL |
                             ECORE_EVENT_MODIFIER_ALT, 0);
   ecore_x_window_key_ungrab(c->man->root, "Home", ECORE_EVENT_MODIFIER_SHIFT |
                             ECORE_EVENT_MODIFIER_CTRL |
                             ECORE_EVENT_MODIFIER_ALT, 0);
   if (c->grabbed)
     {
        c->grabbed = 0;
        ecore_x_ungrab();
     }
   while (c->wins)
     {
        cw = (E_Comp_Win *)(c->wins);
        if (cw->counter)
          {
             ecore_x_sync_counter_free(cw->counter);
             cw->counter = 0;
          }
        cw->force = 1;
        _e_comp_win_hide(cw);
        cw->force = 1;
        _e_comp_win_del(cw);
     }

   EINA_LIST_FREE(c->zones, cz)
     {
        if (cz->zone) cz->zone->comp_zone = NULL;
        evas_object_del(cz->base);
        evas_object_del(cz->over);
        if (cz->bloff)
          {
             if (e_backlight_mode_get(cz->zone) != E_BACKLIGHT_MODE_NORMAL)
               e_backlight_mode_set(cz->zone, E_BACKLIGHT_MODE_NORMAL);
             e_backlight_level_set(cz->zone, e_config->backlight.normal, 0.0);
          }
        free(cz);
     }

   if (c->layout) evas_object_del(c->layout);

   ecore_evas_free(c->ee);
   ecore_x_composite_unredirect_subwindows
     (c->man->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);
   ecore_x_composite_render_window_disable(c->win);
   if (c->man->num == 0) e_alert_composite_win(c->man->root, 0);
   if (c->render_animator) ecore_animator_del(c->render_animator);
   if (c->new_up_timer) ecore_timer_del(c->new_up_timer);
   if (c->update_job) ecore_job_del(c->update_job);
   if (c->wins_list) eina_list_free(c->wins_list);
   if (c->screen_job) ecore_job_del(c->screen_job);
   if (c->nocomp_delay_timer) ecore_timer_del(c->nocomp_delay_timer);
   if (c->nocomp_override_timer) ecore_timer_del(c->nocomp_override_timer);

   ecore_x_window_free(c->cm_selection);
   ecore_x_e_comp_sync_supported_set(c->man->root, 0);
   ecore_x_screen_is_composited_set(c->man->num, 0);

   EINA_LIST_FREE(hide_bd, bd)
     {
        e_border_show(bd);
        e_border_hide(bd, 1);
     }

   free(c);
}

//////////////////////////////////////////////////////////////////////////

static void
_e_comp_sys_done_cb(void *data, Evas_Object *obj, const char *sig, const char *src)
{
   edje_object_signal_callback_del(obj, sig, src, _e_comp_sys_done_cb);
   e_sys_action_raw_do((E_Sys_Action)(long)data, NULL);
   E_FN_DEL(ecore_timer_del, action_timeout);
}

static Eina_Bool
_e_comp_sys_action_timeout(void *data)
{
   Eina_List *l, *ll;
   E_Comp *c;
   E_Comp_Zone *cz;
   E_Sys_Action a = (long)(intptr_t)data;
   const char *sig = NULL;

   switch (a)
     {
      case E_SYS_LOGOUT:
        sig = "e,state,sys,logout,done";
        break;
      case E_SYS_HALT:
        sig = "e,state,sys,halt,done";
        break;
      case E_SYS_REBOOT:
        sig = "e,state,sys,reboot,done";
        break;
      case E_SYS_SUSPEND:
        sig = "e,state,sys,suspend,done";
        break;
      case E_SYS_HIBERNATE:
        sig = "e,state,sys,hibernate,done";
        break;
      default:
        break;
     }
   E_FN_DEL(ecore_timer_del, action_timeout);
   if (sig)
     {
        EINA_LIST_FOREACH(compositors, l, c)
          EINA_LIST_FOREACH(c->zones, ll, cz)
            edje_object_signal_callback_del(cz->over, sig, "e", _e_comp_sys_done_cb);
     }
   e_sys_action_raw_do(a, NULL);
   return EINA_FALSE;
}

static void
_e_comp_sys_emit_cb_wait(E_Sys_Action a, const char *sig, const char *rep, Eina_Bool nocomp_push)
{
   Eina_List *l, *ll;
   E_Comp_Zone *cz;
   E_Comp *c;
   Eina_Bool first = EINA_TRUE;

   EINA_LIST_FOREACH(compositors, l, c)
     {
        if (nocomp_push) _e_comp_override_push(c);
        else _e_comp_override_timed_pop(c);
        EINA_LIST_FOREACH(c->zones, ll, cz)
          {
             _e_comp_fade_handle(cz, nocomp_push, 0.5);
             edje_object_signal_emit(cz->base, sig, "e");
             edje_object_signal_emit(cz->over, sig, "e");
             if ((rep) && (first))
               edje_object_signal_callback_add(cz->over, rep, "e", _e_comp_sys_done_cb, (void *)(long)a);
             first = EINA_FALSE;
          }
     }
   if (action_timeout) ecore_timer_del(action_timeout);
   action_timeout = ecore_timer_add(ACTION_TIMEOUT, (Ecore_Task_Cb)_e_comp_sys_action_timeout, (intptr_t*)(long)a);
}

static void
_e_comp_sys_suspend(void)
{
   _e_comp_sys_emit_cb_wait(E_SYS_SUSPEND, "e,state,sys,suspend", "e,state,sys,suspend,done", EINA_TRUE);
}

static void
_e_comp_sys_hibernate(void)
{
   _e_comp_sys_emit_cb_wait(E_SYS_HIBERNATE, "e,state,sys,hibernate", "e,state,sys,hibernate,done", EINA_TRUE);
}

static void
_e_comp_sys_reboot(void)
{
   _e_comp_sys_emit_cb_wait(E_SYS_REBOOT, "e,state,sys,reboot", "e,state,sys,reboot,done", EINA_TRUE);
}

static void
_e_comp_sys_shutdown(void)
{
   _e_comp_sys_emit_cb_wait(E_SYS_HALT, "e,state,sys,halt", "e,state,sys,halt,done", EINA_TRUE);
}

static void
_e_comp_sys_logout(void)
{
   _e_comp_sys_emit_cb_wait(E_SYS_LOGOUT, "e,state,sys,logout", "e,state,sys,logout,done", EINA_TRUE);
}

static void
_e_comp_sys_resume(void)
{
   _e_comp_sys_emit_cb_wait(E_SYS_SUSPEND, "e,state,sys,resume", NULL, EINA_FALSE);
}

static Eina_List *
_e_comp_config_engine_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Software", NULL
   };

   if (gl_avail) name[1] = "OpenGL";
   for (x = E_COMP_ENGINE_SW; x <= E_COMP_ENGINE_SW + gl_avail; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x - 1]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_Bool
_e_comp_config_style_thumb_timer_cb(void *data)
{
   Evas_Object *oo, *ofr = data;
   int demo_state;

   demo_state = (long)(intptr_t)evas_object_data_get(data, "style_demo_state");
   demo_state = (demo_state + 1) % 4;
   evas_object_data_set(data, "style_demo_state", (intptr_t *)(long)demo_state);

   oo = evas_object_data_get(ofr, "comp_preview");
   switch (demo_state)
     {
      case 0:
        edje_object_signal_emit(oo, "e,state,visible,on", "e");
        edje_object_signal_emit(oo, "e,state,focus,on", "e");
        edje_object_part_text_set(ofr, "e.text.label", _("Visible"));
        break;

      case 1:
        edje_object_signal_emit(oo, "e,state,focus,off", "e");
        edje_object_part_text_set(ofr, "e.text.label", _("Focus-Out"));
        break;

      case 2:
        edje_object_signal_emit(oo, "e,state,focus,on", "e");
        edje_object_part_text_set(ofr, "e.text.label", _("Focus-In"));
        break;

      case 3:
        edje_object_signal_emit(oo, "e,state,visible,off", "e");
        edje_object_part_text_set(ofr, "e.text.label", _("Hidden"));
        break;

      default:
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_config_style_thumb_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(edje_object_part_swallow_get(obj, "e.swallow.preview"));
   ecore_timer_del(data);
}

static Evas_Object *
_e_comp_config_style_thumb_cb(E_Configure_Option_Info *oi, Evas *evas)
{
   Evas_Object *ob, *oly, *oo, *obd, *ofr, *orec;
   Ecore_Timer *timer;
   char buf[4096];

   ob = e_livethumb_add(evas);
   e_livethumb_vsize_set(ob, 240, 240);

   oly = e_layout_add(e_livethumb_evas_get(ob));
   e_layout_virtual_size_set(oly, 240, 240);
   e_livethumb_thumb_set(ob, oly);
   evas_object_show(oly);

   oo = edje_object_add(e_livethumb_evas_get(ob));
   snprintf(buf, sizeof(buf), "e/comp/%s", oi->name);
   e_theme_edje_object_set(oo, "base/theme/borders", buf);
   e_layout_pack(oly, oo);
   e_layout_child_move(oo, 39, 39);
   e_layout_child_resize(oo, 162, 162);
   edje_object_signal_emit(oo, "e,state,shadow,on", "e");
   edje_object_signal_emit(oo, "e,state,visible,on", "e");
   evas_object_show(oo);

   ofr = edje_object_add(evas);
   e_theme_edje_object_set
     (ofr, "base/theme/comp", "e/comp/preview");
   edje_object_part_swallow(ofr, "e.swallow.preview", ob);
   evas_object_show(ofr);

   obd = edje_object_add(e_livethumb_evas_get(ob));
   e_theme_edje_object_set(obd, "base/theme/borders", "e/widgets/border/default/border");
   edje_object_part_text_set(obd, "e.text.title", _("Title"));
   edje_object_signal_emit(obd, "e,state,focused", "e");
   edje_object_part_swallow(oo, "e.swallow.content", obd);
   evas_object_show(obd);

   orec = evas_object_rectangle_add(e_livethumb_evas_get(ob));
   evas_object_color_set(orec, 0, 0, 0, 128);
   edje_object_part_swallow(obd, "e.swallow.client", orec);
   evas_object_show(orec);

   timer = ecore_timer_add(3.0, _e_comp_config_style_thumb_timer_cb, ofr);
   evas_object_data_set(ofr, "style_demo_state", (void *)1);
   evas_object_data_set(ofr, "comp_timer", timer);
   evas_object_data_set(ofr, "comp_preview", oo);
   evas_object_event_callback_add(ofr, EVAS_CALLBACK_DEL, _e_comp_config_style_thumb_del_cb, timer);

   return ofr;
}

static Eina_List *
_e_comp_config_style_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL, *styles;
   Eina_Stringshare *style;
   E_Configure_Option_Info *oi;

   styles = e_theme_comp_list();
   EINA_LIST_FREE(styles, style)
     {
        oi = e_configure_option_info_new(co, style, style);
        oi->current = (conf->shadow_style == style);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

static Eina_List *
_e_comp_fps_corner_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Top left", "Top right", "Bottom left", "Bottom right", };

   for (x = 0; x <= 3; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

#ifdef ECORE_EVAS_GL_X11_OPT_SWAP_MODE
static Eina_List *
_e_comp_swap_mode_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
      "Auto", "Invalidate (full redraw)", "Copy from back to front", "Double-buffered", "Triple-buffered"
   };

   for (x = ECORE_EVAS_GL_X11_SWAP_MODE_AUTO; x <= ECORE_EVAS_GL_X11_SWAP_MODE_TRIPLE; x++)
     {
        oi = e_configure_option_info_new(co, _(name[x]), (intptr_t *)(long)x);
        oi->current = (*(int *)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}
#endif
static void
_e_comp_cfg_init(void)
{
   E_Configure_Option *co;

   e_configure_option_domain_current_set("e_comp");

   E_CONFIGURE_OPTION_ADD(co, BOOL, fast_borders, conf, _("Use fast composite effects for windows"), _("composite"), _("border"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, fast_menus, conf, _("Use fast composite effects for menus"), _("composite"), _("menu"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, fast_popups, conf, _("Use fast composite effects for popups"), _("composite"), _("popup"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, fast_overrides, conf, _("Use fast composite effects for override-redirect windows (tooltips and such)"), _("composite"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;

   E_CONFIGURE_OPTION_ADD(co, BOOL, match.disable_borders, conf, _("Disable composite effects for windows"), _("composite"), _("border"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, match.disable_menus, conf, _("Disable composite effects for menus"), _("composite"), _("menu"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, match.disable_popups, conf, _("Disable composite effects for popups"), _("composite"), _("popup"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, match.disable_overrides, conf, _("Disable composite effects for override-redirect windows (tooltips and such)"), _("composite"), _("theme"), _("animate"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, disable_screen_effects, conf, _("Disable composite effects for the screen"), _("composite"), _("theme"), _("animate"), _("screen"));
   E_CONFIGURE_OPTION_HELP(co, _("This option disables composite effects from themes, such as animating the screen fade when blanking"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;

   E_CONFIGURE_OPTION_ADD(co, ENUM, engine, conf, _("Compositing engine"), _("composite"), _("border"));
   E_CONFIGURE_OPTION_ICON(co, "preferences-engine");
   co->info_cb = _e_comp_config_engine_info_cb;
   co->requires_restart = 1;
   E_CONFIGURE_OPTION_ADD(co, STR, shadow_style, conf, _("Default window composite effect"), _("composite"), _("border"), _("theme"));
   co->info_cb = _e_comp_config_style_info_cb;
   co->thumb_cb = _e_comp_config_style_thumb_cb;
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;

   E_CONFIGURE_OPTION_ADD(co, BOOL, smooth_windows, conf, _("Smooth scaling of composited window content"), _("composite"), _("border"));
   /* FIXME: help text */
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, efl_sync, conf, _("Sync composited windows"), _("composite"), _("border"));
   /* FIXME: help text */
   co->requires_restart = 1;
   E_CONFIGURE_OPTION_ADD(co, BOOL, loose_sync, conf, _("Loose sync composited windows"), _("composite"), _("border"));
   /* FIXME: help text */
   co->requires_restart = 1;
   E_CONFIGURE_OPTION_ADD(co, BOOL, grab, conf, _("Grab server during rendering of composited windows"), _("composite"), _("border"));
   /* FIXME: help text */
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, DOUBLE, first_draw_delay, conf, _("Initial draw timeout for newly-mapped composited windows"), _("composite"), _("border"), _("delay"));
   E_CONFIGURE_OPTION_MINMAX_STEP_FMT(co, 0.01, 0.5, 0.01, _("1.2f seconds"));
   /* FIXME: help text */
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;

   if (gl_avail)
     {
        E_CONFIGURE_OPTION_ADD(co, BOOL, vsync, conf, _("Tear-free compositing (VSYNC)"), _("composite"), _("border"));
        co->requires_restart = 1;

        E_CONFIGURE_OPTION_ADD(co, BOOL, texture_from_pixmap, conf, _("Texture from pixmap rendering for composite"), _("composite"), _("border"));
        co->requires_restart = 1;
#ifdef ECORE_EVAS_GL_X11_OPT_SWAP_MODE
             if ((evas_version->major >= 1) &&
                 (evas_version->minor >= 7) &&
                 (evas_version->micro >= 99))
               {
                  E_CONFIGURE_OPTION_ADD(co, ENUM, swap_mode, conf, _("Composite swapping method"), _("composite"), _("border"));
                  co->info_cb = _e_comp_swap_mode_info_cb;
                  co->requires_restart = 1;
               }
#endif
     }


   E_CONFIGURE_OPTION_ADD(co, BOOL, nocomp_fs, conf, _("Don't composite fullscreen windows"), _("composite"), _("border"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, send_flush, conf, _("Send flush when compositing windows"), _("composite"), _("border"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, BOOL, send_dump, conf, _("Send dump when compositing windows"), _("composite"), _("border"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;

   E_CONFIGURE_OPTION_ADD(co, BOOL, fps_show, conf, _("Show framerate when compositing windows"), _("composite"), _("border"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, DOUBLE_INT, fps_average_range, conf, _("Rolling average for fps display when compositing"), _("composite"), _("border"));
   E_CONFIGURE_OPTION_MINMAX_STEP_FMT(co, 1, 120, 1, _("1.0f frames"));
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;
   E_CONFIGURE_OPTION_ADD(co, ENUM, fps_corner, conf, _("Composite framerate display corner"), _("composite"), _("border"));
   co->info_cb = _e_comp_fps_corner_info_cb;
   co->funcs[1].none = co->funcs[0].none = e_comp_shadows_reset;

   e_configure_option_category_tag_add(_("windows"), _("composite"));
   e_configure_option_category_tag_add(_("composite"), _("composite"));
   e_configure_option_category_icon_set(_("composite"), "preferences-composite");
}

//////////////////////////////////////////////////////////////////////////

EINTERN Eina_Bool
e_comp_init(void)
{
   if (!ecore_x_composite_query())
     {
        e_util_dialog_internal
          (_("Compositor Error"), _("Your display server does not support XComposite,<br>"
            "or Ecore-X was built without XComposite support.<br>"
            "Note that for composite support you will also need<br>"
            "XRender and XFixes support in X11 and Ecore."));
        return EINA_FALSE;
     }
   if (!ecore_x_damage_query())
     {
        e_util_dialog_internal
          (_("Compositor Error"), _("Your display server does not support XDamage<br>"
            "or Ecore was built without XDamage support."));
        return EINA_FALSE;
     }

   _e_comp_log_dom = eina_log_domain_register("e_comp", EINA_COLOR_YELLOW);
   eina_log_domain_level_set("e_comp", EINA_LOG_LEVEL_INFO);

   e_sys_handlers_set(_e_comp_sys_suspend, _e_comp_sys_hibernate,
     _e_comp_sys_reboot, _e_comp_sys_shutdown, _e_comp_sys_logout, _e_comp_sys_resume);

   windows = eina_hash_string_superfast_new(NULL);
   borders = eina_hash_string_superfast_new(NULL);
   damages = eina_hash_string_superfast_new(NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_CREATE, _e_comp_create, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_DESTROY, _e_comp_destroy, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_SHOW, _e_comp_show, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_HIDE, _e_comp_hide, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_REPARENT, _e_comp_reparent, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_CONFIGURE, _e_comp_configure, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_STACK, _e_comp_stack, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_PROPERTY, _e_comp_property, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_CLIENT_MESSAGE, _e_comp_message, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_SHAPE, _e_comp_shape, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_DAMAGE_NOTIFY, _e_comp_damage, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_DAMAGE, _e_comp_damage_win, NULL);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_SCREENSAVER_ON, _e_comp_screensaver_on, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_SCREENSAVER_OFF, _e_comp_screensaver_off, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_KEY_DOWN, _e_comp_key_down, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_SIGNAL_USER, _e_comp_signal_user, NULL);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONTAINER_RESIZE, _e_comp_randr, NULL);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_MOVE_RESIZE, _e_comp_zonech, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_ADD, _e_comp_zonech, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_DEL, _e_comp_zonech, NULL);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_ADD, _e_comp_bd_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_REMOVE, _e_comp_bd_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_SHOW, _e_comp_bd_show, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_HIDE, _e_comp_bd_hide, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_MOVE, _e_comp_bd_move, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_RESIZE, _e_comp_bd_resize, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_ICONIFY, _e_comp_bd_iconify, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_UNICONIFY, _e_comp_bd_uniconify, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_URGENT_CHANGE, _e_comp_bd_urgent_change, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_FOCUS_IN, _e_comp_bd_focus_in, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_FOCUS_OUT, _e_comp_bd_focus_out, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_PROPERTY, _e_comp_bd_property, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_FULLSCREEN, _e_comp_bd_fullscreen, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BORDER_UNFULLSCREEN, _e_comp_bd_unfullscreen, NULL);

   E_EVENT_COMP_SOURCE_VISIBILITY = ecore_event_type_new();
   E_EVENT_COMP_SOURCE_ADD = ecore_event_type_new();
   E_EVENT_COMP_SOURCE_DEL = ecore_event_type_new();
   E_EVENT_COMP_SOURCE_CONFIGURE = ecore_event_type_new();

   e_comp_cfdata_edd_init(&conf_edd, &conf_match_edd);
   conf = e_config_domain_load("e_comp", conf_edd);
   if (conf)
     {
        conf->max_unmapped_pixels = 32 * 1024;
        conf->keep_unmapped = 1;
     }
   else
     conf = e_comp_cfdata_config_new();

   if (!getenv("ECORE_X_NO_XLIB"))
     {
        if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_X11))
          gl_avail = EINA_TRUE;
     }

#ifdef HAVE_WAYLAND_CLIENTS
   if (!e_comp_wl_init())
     EINA_LOG_ERR("Failed to initialize Wayland Client Support !!");
#endif
   _e_comp_cfg_init();

   return EINA_TRUE;
}

EINTERN Eina_Bool
e_comp_manager_init(E_Manager *man)
{
   E_Comp *c;

   c = _e_comp_add(man);
   if (c)
     compositors = eina_list_append(compositors, c);
   return !!c;
}

EAPI int
e_comp_internal_save(void)
{
   return e_config_domain_save("e_comp", conf_edd, conf);
}

EINTERN int
e_comp_shutdown(void)
{
   if (!compositors) return 1;
   E_FN_DEL(ecore_timer_del, action_timeout);
   E_FREE_LIST(compositors, _e_comp_del);
   E_FREE_LIST(handlers, ecore_event_handler_del);

#ifdef HAVE_WAYLAND_CLIENTS
   e_comp_wl_shutdown();
#endif

   e_configure_option_domain_clear("e_comp");
   e_configure_option_category_tag_del(_("composite"), _("composite"));
   e_configure_option_category_tag_del(_("windows"), _("composite"));

   gl_avail = EINA_FALSE;
   e_comp_cfdata_config_free(conf);
   E_CONFIG_DD_FREE(conf_match_edd);
   E_CONFIG_DD_FREE(conf_edd);
   conf = NULL;
   conf_match_edd = NULL;
   conf_edd = NULL;

   if (damages) eina_hash_free(damages);
   if (windows) eina_hash_free(windows);
   if (borders) eina_hash_free(borders);
   damages = NULL;
   windows = NULL;
   borders = NULL;

   e_sys_handlers_set(NULL, NULL, NULL, NULL, NULL, NULL);
   return 1;
}

EAPI E_Comp_Config *
e_comp_config_get(void)
{
   return conf;
}

EAPI void
e_comp_shadows_reset(void)
{
   Eina_List *l;
   E_Comp *c;

   EINA_LIST_FOREACH(compositors, l, c)
     {
        E_Comp_Win *cw;

        //        ecore_evas_manual_render_set(c->ee, conf->lock_fps);
        _e_comp_fps_update(c);
        E_LIST_FOREACH(c->zones, e_comp_zone_update);
        EINA_INLIST_FOREACH(c->wins, cw)
          {
             if ((cw->shobj) && (cw->obj))
               {
                  _e_comp_win_shadow_setup(cw);

                  if (cw->visible)
                    {
                       edje_object_signal_emit(cw->shobj, "e,state,visible,on", "e");
                       if (!cw->animating)
                         {
                            cw->c->animating++;
                         }
                       _e_comp_win_render_queue(cw);
                       cw->animating = 1;

                       cw->pending_count++;
                       _e_comp_event_source_visibility(cw);
                    }
               }
          }
     }
}

EAPI void
e_comp_render_update(E_Comp *c)
{
   _e_comp_render_queue(c);
}

EAPI E_Comp_Win *
e_comp_win_find_border_win(Ecore_X_Window win)
{
   return _e_comp_border_client_find(win);
}

EAPI E_Comp_Win *
e_comp_win_find(Ecore_X_Window win)
{
   return _e_comp_win_find(win);
}

EAPI const Eina_List *
e_comp_win_list_get(E_Comp *c)
{
   E_Comp_Win *cw;

   if (!c->wins) return NULL;
   if (c->wins_invalid)
     {
        c->wins_invalid = 0;
        if (c->wins_list) eina_list_free(c->wins_list);
        c->wins_list = NULL;
        EINA_INLIST_FOREACH(c->wins, cw)
          {
             if ((cw->shobj) && (cw->obj))
               c->wins_list = eina_list_append(c->wins_list, cw);
          }
     }
   return c->wins_list;
}

EAPI Evas_Object *
e_comp_win_image_mirror_add(E_Comp_Win *cw)
{
   if ((!cw) || (!cw->c)) return NULL;
   return _e_comp_win_mirror_add(cw);
}

EAPI void
e_comp_win_hidden_set(E_Comp_Win *cw, Eina_Bool hidden)
{
   if (!cw->c) return;
   if (cw->hidden_override == hidden) return;
   cw->hidden_override = hidden;
   if (cw->bd) e_border_comp_hidden_set(cw->bd, cw->hidden_override);
   if (cw->visible)
     {
        if (cw->hidden_override)
          _e_comp_child_hide(cw);
        else if (!cw->bd || cw->bd->visible)
          _e_comp_child_show(cw);
     }
   else
     {
        if (cw->hidden_override) _e_comp_child_hide(cw);
     }
}

EAPI E_Comp *
e_comp_get(void *o)
{
   E_Border *bd;
   E_Object *obj = o;
   E_Zone *zone = NULL;
   E_Container *con = NULL;
   E_Manager *man = NULL;

   if (!o) obj = (E_Object*)e_manager_current_get();
   switch (obj->type)
     {
      case E_BORDER_TYPE:
        bd = (E_Border*)obj;
        zone = bd->zone;
        EINA_SAFETY_ON_NULL_RETURN_VAL(zone, NULL);
      case E_ZONE_TYPE:
        if (!zone) zone = (E_Zone*)obj;
        con = zone->container;
        EINA_SAFETY_ON_NULL_RETURN_VAL(con, NULL);
      case E_CONTAINER_TYPE:
        if (!con) con = (E_Container*)obj;
        man = con->manager;
        EINA_SAFETY_ON_NULL_RETURN_VAL(man, NULL);
      case E_MANAGER_TYPE:
        if (!man) man = (E_Manager*)obj;
        return man->comp;
     }
   CRI("UNIMPLEMENTED TYPE PASSED! FIXME!");
   return NULL;
}

EAPI void
e_comp_zone_update(E_Comp_Zone *cz)
{
   Evas_Object *o;
   const char *const over_styles[] =
   {
      "e/comp/screen/overlay/default",
      "e/comp/screen/overlay/noeffects"
   };
   const char *const under_styles[] =
   {
      "e/comp/screen/base/default",
      "e/comp/screen/base/noeffects"
   };

   if (cz->over && cz->base)
     {
        e_theme_edje_object_set(cz->base, "base/theme/comp",
                                under_styles[conf->disable_screen_effects]);
        edje_object_part_swallow(cz->base, "e.swallow.background",
                                 cz->zone->transition_object ?: cz->zone->bg_object);
        e_theme_edje_object_set(cz->over, "base/theme/comp",
                                over_styles[conf->disable_screen_effects]);
        return;
     }
   E_FN_DEL(evas_object_del, cz->base);
   E_FN_DEL(evas_object_del, cz->over);
   cz->base = o = edje_object_add(cz->comp->evas);
   evas_object_repeat_events_set(o, 1);
   evas_object_name_set(cz->base, "cz->base");
   e_theme_edje_object_set(o, "base/theme/comp", under_styles[conf->disable_screen_effects]);
   edje_object_part_swallow(cz->base, "e.swallow.background", cz->zone->transition_object ?: cz->zone->bg_object);
   evas_object_move(o, cz->zone->x, cz->zone->y);
   evas_object_resize(o, cz->zone->w, cz->zone->h);
   E_LAYER_SET(o, E_COMP_CANVAS_LAYER_BG);
   evas_object_show(o);

   cz->over = o = edje_object_add(cz->comp->evas);
   E_LAYER_SET_ABOVE(o, E_COMP_CANVAS_LAYER_MAX);
   evas_object_name_set(cz->over, "cz->over");
   evas_object_pass_events_set(o, 1);
   e_theme_edje_object_set(o, "base/theme/comp", over_styles[conf->disable_screen_effects]);
   evas_object_move(o, cz->zone->x, cz->zone->y);
   evas_object_resize(o, cz->zone->w, cz->zone->h);
   evas_object_raise(o);
   evas_object_show(o);
}

EAPI Ecore_X_Window
e_comp_top_window_at_xy_get(E_Comp *c, Evas_Coord x, Evas_Coord y, Ecore_X_Window *ignore, unsigned int ignore_num)
{
   E_Comp_Win *cw;
   Evas_Object *o;
   Eina_List *ignore_list = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(c, 0);
   o = evas_object_top_at_xy_get(c->evas, x, y, 0, 0);
   if (!o) return c->ee_win;
   if (o != c->layout) return c->ee_win;
   if (ignore && ignore_num)
     {
        unsigned int i;

        for (i = 0; i < ignore_num; i++)
          {
             cw = e_comp_win_find(ignore[i]);
             if (cw)
               ignore_list = eina_list_append(ignore_list, cw->shobj);
          }
     }
   o = e_layout_top_child_at_xy_get(c->layout, x, y, ignore_list);
   eina_list_free(ignore_list);
   cw = evas_object_data_get(o, "comp_win");
   if (!cw) return c->ee_win;
   return cw->win;
}
