#include "e_mod_main.h"

#define PASSWD_LEN                256

typedef enum
{
   LOKKER_STATE_DEFAULT,
   LOKKER_STATE_CHECKING,
   LOKKER_STATE_INVALID,
   LOKKER_STATE_LAST,
} Lokker_State;

typedef struct Lokker_Popup
{
   E_Zone *zone;
   Evas_Object *comp_object;
   Evas_Object *bg_object;
   Evas_Object *login_box;
} Lokker_Popup;

typedef struct Lokker_Data
{
   Eina_List           *elock_wnd_list;
   Eina_List           *handlers;
   Ecore_Event_Handler *move_handler;
   char                 passwd[PASSWD_LEN];
   int                  state;
   Eina_Bool            selected : 1;
} Lokker_Data;

static pid_t _auth_child_pid = -1;
static Ecore_Event_Handler *_auth_child_exit_handler = NULL;
static E_Zone *last_active_zone = NULL;

static Lokker_Data *edd = NULL;

static int
_zone_count_get(void)
{
   int num = 0;
   const Eina_List *l;
   E_Comp *comp;

   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     num += eina_list_count(comp->zones);

   return num;
}

static void
_lokker_state_set(int state)
{
   Eina_List *l;
   Lokker_Popup *lp;
   const char *signal_desklock, *text;
   if (!edd) return;

   edd->state = state;
   if (state == LOKKER_STATE_CHECKING)
     {
        signal_desklock = "e,state,checking";
        text = _("Authenticating...");
     }
   else if (state == LOKKER_STATE_INVALID)
     {
        signal_desklock = "e,state,invalid";
        text = _("The password you entered is invalid. Try again.");
     }
   else
     return;

   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     {
        edje_object_signal_emit(lp->login_box, signal_desklock, "e");
        edje_object_signal_emit(lp->bg_object, signal_desklock, "e");
        edje_object_part_text_set(lp->login_box, "e.text.title", text);
     }
}

static void
_text_passwd_update(void)
{
   int len, i;
   char passwd_hidden[PASSWD_LEN] = "", *pp;
   Lokker_Popup *lp;
   Eina_List *l;

   if (!edd) return;

   len = eina_unicode_utf8_get_len(edd->passwd);
   for (i = 0, pp = passwd_hidden; i < len; i++, pp++)
     *pp = '*';
   *pp = 0;

   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     if (lp->login_box)
       edje_object_part_text_set(lp->login_box, "e.text.password",
                                 passwd_hidden);
}

static void
_lokker_null(void)
{
   memset(edd->passwd, 0, sizeof(char) * PASSWD_LEN);
   /* break compiler optimization */
   if (edd->passwd[0] || edd->passwd[3])
     fprintf(stderr, "ACK!\n");
   _text_passwd_update();
}


static void
_lokker_select(void)
{
   Lokker_Popup *lp;
   Eina_List *l;
   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     if (lp->login_box)
       edje_object_signal_emit(lp->login_box, "e,state,selected", "e");
   edd->selected = EINA_TRUE;
}

static void
_lokker_unselect(void)
{
   Lokker_Popup *lp;
   Eina_List *l;
   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     if (lp->login_box)
       edje_object_signal_emit(lp->login_box, "e,state,unselected", "e");
   edd->selected = EINA_FALSE;
}

static void
_lokker_backspace(void)
{
   int len, val, pos;

   if (!edd) return;

   len = strlen(edd->passwd);
   if (len > 0)
     {
        pos = evas_string_char_prev_get(edd->passwd, len, &val);
        if ((pos < len) && (pos >= 0))
          {
             edd->passwd[pos] = 0;
             _text_passwd_update();
          }
     }
}

static void
_lokker_delete(void)
{
   _lokker_backspace();
}


