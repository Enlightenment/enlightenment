#include "e_mod_main.h"

static Eina_Bool _cb_event_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event);
static Eina_Bool _cb_event_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event);
static void _cb_hook_post_fetch(void *data __UNUSED__, void *data2);
static void _cb_hook_post_assign(void *data __UNUSED__, void *data2);
static void _cb_hook_layout(void *data __UNUSED__, void *data2);

static Eina_List *hooks = NULL;
static Eina_List *handlers = NULL;

static Eina_Bool kbd_on = EINA_FALSE;
static Eina_Bool kbd_override = EINA_FALSE;

#define LADD(l, f) l = eina_list_append(l, f)

void
e_policy_init(void)
{
   LADD(hooks, e_border_hook_add(E_BORDER_HOOK_EVAL_POST_FETCH,
                                 _cb_hook_post_fetch, NULL));
   LADD(hooks, e_border_hook_add(E_BORDER_HOOK_EVAL_POST_BORDER_ASSIGN,
                                 _cb_hook_post_assign, NULL));
   LADD(hooks, e_border_hook_add(E_BORDER_HOOK_CONTAINER_LAYOUT,
                                 _cb_hook_layout, NULL));
   LADD(handlers, ecore_event_handler_add(E_EVENT_BORDER_FOCUS_IN,
                                          _cb_event_focus_in, NULL));
   LADD(handlers, ecore_event_handler_add(E_EVENT_BORDER_FOCUS_OUT,
                                          _cb_event_focus_out, NULL));
}

void
e_policy_shutdown(void)
{
   E_Border_Hook *bh;
   Ecore_Event_Handler *eh;
   
   EINA_LIST_FREE(hooks, bh) e_border_hook_del(bh);
   EINA_LIST_FREE(handlers, eh) ecore_event_handler_del(eh);
}

void
e_policy_kbd_override_set(Eina_Bool override)
{
   Eina_List *l;
   E_Border *bd, *kbd = NULL;;
   
   if (kbd_override == override) return;
   kbd_override = override;
   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        if (bd->client.vkbd.vkbd)
          {
             kbd = bd;
          }
     }
   if (kbd)
     {
        bd = kbd;
        e_border_uniconify(bd);
        e_border_raise(bd);
        e_border_show(bd);
     }
}

static Eina_Bool
_cb_event_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Border *bd = event;
   
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_event_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   E_Border *bd = event;

   if (kbd_on) e_policy_kbd_override_set(EINA_FALSE);
   return ECORE_CALLBACK_PASS_ON;
}

static void
_cb_hook_post_fetch(void *data __UNUSED__, void *data2)
{
   E_Border *bd = data2;
   
   if (!bd) return;
   /* NB: for this policy we disable all remembers set on a border */
   if (bd->remember) e_remember_del(bd->remember);
   bd->remember = NULL;
   
   /* set this border to borderless */
   bd->borderless = 1;
   bd->client.border.changed = 1;
}

static void
_cb_hook_post_assign(void *data __UNUSED__, void *data2)
{
   E_Border *bd = data2;

   if (!bd) return;

   bd->internal_no_remember = 1;

   /* do not allow client to change these properties */
   bd->lock_client_size = 1;
   bd->lock_client_shade = 1;
   bd->lock_client_maximize = 1;
   bd->lock_client_location = 1;
   bd->lock_client_stacking = 1;

   /* do not allow the user to change these properties */
   bd->lock_user_location = 1;
   bd->lock_user_size = 1;
   bd->lock_user_shade = 1;

   /* clear any centered states */
   /* NB: this is mainly needed for E's main config dialog */
   bd->client.e.state.centered = 0;

   /* lock the border type so user/client cannot change */
   bd->lock_border = 1;
}

static void
_cb_hook_layout(void *data __UNUSED__, void *data2)
{
   E_Container *con = data2;
   Eina_List *l;
   E_Border *bd, *kbd = NULL;;
   Eina_Bool want_kbd = EINA_FALSE;
   Eina_Bool have_focused = EINA_FALSE;
   int kx = 0, ky = 0, kw = 0, kh = 0;
   
   if (!con) return;
   
   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        if (bd->focused) have_focused = EINA_TRUE;
        if ((bd->focused) &&
            (bd->client.vkbd.state > ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF))
          want_kbd = EINA_TRUE;
        if (bd->client.vkbd.vkbd) kbd = bd;
     }
   
   if ((have_focused) && (kbd_override)) want_kbd = EINA_TRUE;
   
   if (kbd)
     {
        kw = kbd->zone->w;
        kh = kbd->client.icccm.min_h;
        kx = kbd->zone->x;
        ky = kbd->zone->y + kbd->zone->h - kh;
     }
   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        int x, y, w, h;
        
        if (!bd->zone) continue;
        
        w = bd->zone->w;
        h = bd->zone->h;
        x = bd->zone->x;
        y = bd->zone->y;

        if (bd->client.vkbd.vkbd)
          {
             x = kx; y = ky; w = kw; h = kh;
             if (want_kbd)
               {
                  e_border_uniconify(bd);
                  e_border_raise(bd);
                  e_border_show(bd);
               }
             else
               {
                  e_border_iconify(bd);
               }
          }
        else if (((bd->client.netwm.type == ECORE_X_WINDOW_TYPE_DIALOG) ||
                  (bd->client.icccm.transient_for != 0)) &&
                 ((bd->client.icccm.min_w == bd->client.icccm.max_w) &&
                  (bd->client.icccm.min_h == bd->client.icccm.max_h)))
          {
             // center dialog at min size
             w = bd->client.icccm.min_w;
             h = bd->client.icccm.min_h;
             if (w > (bd->zone->w)) w = bd->zone->w;
             if (h > (bd->zone->h - kh)) h = (bd->zone->h - kh);
             x = bd->zone->x + ((bd->zone->w - w) / 2);
             y = bd->zone->y + ((bd->zone->h - kh - h) / 2);
          }
        else
          {
             if (bd->client.illume.conformant.conformant)
               {
                  if (kbd_on != want_kbd)
                    {
                       if (want_kbd)
                         ecore_x_e_illume_keyboard_geometry_set(bd->client.win,
                                                                kx, ky, kw, kh);
                       else
                         ecore_x_e_illume_keyboard_geometry_set(bd->client.win,
                                                                0, 0, 0, 0);
                    }
               }
             else
               {
                  // just make all windows fill the zone...
                  if (want_kbd)
                    {
                       w = bd->zone->w;
                       h = bd->zone->h - kh;
                       x = bd->zone->x;
                       y = bd->zone->y;
                    }
               }
          }
        
        // implement the positioning/sizing
        if ((bd->x != x) || (bd->y != y))
          {
             bd->placed = 1;
             bd->x = x;
             bd->y = y;
             bd->changes.pos = 1;
             bd->changed = 1;
          }
        if ((bd->w != w) || (bd->h != h))
          {
             bd->w = w;
             bd->h = h;
             bd->client.w = (bd->w - (bd->client_inset.l + bd->client_inset.r));
             bd->client.h = (bd->h - (bd->client_inset.t + bd->client_inset.b));
             bd->changes.size = 1;
             bd->changed = 1;
          }
     }
   
   kbd_on = want_kbd;
}
