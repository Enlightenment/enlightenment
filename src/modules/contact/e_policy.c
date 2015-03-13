#define E_COMP_X

#include "e_mod_main.h"

static Eina_Bool _cb_event_add(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _cb_event_del(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _cb_event_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _cb_event_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event);
static void _cb_hook_post_fetch(void *data __UNUSED__, E_Client *ec);
static void _cb_hook_post_assign(void *data __UNUSED__, E_Client *ec);
static void _cb_hook_layout(E_Comp *comp);

static Eina_List *hooks = NULL;
static Eina_List *handlers = NULL;

static Eina_Bool  kbd_on = EINA_FALSE;
static Eina_Bool  kbd_override = EINA_FALSE;
static Eina_List *clients = NULL;
static E_Client  *ec_active = NULL;

#define LADD(l, f) l = eina_list_append(l, f)

void
e_policy_init(void)
{
   LADD(hooks, e_client_hook_add(E_CLIENT_HOOK_EVAL_POST_FETCH,
                                 _cb_hook_post_fetch, NULL));
   LADD(hooks, e_client_hook_add(E_CLIENT_HOOK_EVAL_POST_FRAME_ASSIGN,
                                 _cb_hook_post_assign, NULL));
   LADD(handlers, ecore_event_handler_add(E_EVENT_CLIENT_ADD,
                                          _cb_event_add, NULL));
   LADD(handlers, ecore_event_handler_add(E_EVENT_CLIENT_REMOVE,
                                          _cb_event_del, NULL));
   LADD(handlers, ecore_event_handler_add(E_EVENT_CLIENT_FOCUS_IN,
                                          _cb_event_focus_in, NULL));
   LADD(handlers, ecore_event_handler_add(E_EVENT_CLIENT_FOCUS_OUT,
                                          _cb_event_focus_out, NULL));
   e_client_layout_cb_set((E_Client_Layout_Cb)_cb_hook_layout);
}

void
e_policy_shutdown(void)
{
   E_Client_Hook *bh;
   Ecore_Event_Handler *eh;
   
   EINA_LIST_FREE(hooks, bh) e_client_hook_del(bh);
   EINA_LIST_FREE(handlers, eh) ecore_event_handler_del(eh);
   e_client_layout_cb_set(NULL);
}

void
e_policy_kbd_override_set(Eina_Bool override)
{
   const Eina_List *l;
   E_Client *ec, *kbd = NULL;;
   
   if (kbd_override == override) return;
   kbd_override = override;
   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (ec->vkbd.vkbd)
          {
             kbd = ec;
          }
     }
   if (kbd)
     {
        ec = kbd;
        e_client_uniconify(ec);
        evas_object_raise(ec->frame);
        evas_object_show(ec->frame);
     }
}

const Eina_List *
e_policy_clients_get(void)
{
   return clients;
}

const E_Client *
e_policy_client_active_get(void)
{
   return ec_active;
}