static void
_text_login_box_add(Lokker_Popup *lp)
{
   int mw, mh;
   E_Zone *zone, *current_zone;
   int total_zone_num;
   Evas *evas;

   zone = lp->zone;
   last_active_zone = current_zone = e_util_zone_current_get(e_manager_current_get());
   total_zone_num = _zone_count_get();
   if (total_zone_num > 1)
     {
        if ((e_config->desklock_login_box_zone == -2) && (zone != current_zone))
          return;
        if ((e_config->desklock_login_box_zone > -1) && (e_config->desklock_login_box_zone != (int)eina_list_count(edd->elock_wnd_list)))
          return;
     }

   evas = evas_object_evas_get(lp->bg_object);
   lp->login_box = edje_object_add(evas);
   evas_object_name_set(lp->login_box, "desklock->login_box");
   e_theme_edje_object_set(lp->login_box,
                           "base/theme/desklock",
                           "e/desklock/login_box");
   edje_object_part_text_set(lp->login_box, "e.text.title",
                             _("Please enter your unlock password"));
   edje_object_size_min_calc(lp->login_box, &mw, &mh);
   if (edje_object_part_exists(lp->bg_object, "e.swallow.login_box"))
     {
        edje_extern_object_min_size_set(lp->login_box, mw, mh);
        edje_object_part_swallow(lp->bg_object, "e.swallow.login_box", lp->login_box);
     }
   else
     {
        evas_object_resize(lp->login_box, mw, mh);
        e_comp_object_util_center_on(lp->login_box, lp->comp_object);
        evas_object_show(lp->login_box);
        evas_object_layer_set(lp->login_box, E_LAYER_DESKLOCK);
        evas_object_stack_above(lp->login_box, lp->comp_object);
     }

   evas_object_clip_set(lp->login_box, lp->zone->bg_clip_object);
}

static void
_lokker_popup_add(E_Zone *zone)
{
   Lokker_Popup *lp;
   E_Config_Desklock_Background *cbg;
   Eina_Stringshare *bg;
   Evas *evas;

   lp = E_NEW(Lokker_Popup, 1);
   cbg = eina_list_nth(e_config->desklock_backgrounds, zone->num);
   bg = cbg ? cbg->file : NULL;

   lp->zone = zone;
   evas = e_comp_get(zone)->evas;
   evas_event_freeze(evas);
   lp->bg_object = edje_object_add(evas);
   evas_object_name_set(lp->bg_object, "desklock->bg_object");

   if ((!bg) || (!strcmp(bg, "theme_desklock_background")))
     {
        e_theme_edje_object_set(lp->bg_object,
                                "base/theme/desklock",
                                "e/desklock/background");
     }
   else if (!strcmp(bg, "theme_background"))
     {
        e_theme_edje_object_set(lp->bg_object,
                                "base/theme/backgrounds",
                                "e/desktop/background");
     }
   else
     {
        Eina_Stringshare *f;

        if (!strcmp(bg, "user_background"))
          f = e_desklock_user_wallpaper_get(zone);
        else
          f = bg;

        if (e_util_edje_collection_exists(f, "e/desklock/background"))
          {
             edje_object_file_set(lp->bg_object, f, "e/desklock/background");
          }
        else
          {
             if (!edje_object_file_set(lp->bg_object,
                                       f, "e/desktop/background"))
               {
                  edje_object_file_set(lp->bg_object,
                                       e_theme_edje_file_get("base/theme/desklock",
                                                             "e/desklock/background"),
                                       "e/desklock/background");
               }
          }
     }

   evas_object_move(lp->bg_object, zone->x, zone->y);
   evas_object_resize(lp->bg_object, zone->w, zone->h);
   evas_object_show(lp->bg_object);
   lp->comp_object = e_comp_object_util_add(lp->bg_object, 0);
   evas_object_layer_set(lp->comp_object, E_LAYER_DESKLOCK);
   evas_object_clip_set(lp->comp_object, lp->zone->bg_clip_object);

   _text_login_box_add(lp);

   evas_event_thaw(evas);

   edd->elock_wnd_list = eina_list_append(edd->elock_wnd_list, lp);
}

static void
_lokker_popup_free(Lokker_Popup *lp)
{
   if (!lp) return;

   evas_object_hide(lp->comp_object);
   evas_object_del(lp->comp_object);
   evas_object_del(lp->bg_object);
   evas_object_del(lp->login_box);

   free(lp);
}

static Eina_List *
_lokker_popup_find(E_Zone *zone)
{
   Eina_List *l;
   Lokker_Popup *lp;

   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     if (lp->zone == zone) return l;
   return NULL;
}

static Eina_Bool
_lokker_cb_mouse_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Lokker_Popup *lp;
   E_Zone *current_zone;
   Eina_List *l;

   current_zone = e_util_zone_current_get(e_manager_current_get());

   if (current_zone == last_active_zone)
     return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     {
        if (!lp) continue;

        if (lp->zone != current_zone)
          {
             if (lp->login_box) evas_object_hide(lp->login_box);
             continue;
          }
        if (lp->login_box)
          evas_object_show(lp->login_box);
        else
          _text_login_box_add(lp);
     }
   _text_passwd_update();
   last_active_zone = current_zone;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_lokker_cb_exit(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Del *ev = event;

   if (ev->pid != _auth_child_pid) return ECORE_CALLBACK_PASS_ON;

   _auth_child_pid = -1;
   /* ok */
   if (ev->exit_code == 0)
     {
        /* security - null out passwd string once we are done with it */
        _lokker_null();
        e_desklock_hide();
     }
   /* error */
   else if (ev->exit_code < 128)
     {
        /* security - null out passwd string once we are done with it */
        _lokker_null();
        e_desklock_hide();
        e_util_dialog_show(_("Authentication System Error"),
                           _("Authentication via PAM had errors setting up the<br>"
                             "authentication session. The error code was <hilight>%i</hilight>.<br>"
                             "This is bad and should not be happening. Please report this bug.")
                           , ev->exit_code);
     }
   /* failed auth */
   else
     {
        _lokker_state_set(LOKKER_STATE_INVALID);
        /* security - null out passwd string once we are done with it */
        _lokker_null();
     }
   E_FREE_FUNC(_auth_child_exit_handler, ecore_event_handler_del);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_lokker_cb_zone_add(void *data EINA_UNUSED,
                        int type EINA_UNUSED,
                        void *event)
{
   E_Event_Zone_Add *ev = event;
   if (!edd) return ECORE_CALLBACK_PASS_ON;
   if ((!edd->move_handler) && (e_config->desklock_login_box_zone == -2))
     edd->move_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, _lokker_cb_mouse_move, NULL);
   if (!_lokker_popup_find(ev->zone)) _lokker_popup_add(ev->zone);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_lokker_cb_zone_del(void *data EINA_UNUSED,
                        int type EINA_UNUSED,
                        void *event)
{
   E_Event_Zone_Del *ev = event;
   Eina_List *l;
   if (!edd) return ECORE_CALLBACK_PASS_ON;
   if ((eina_list_count(e_util_comp_current_get()->zones) == 1) && (e_config->desklock_login_box_zone == -2))
     edd->move_handler = ecore_event_handler_del(edd->move_handler);

   l = _lokker_popup_find(ev->zone);
   if (l)
     {
        _lokker_popup_free(l->data);
        edd->elock_wnd_list = eina_list_remove_list(edd->elock_wnd_list, l);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_lokker_cb_zone_move_resize(void *data EINA_UNUSED,
                                int type EINA_UNUSED,
                                void *event)
{
   Lokker_Popup *lp;
   Eina_List *l;
   E_Event_Zone_Move_Resize *ev = event;

   if (!edd) return ECORE_CALLBACK_PASS_ON;
   EINA_LIST_FOREACH(edd->elock_wnd_list, l, lp)
     if (lp->zone == ev->zone)
       {
          evas_object_resize(lp->bg_object, ev->zone->w, ev->zone->h);
          e_comp_object_util_center_on(lp->login_box, lp->comp_object);
          break;
       }
   return ECORE_CALLBACK_PASS_ON;
}

static int
_desklock_auth(char *passwd)
{
   _lokker_state_set(LOKKER_STATE_CHECKING);
   _auth_child_pid = e_auth_begin(passwd);
   if (_auth_child_pid > 0)
     /* parent */
     _auth_child_exit_handler =
       ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _lokker_cb_exit, NULL);
   else
     _lokker_state_set(LOKKER_STATE_INVALID);
   return (_auth_child_pid > 0);
}

static int
_lokker_check_auth(void)
{
   if (!edd) return 0;
#ifdef HAVE_PAM
   if (e_config->desklock_auth_method == 0)
     {
        int ret;

        ret = _desklock_auth(edd->passwd);
        // passwd off in child proc now - null out from parent
        _lokker_null();
        return ret;
     }
   else if (e_config->desklock_auth_method == 1)
     {
#endif
   if ((e_config->desklock_passwd) && (edd->passwd && edd->passwd[0]) &&
       (e_config->desklock_passwd == eina_hash_djb2(edd->passwd, strlen(edd->passwd))))
     {
        /* password ok */
        /* security - null out passwd string once we are done with it */
        _lokker_null();
        e_desklock_hide();
        return 1;
     }
#ifdef HAVE_PAM
}

#endif
   /* password is definitely wrong */
   _lokker_state_set(LOKKER_STATE_INVALID);
   _lokker_null();
   return 0;
}

static Eina_Bool
_lokker_cb_key_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev = event;

   if (edd->state == LOKKER_STATE_CHECKING) return ECORE_CALLBACK_DONE;

   if (!strcmp(ev->key, "Escape"))
     {
        if (edd->selected)
          {
             _lokker_unselect();
             return ECORE_CALLBACK_RENEW;
          }
     }
   else if (!strcmp(ev->key, "KP_Enter"))
     _lokker_check_auth();
   else if (!strcmp(ev->key, "Return"))
     _lokker_check_auth();
   else if (!strcmp(ev->key, "BackSpace"))
     {
        if (edd->selected)
          {
             _lokker_null();
             _lokker_unselect();
             return ECORE_CALLBACK_RENEW;
          }
        _lokker_backspace();
     }
   else if (!strcmp(ev->key, "Delete"))
     {
        if (edd->selected)
          {
             _lokker_null();
             _lokker_unselect();
             return ECORE_CALLBACK_RENEW;
          }
        _lokker_delete();
     }
   else if ((!strcmp(ev->key, "u") &&
             (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)))
     _lokker_null();
   else if ((!strcmp(ev->key, "a") &&
             (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)))
     _lokker_select();
   else
     {
        /* here we have to grab a password */
        if (ev->compose)
          {
             if (edd->selected)
               {
                  _lokker_null();
                  _lokker_unselect();
               }
             if ((strlen(edd->passwd) < (PASSWD_LEN - strlen(ev->compose))))
               {
                  strcat(edd->passwd, ev->compose);
                  _text_passwd_update();
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

EINTERN Eina_Bool
lokker_lock(void)
{
   int total_zone_num = 0;
   const Eina_List *l;
   E_Comp *comp;

   if (edd) return EINA_TRUE;
   edd = E_NEW(Lokker_Data, 1);
   if (!edd) return EINA_FALSE;

   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     {
        E_LIST_FOREACH(comp->zones, _lokker_popup_add);
        total_zone_num += eina_list_count(comp->zones);
     }

   /* handlers */
   E_LIST_HANDLER_APPEND(edd->handlers, ECORE_EVENT_KEY_DOWN, _lokker_cb_key_down, NULL);
   E_LIST_HANDLER_APPEND(edd->handlers, E_EVENT_ZONE_ADD, _lokker_cb_zone_add, NULL);
   E_LIST_HANDLER_APPEND(edd->handlers, E_EVENT_ZONE_DEL, _lokker_cb_zone_del, NULL);
   E_LIST_HANDLER_APPEND(edd->handlers, E_EVENT_ZONE_MOVE_RESIZE, _lokker_cb_zone_move_resize, NULL);

   if ((total_zone_num > 1) && (e_config->desklock_login_box_zone == -2))
     edd->move_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, _lokker_cb_mouse_move, NULL);

   _text_passwd_update();
   return EINA_TRUE;
}

EINTERN void
lokker_unlock(void)
{
   E_FREE_LIST(edd->elock_wnd_list, _lokker_popup_free);
   E_FREE_LIST(edd->handlers, ecore_event_handler_del);
   if (edd->move_handler) ecore_event_handler_del(edd->move_handler);

   E_FREE(edd);
}