static Eina_Bool
_cb_event_add(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec = ev->ec;
   
   if (ec_active) clients = eina_list_append_relative(clients, ec, ec_active);
   else clients = eina_list_prepend(clients, ec);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_event_del(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec = ev->ec;
   
   clients = eina_list_remove(clients, ec);
   if (ec_active == ec) ec_active = NULL;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_event_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec = ev->ec;
   
   ec_active = ec;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_event_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec = ev->ec;

   if (ec_active == ec) ec_active = NULL;
   if (kbd_on) e_policy_kbd_override_set(EINA_FALSE);
   return ECORE_CALLBACK_PASS_ON;
}

static void
_cb_hook_post_fetch(void *data __UNUSED__, E_Client *ec)
{
   /* NB: for this policy we disable all remembers set on a client */
   if (ec->remember) e_remember_del(ec->remember);
   ec->remember = NULL;
   
   /* set this client to borderless */
   ec->borderless = 1;
   EC_CHANGED(ec);
}

static void
_cb_hook_post_assign(void *data __UNUSED__, E_Client *ec)
{
   ec->internal_no_remember = 1;

   /* do not allow client to change these properties */
   ec->lock_client_size = 1;
   ec->lock_client_shade = 1;
   ec->lock_client_maximize = 1;
   ec->lock_client_location = 1;
   ec->lock_client_stacking = 1;

   /* do not allow the user to change these properties */
   ec->lock_user_location = 1;
   ec->lock_user_size = 1;
   ec->lock_user_shade = 1;

   /* clear any centered states */
   /* NB: this is mainly needed for E's main config dialog */
   ec->e.state.centered = 0;

   /* lock the border type so user/client cannot change */
   ec->lock_border = 1;
}

static void
_cb_hook_layout(E_Comp *comp)
{
   Eina_List *l;
   E_Client *ec, *kbd = NULL;;
   Eina_Bool want_kbd = EINA_FALSE;
   Eina_Bool have_focused = EINA_FALSE;
   int kx = 0, ky = 0, kw = 0, kh = 0;
   
   EINA_LIST_FOREACH(comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        if (ec->focused) have_focused = EINA_TRUE;
#ifndef HAVE_WAYLAND_ONLY
        if ((ec->focused) &&
            (ec->vkbd.state > ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF))
          want_kbd = EINA_TRUE;
        if (ec->vkbd.vkbd) kbd = ec;
#endif
     }
   
   if ((have_focused) && (kbd_override)) want_kbd = EINA_TRUE;
   
   if (kbd)
     {
        kw = kbd->zone->w;
        kh = kbd->icccm.min_h;
        kx = kbd->zone->x;
        ky = kbd->zone->y + kbd->zone->h - kh;
     }
   EINA_LIST_FOREACH(comp->clients, l, ec)
     {
        int x, y, w, h;
        
        if (!ec->zone) continue;
        if (e_client_util_ignored_get(ec)) continue;
        
        w = ec->zone->w;
        h = ec->zone->h;
        x = ec->zone->x;
        y = ec->zone->y;

        if (ec->vkbd.vkbd)
          {
             x = kx; y = ky; w = kw; h = kh;
             if (want_kbd)
               {
                  e_client_uniconify(ec);
                  evas_object_raise(ec->frame);
                  evas_object_show(ec->frame);
               }
             else
               {
                  e_client_iconify(ec);
               }
          }
        else if (((ec->netwm.type == E_WINDOW_TYPE_DIALOG) ||
                  (ec->icccm.transient_for != 0)) &&
                 ((ec->icccm.min_w == ec->icccm.max_w) &&
                  (ec->icccm.min_h == ec->icccm.max_h)))
          {
             // center dialog at min size
             w = ec->icccm.min_w;
             h = ec->icccm.min_h;
             if (w > (ec->zone->w)) w = ec->zone->w;
             if (h > (ec->zone->h - kh)) h = (ec->zone->h - kh);
             x = ec->zone->x + ((ec->zone->w - w) / 2);
             y = ec->zone->y + ((ec->zone->h - kh - h) / 2);
          }
        else
          {
#warning X ONLY! SPANK! SPANK! SPANK!!!
             if (e_comp_data->illume.conformant.conformant)
               {
                  if (kbd_on != want_kbd)
                    {
                       if (want_kbd)
                         ecore_x_e_illume_keyboard_geometry_set(e_client_util_win_get(ec),
                                                                kx, ky, kw, kh);
                       else
                         ecore_x_e_illume_keyboard_geometry_set(e_client_util_win_get(ec),
                                                                0, 0, 0, 0);
                    }
               }
             else
               {
                  // just make all windows fill the zone...
                  if (want_kbd)
                    {
                       w = ec->zone->w;
                       h = ec->zone->h - kh;
                       x = ec->zone->x;
                       y = ec->zone->y;
                    }
               }
          }
        
        // implement the positioning/sizing
        if ((ec->x != x) || (ec->y != y))
          {
             ec->placed = 1;
             ec->x = x;
             ec->y = y;
             ec->changes.pos = 1;
             EC_CHANGED(ec);
          }
        if ((ec->w != w) || (ec->h != h))
          {
             ec->w = w;
             ec->h = h;
             e_comp_object_frame_wh_unadjust(ec->frame, ec->w, ec->h, &ec->client.w, &ec->client.h);
             ec->changes.size = 1;
             EC_CHANGED(ec);
          }
     }
   
   kbd_on = want_kbd;
}
