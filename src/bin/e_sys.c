#include "e.h"

#define ACTION_TIMEOUT 30.0

/* local subsystem functions */
static Eina_Bool _e_sys_cb_timer(void *data);
static Eina_Bool _e_sys_cb_exit(void *data, int type, void *event);
static void      _e_sys_cb_logout_logout(void *data, E_Dialog *dia);
static void      _e_sys_cb_logout_wait(void *data, E_Dialog *dia);
static void      _e_sys_cb_logout_abort(void *data, E_Dialog *dia);
static Eina_Bool _e_sys_cb_logout_timer(void *data);
static void      _e_sys_logout_after(void);
static void      _e_sys_logout_begin(E_Sys_Action a_after, Eina_Bool raw);
static void      _e_sys_current_action(void);
static void      _e_sys_action_failed(void);
static int       _e_sys_action_do(E_Sys_Action a, char *param, Eina_Bool raw);
static void      _e_sys_dialog_cb_delete(E_Obj_Dialog *od);

static Ecore_Event_Handler *_e_sys_exe_exit_handler = NULL;
static Ecore_Exe *_e_sys_halt_check_exe = NULL;
static Ecore_Exe *_e_sys_reboot_check_exe = NULL;
static Ecore_Exe *_e_sys_suspend_check_exe = NULL;
static Ecore_Exe *_e_sys_hibernate_check_exe = NULL;
static int _e_sys_can_halt = 0;
static int _e_sys_can_reboot = 0;
static int _e_sys_can_suspend = 0;
static int _e_sys_can_hibernate = 0;

static E_Sys_Action _e_sys_action_current = E_SYS_NONE;
static E_Sys_Action _e_sys_action_after = E_SYS_NONE;
static Eina_Bool _e_sys_action_after_raw = EINA_FALSE;
static Ecore_Exe *_e_sys_exe = NULL;
static double _e_sys_begin_time = 0.0;
static double _e_sys_logout_begin_time = 0.0;
static Ecore_Timer *_e_sys_logout_timer = NULL;
static E_Obj_Dialog *_e_sys_dialog = NULL;
static E_Dialog *_e_sys_logout_confirm_dialog = NULL;
static Ecore_Timer *_e_sys_susp_hib_check_timer = NULL;
static double _e_sys_susp_hib_check_last_tick = 0.0;

static void _e_sys_systemd_handle_inhibit(void);
static void _e_sys_systemd_poweroff(void);
static void _e_sys_systemd_reboot(void);
static void _e_sys_systemd_suspend(void);
static void _e_sys_systemd_hibernate(void);
static void _e_sys_systemd_exists_cb(void *data, const Eldbus_Message *m, Eldbus_Pending *p);

static Eina_Bool systemd_works = EINA_FALSE;
static int _e_sys_systemd_inhibit_fd = -1;

static const int E_LOGOUT_AUTO_TIME = 60;
static const int E_LOGOUT_WAIT_TIME = 15;

static Ecore_Timer *action_timeout = NULL;

static Eldbus_Proxy *login1_manger_proxy = NULL;

EAPI int E_EVENT_SYS_SUSPEND = -1;
EAPI int E_EVENT_SYS_HIBERNATE = -1;
EAPI int E_EVENT_SYS_RESUME = -1;

static void
_e_sys_comp_done_cb(void *data, Evas_Object *obj, const char *sig, const char *src)
{
   edje_object_signal_callback_del(obj, sig, src, _e_sys_comp_done_cb);
   e_sys_action_raw_do((E_Sys_Action)(long)data, NULL);
   E_FREE_FUNC(action_timeout, ecore_timer_del);
}

static Eina_Bool
_e_sys_comp_action_timeout(void *data)
{
   const Eina_List *l;
   E_Zone *zone;
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
   E_FREE_FUNC(action_timeout, ecore_timer_del);
   if (sig)
     {
        EINA_LIST_FOREACH(e_comp->zones, l, zone)
          edje_object_signal_callback_del(zone->over, sig, "e", _e_sys_comp_done_cb);
     }
   e_sys_action_raw_do(a, NULL);
   return EINA_FALSE;
}

static void
_e_sys_comp_emit_cb_wait(E_Sys_Action a, const char *sig, const char *rep, Eina_Bool nocomp_push)
{
   const Eina_List *l;
   E_Zone *zone;
   Eina_Bool first = EINA_TRUE;

   if (nocomp_push) e_comp_override_add();
   else e_comp_override_timed_pop();
   printf("_e_sys_comp_emit_cb_wait - [%x] %s %s\n", a, sig, rep);
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        e_zone_fade_handle(zone, nocomp_push, 0.5);
        edje_object_signal_emit(zone->base, sig, "e");
        edje_object_signal_emit(zone->over, sig, "e");
        if ((rep) && (first))
          edje_object_signal_callback_add(zone->over, rep, "e", _e_sys_comp_done_cb, (void *)(long)a);
        first = EINA_FALSE;
     }
   if (rep)
     {
        if (action_timeout) ecore_timer_del(action_timeout);
        action_timeout = ecore_timer_add(ACTION_TIMEOUT, (Ecore_Task_Cb)_e_sys_comp_action_timeout, (intptr_t*)(long)a);
     }
}

static void
_e_sys_comp_suspend(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_SUSPEND, "e,state,sys,suspend", "e,state,sys,suspend,done", EINA_TRUE);
}

static void
_e_sys_comp_hibernate(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_HIBERNATE, "e,state,sys,hibernate", "e,state,sys,hibernate,done", EINA_TRUE);
}

static void
_e_sys_comp_reboot(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_REBOOT, "e,state,sys,reboot", "e,state,sys,reboot,done", EINA_TRUE);
}

static void
_e_sys_comp_shutdown(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_HALT, "e,state,sys,halt", "e,state,sys,halt,done", EINA_TRUE);
}

static void
_e_sys_comp_logout(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_LOGOUT, "e,state,sys,logout", "e,state,sys,logout,done", EINA_TRUE);
}

static void
_e_sys_comp_resume(void)
{
   evas_damage_rectangle_add(e_comp->evas, 0, 0, e_comp->man->w, e_comp->man->h);
   _e_sys_comp_emit_cb_wait(E_SYS_SUSPEND, "e,state,sys,resume", NULL, EINA_FALSE);
   e_screensaver_deactivate();
}

/* externally accessible functions */
EINTERN int
e_sys_init(void)
{
   Eldbus_Connection *conn;
   Eldbus_Object *obj;

   eldbus_init();
   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   obj = eldbus_object_get(conn, "org.freedesktop.login1",
                           "/org/freedesktop/login1");
   login1_manger_proxy = eldbus_proxy_get(obj,
                                          "org.freedesktop.login1.Manager");
   eldbus_name_owner_get(conn, "org.freedesktop.login1",
                         _e_sys_systemd_exists_cb, NULL);
   _e_sys_systemd_handle_inhibit();
   
   E_EVENT_SYS_SUSPEND = ecore_event_type_new();
   E_EVENT_SYS_HIBERNATE = ecore_event_type_new();
   E_EVENT_SYS_RESUME = ecore_event_type_new();
   /* this is not optimal - but it does work cleanly */
   _e_sys_exe_exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                                     _e_sys_cb_exit, NULL);
   return 1;
}

EINTERN int
e_sys_shutdown(void)
{
   if (_e_sys_exe_exit_handler)
     ecore_event_handler_del(_e_sys_exe_exit_handler);
   _e_sys_exe_exit_handler = NULL;
   _e_sys_halt_check_exe = NULL;
   _e_sys_reboot_check_exe = NULL;
   _e_sys_suspend_check_exe = NULL;
   _e_sys_hibernate_check_exe = NULL;
   if (login1_manger_proxy)
     {
         Eldbus_Connection *conn;
         Eldbus_Object *obj;

         obj = eldbus_proxy_object_get(login1_manger_proxy);
         conn = eldbus_object_connection_get(obj);
         eldbus_proxy_unref(login1_manger_proxy);
         eldbus_object_unref(obj);
         eldbus_connection_unref(conn);
         login1_manger_proxy = NULL;
     }
   if (_e_sys_systemd_inhibit_fd >= 0)
     {
        close(_e_sys_systemd_inhibit_fd);
        _e_sys_systemd_inhibit_fd = -1;
     }
   eldbus_shutdown();
   return 1;
}

EAPI int
e_sys_action_possible_get(E_Sys_Action a)
{
   switch (a)
     {
      case E_SYS_EXIT:
      case E_SYS_RESTART:
      case E_SYS_EXIT_NOW:
        return 1;

      case E_SYS_LOGOUT:
        return 1;

      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        return _e_sys_can_halt;

      case E_SYS_REBOOT:
        return _e_sys_can_reboot;

      case E_SYS_SUSPEND:
        return _e_sys_can_suspend;

      case E_SYS_HIBERNATE:
        return _e_sys_can_hibernate;

      default:
        return 0;
     }
   return 0;
}

EAPI int
e_sys_action_do(E_Sys_Action a, char *param)
{
   int ret = 0;

   if (_e_sys_action_current != E_SYS_NONE)
     {
        _e_sys_current_action();
        return 0;
     }
   e_config_save_flush();
   switch (a)
     {
      case E_SYS_EXIT:
      case E_SYS_RESTART:
      case E_SYS_EXIT_NOW:
      case E_SYS_LOGOUT:
      case E_SYS_SUSPEND:
      case E_SYS_HIBERNATE:
      case E_SYS_HALT_NOW:
        ret = _e_sys_action_do(a, param, EINA_FALSE);
        break;

      case E_SYS_HALT:
      case E_SYS_REBOOT:
        if (!e_util_immortal_check())
          ret = _e_sys_action_do(a, param, EINA_FALSE);
        break;

      default:
        break;
     }

   if (ret) _e_sys_action_current = a;
   else _e_sys_action_current = E_SYS_NONE;

   return ret;
}

EAPI int
e_sys_action_raw_do(E_Sys_Action a, char *param)
{
   int ret = 0;

   e_config_save_flush();
   if (_e_sys_action_current != E_SYS_NONE)
     {
        _e_sys_current_action();
        return 0;
     }
   ret = _e_sys_action_do(a, param, EINA_TRUE);

   if (ret) _e_sys_action_current = a;
   else _e_sys_action_current = E_SYS_NONE;

   return ret;
}

static Eina_List *extra_actions = NULL;

EAPI E_Sys_Con_Action *
e_sys_con_extra_action_register(const char *label,
                                const char *icon_group,
                                const char *button_name,
                                void (*func)(void *data),
                                const void *data)
{
   E_Sys_Con_Action *sca;

   sca = E_NEW(E_Sys_Con_Action, 1);
   if (label)
     sca->label = eina_stringshare_add(label);
   if (icon_group)
     sca->icon_group = eina_stringshare_add(icon_group);
   if (button_name)
     sca->button_name = eina_stringshare_add(button_name);
   sca->func = func;
   sca->data = data;
   extra_actions = eina_list_append(extra_actions, sca);
   return sca;
}

EAPI void
e_sys_con_extra_action_unregister(E_Sys_Con_Action *sca)
{
   extra_actions = eina_list_remove(extra_actions, sca);
   if (sca->label) eina_stringshare_del(sca->label);
   if (sca->icon_group) eina_stringshare_del(sca->icon_group);
   if (sca->button_name) eina_stringshare_del(sca->button_name);
   free(sca);
}

EAPI const Eina_List *
e_sys_con_extra_action_list_get(void)
{
   return extra_actions;
}

static void
_e_sys_systemd_inhibit_cb(void *data __UNUSED__, const Eldbus_Message *m, Eldbus_Pending *p __UNUSED__)
{
   int fd = -1;
   if (eldbus_message_error_get(m, NULL, NULL)) return;
   if (!eldbus_message_arguments_get(m, "h", &fd))
     _e_sys_systemd_inhibit_fd = fd;
}

static void
_e_sys_systemd_handle_inhibit(void)
{
   Eldbus_Message *m;
   
   if (!login1_manger_proxy) return;
   if (!(m = eldbus_proxy_method_call_new(login1_manger_proxy, "Inhibit")))
     return;
   eldbus_message_arguments_append
     (m, "ssss",
         "handle-power-key:"
         "handle-suspend-key:"
         "handle-hibernate-key:"
         "handle-lid-switch", // what
         "Enlightenment", // who (string)
         "Normal Execution", // why (string)
         "block");
   eldbus_proxy_send(login1_manger_proxy, m, _e_sys_systemd_inhibit_cb, NULL, -1);
}

static void
_e_sys_systemd_check_cb(void *data, const Eldbus_Message *m, Eldbus_Pending *p __UNUSED__)
{
   int *dest = data;
   char *s = NULL;
   if (!eldbus_message_arguments_get(m, "s", &s)) return;
   if (!s) return;
   if (!strcmp(s, "yes")) *dest = 1;
   else *dest = 0;
}

static void
_e_sys_systemd_check(void)
{
   if (!login1_manger_proxy) return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanPowerOff",
                          _e_sys_systemd_check_cb, &_e_sys_can_halt, -1, ""))
     return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanReboot",
                          _e_sys_systemd_check_cb, &_e_sys_can_reboot, -1, ""))
     return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanSuspend",
                          _e_sys_systemd_check_cb, &_e_sys_can_suspend, -1, ""))
     return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanHibernate",
                          _e_sys_systemd_check_cb, &_e_sys_can_hibernate, -1, ""))
     return;
}

static void
_e_sys_systemd_exists_cb(void *data __UNUSED__, const Eldbus_Message *m, Eldbus_Pending *p __UNUSED__)
{
   const char *id = NULL;
   
   if (eldbus_message_error_get(m, NULL, NULL)) goto fail;
   if (!eldbus_message_arguments_get(m, "s", &id)) goto fail;
   if ((!id) || (id[0] != ':')) goto fail;
   systemd_works = EINA_TRUE;
   _e_sys_systemd_check();
   return;
fail:
   systemd_works = EINA_FALSE;
   /* delay this for 1.0 seconds while the rest of e starts up */
   ecore_timer_add(1.0, _e_sys_cb_timer, NULL);
}

static void
_e_sys_systemd_poweroff(void)
{
   eldbus_proxy_call(login1_manger_proxy, "PowerOff", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_systemd_reboot(void)
{
   eldbus_proxy_call(login1_manger_proxy, "Reboot", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_systemd_suspend(void)
{
   eldbus_proxy_call(login1_manger_proxy, "Suspend", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_systemd_hibernate(void)
{
   eldbus_proxy_call(login1_manger_proxy, "Hibernate", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_resume_job(void *d EINA_UNUSED)
{
   ecore_event_add(E_EVENT_SYS_RESUME, NULL, NULL, NULL);
   _e_sys_comp_resume();
}

static Eina_Bool
_e_sys_susp_hib_check_timer_cb(void *data __UNUSED__)
{
   double t = ecore_time_unix_get();

   if ((t - _e_sys_susp_hib_check_last_tick) > 0.2)
     {
        _e_sys_susp_hib_check_timer = NULL;
        if (_e_sys_dialog)
          {
             e_object_del(E_OBJECT(_e_sys_dialog));
             _e_sys_dialog = NULL;
          }
        ecore_job_add(_e_sys_resume_job, NULL);
        return EINA_FALSE;
     }
   _e_sys_susp_hib_check_last_tick = t;
   return EINA_TRUE;
}

static void
_e_sys_susp_hib_check(void)
{
   if (_e_sys_susp_hib_check_timer)
     ecore_timer_del(_e_sys_susp_hib_check_timer);
   _e_sys_susp_hib_check_last_tick = ecore_time_unix_get();
   _e_sys_susp_hib_check_timer =
     ecore_timer_add(0.1, _e_sys_susp_hib_check_timer_cb, NULL);
}

/* local subsystem functions */
static Eina_Bool
_e_sys_cb_timer(void *data __UNUSED__)
{
   /* exec out sys helper and ask it to test if we are allowed to do these
    * things
    */
   char buf[4096];

   e_init_status_set(_("Checking System Permissions"));
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t halt",
            e_prefix_lib_get());
   _e_sys_halt_check_exe = ecore_exe_run(buf, NULL);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t reboot",
            e_prefix_lib_get());
   _e_sys_reboot_check_exe = ecore_exe_run(buf, NULL);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t suspend",
            e_prefix_lib_get());
   _e_sys_suspend_check_exe = ecore_exe_run(buf, NULL);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t hibernate",
            e_prefix_lib_get());
   _e_sys_hibernate_check_exe = ecore_exe_run(buf, NULL);
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_sys_cb_exit(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Exe_Event_Del *ev;

   ev = event;
   if ((_e_sys_exe) && (ev->exe == _e_sys_exe))
     {
        if (ev->exit_code != 0) _e_sys_action_failed();
        if (((_e_sys_action_current != E_SYS_HALT) &&
             (_e_sys_action_current != E_SYS_HALT_NOW) &&
             (_e_sys_action_current != E_SYS_REBOOT)) ||
            (ev->exit_code != 0))
          {
             if (_e_sys_dialog)
               {
                  e_object_del(E_OBJECT(_e_sys_dialog));
                  _e_sys_dialog = NULL;
               }
          }
        _e_sys_action_current = E_SYS_NONE;
        _e_sys_exe = NULL;
        return ECORE_CALLBACK_RENEW;
     }
   if ((_e_sys_halt_check_exe) && (ev->exe == _e_sys_halt_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        /* exit_code: 0 == OK, 5 == suid root removed, 7 == group id error
         * 10 == permission denied, 20 == action undefined */
        if (ev->exit_code == 0)
          {
             _e_sys_can_halt = 1;
             _e_sys_halt_check_exe = NULL;
          }
     }
   else if ((_e_sys_reboot_check_exe) && (ev->exe == _e_sys_reboot_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        if (ev->exit_code == 0)
          {
             _e_sys_can_reboot = 1;
             _e_sys_reboot_check_exe = NULL;
          }
     }
   else if ((_e_sys_suspend_check_exe) && (ev->exe == _e_sys_suspend_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        if (ev->exit_code == 0)
          {
             _e_sys_can_suspend = 1;
             _e_sys_suspend_check_exe = NULL;
          }
     }
   else if ((_e_sys_hibernate_check_exe) && (ev->exe == _e_sys_hibernate_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        if (ev->exit_code == 0)
          {
             _e_sys_can_hibernate = 1;
             _e_sys_hibernate_check_exe = NULL;
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_sys_cb_logout_logout(void *data __UNUSED__, E_Dialog *dia)
{
   if (_e_sys_logout_timer)
     {
        ecore_timer_del(_e_sys_logout_timer);
        _e_sys_logout_timer = NULL;
     }
   _e_sys_logout_begin_time = 0.0;
   _e_sys_logout_after();
   e_object_del(E_OBJECT(dia));
   _e_sys_logout_confirm_dialog = NULL;
}

static void
_e_sys_cb_logout_wait(void *data __UNUSED__, E_Dialog *dia)
{
   if (_e_sys_logout_timer) ecore_timer_del(_e_sys_logout_timer);
   _e_sys_logout_timer = ecore_timer_add(0.5, _e_sys_cb_logout_timer, NULL);
   _e_sys_logout_begin_time = ecore_time_get();
   e_object_del(E_OBJECT(dia));
   _e_sys_logout_confirm_dialog = NULL;
}

static void
_e_sys_cb_logout_abort(void *data __UNUSED__, E_Dialog *dia)
{
   if (_e_sys_logout_timer)
     {
        ecore_timer_del(_e_sys_logout_timer);
        _e_sys_logout_timer = NULL;
     }
   _e_sys_logout_begin_time = 0.0;
   e_object_del(E_OBJECT(dia));
   _e_sys_logout_confirm_dialog = NULL;
   _e_sys_action_current = E_SYS_NONE;
   _e_sys_action_after = E_SYS_NONE;
   _e_sys_action_after_raw = EINA_FALSE;
   if (_e_sys_dialog)
     {
        e_object_del(E_OBJECT(_e_sys_dialog));
        _e_sys_dialog = NULL;
     }
}

static void
_e_sys_logout_confirm_dialog_update(int remaining)
{
   char txt[4096];

   if (!_e_sys_logout_confirm_dialog)
     {
        fputs("ERROR: updating logout confirm dialog, but none exists!\n",
              stderr);
        return;
     }

   snprintf(txt, sizeof(txt),
            _("Logout is taking too long.<br>"
              "Some applications refuse to close.<br>"
              "Do you want to finish the logout<br>"
              "anyway without closing these<br>"
              "applications first?<br><br>"
              "Auto logout in %d seconds."), remaining);

   e_dialog_text_set(_e_sys_logout_confirm_dialog, txt);
}

static Eina_Bool
_e_sys_cb_logout_timer(void *data __UNUSED__)
{
   E_Client *ec;
   int pending = 0;

   E_CLIENT_FOREACH(ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        if (!ec->internal) pending++;
     }
   if (pending == 0) goto after;
   else if (_e_sys_logout_confirm_dialog)
     {
        int remaining = E_LOGOUT_AUTO_TIME -
          round(ecore_loop_time_get() - _e_sys_logout_begin_time);
        /* it has taken 60 (E_LOGOUT_AUTO_TIME) seconds of waiting the
         * confirm dialog and we still have apps that will not go
         * away. Do the action as user may be far away or forgot it.
         *
         * NOTE: this is the behavior for many operating systems and I
         *       guess the reason is people that hit "shutdown" and
         *       put their laptops in their backpacks in the hope
         *       everything will be turned off properly.
         */
        if (remaining > 0)
          {
             _e_sys_logout_confirm_dialog_update(remaining);
             return ECORE_CALLBACK_RENEW;
          }
        else
          {
             _e_sys_cb_logout_logout(NULL, _e_sys_logout_confirm_dialog);
             return ECORE_CALLBACK_CANCEL;
          }
     }
   else
     {
        /* it has taken 15 seconds of waiting and we still have apps that
         * will not go away
         */
        double now = ecore_loop_time_get();
        if ((now - _e_sys_logout_begin_time) > E_LOGOUT_WAIT_TIME)
          {
             E_Dialog *dia;

             dia = e_dialog_new(NULL, "E", "_sys_error_logout_slow");
             if (dia)
               {
                  _e_sys_logout_confirm_dialog = dia;
                  e_dialog_title_set(dia, _("Logout problems"));
                  e_dialog_icon_set(dia, "system-log-out", 64);
                  e_dialog_button_add(dia, _("Logout now"), NULL,
                                      _e_sys_cb_logout_logout, NULL);
                  e_dialog_button_add(dia, _("Wait longer"), NULL,
                                      _e_sys_cb_logout_wait, NULL);
                  e_dialog_button_add(dia, _("Cancel Logout"), NULL,
                                      _e_sys_cb_logout_abort, NULL);
                  e_dialog_button_focus_num(dia, 1);
                  _e_sys_logout_confirm_dialog_update(E_LOGOUT_AUTO_TIME);
                  elm_win_center(dia->win, 1, 1);
                  e_dialog_show(dia);
                  _e_sys_logout_begin_time = now;
               }
             _e_sys_comp_resume();
             return ECORE_CALLBACK_RENEW;
          }
     }
   return ECORE_CALLBACK_RENEW;
after:
   switch (_e_sys_action_after)
     {
      case E_SYS_EXIT:
        _e_sys_comp_logout();
        break;
      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        _e_sys_comp_shutdown();
        break;
      case E_SYS_REBOOT:
        _e_sys_comp_reboot();
        break;
      default: break;
     }
   _e_sys_logout_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_sys_logout_after(void)
{
   if (_e_sys_dialog)
     {
        e_object_del(E_OBJECT(_e_sys_dialog));
        _e_sys_dialog = NULL;
     }
   _e_sys_action_current = _e_sys_action_after;
   _e_sys_action_do(_e_sys_action_after, NULL, _e_sys_action_after_raw);
   _e_sys_action_after = E_SYS_NONE;
   _e_sys_action_after_raw = EINA_FALSE;
}

static void
_e_sys_logout_begin(E_Sys_Action a_after, Eina_Bool raw)
{
   const Eina_List *l;
   E_Client *ec;
   E_Obj_Dialog *od;

   /* start logout - at end do the a_after action */
   if (!raw)
     {
        od = e_obj_dialog_new(_("Logout in progress"), "E", "_sys_logout");
        e_obj_dialog_obj_theme_set(od, "base/theme/sys", "e/sys/logout");
        e_obj_dialog_obj_part_text_set(od, "e.textblock.message",
                                       _("Logout in progress.<br>"
                                         "<hilight>Please wait.</hilight>"));
        e_obj_dialog_show(od);
        e_obj_dialog_icon_set(od, "system-log-out");
        if (_e_sys_dialog) e_object_del(E_OBJECT(_e_sys_dialog));
        _e_sys_dialog = od;
     }
   _e_sys_action_after = a_after;
   _e_sys_action_after_raw = raw;
   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        e_client_act_close_begin(ec);
     }
   /* and poll to see if all pending windows are gone yet every 0.5 sec */
   _e_sys_logout_begin_time = ecore_time_get();
   if (_e_sys_logout_timer) ecore_timer_del(_e_sys_logout_timer);
   _e_sys_logout_timer = ecore_timer_add(0.5, _e_sys_cb_logout_timer, NULL);
}

static void
_e_sys_current_action(void)
{
   /* display dialog that currently an action is in progress */
   E_Dialog *dia;

   dia = e_dialog_new(NULL,
                      "E", "_sys_error_action_busy");
   if (!dia) return;

   e_dialog_title_set(dia, _("Enlightenment is busy with another request"));
   e_dialog_icon_set(dia, "enlightenment/sys", 64);
   switch (_e_sys_action_current)
     {
      case E_SYS_LOGOUT:
        e_dialog_text_set(dia, _("Logging out.<br>"
                                 "You cannot perform other system actions<br>"
                                 "once a logout has begun."));
        break;

      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        e_dialog_text_set(dia, _("Powering off.<br>"
                                 "You cannot do any other system actions<br>"
                                 "once a shutdown has been started."));
        break;

      case E_SYS_REBOOT:
        e_dialog_text_set(dia, _("Resetting.<br>"
                                 "You cannot do any other system actions<br>"
                                 "once a reboot has begun."));
        break;

      case E_SYS_SUSPEND:
        e_dialog_text_set(dia, _("Suspending.<br>"
                                 "Until suspend is complete you cannot perform<br>"
                                 "any other system actions."));
        break;

      case E_SYS_HIBERNATE:
        e_dialog_text_set(dia, _("Hibernating.<br>"
                                 "You cannot perform any other system actions<br>"
                                 "until this is complete."));
        break;

      default:
        e_dialog_text_set(dia, _("EEK! This should not happen"));
        break;
     }
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   elm_win_center(dia->win, 1, 1);
   e_dialog_show(dia);
}

static void
_e_sys_action_failed(void)
{
   /* display dialog that the current action failed */
   E_Dialog *dia;

   dia = e_dialog_new(NULL,
                      "E", "_sys_error_action_failed");
   if (!dia) return;

   e_dialog_title_set(dia, _("Enlightenment is busy with another request"));
   e_dialog_icon_set(dia, "enlightenment/sys", 64);
   switch (_e_sys_action_current)
     {
      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        e_dialog_text_set(dia, _("Power off failed."));
        break;

      case E_SYS_REBOOT:
        e_dialog_text_set(dia, _("Reset failed."));
        break;

      case E_SYS_SUSPEND:
        e_dialog_text_set(dia, _("Suspend failed."));
        break;

      case E_SYS_HIBERNATE:
        e_dialog_text_set(dia, _("Hibernate failed."));
        break;

      default:
        e_dialog_text_set(dia, _("EEK! This should not happen"));
        break;
     }
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   elm_win_center(dia->win, 1, 1);
   e_dialog_show(dia);
}

static int
_e_sys_action_do(E_Sys_Action a, char *param __UNUSED__, Eina_Bool raw)
{
   char buf[PATH_MAX];
   E_Obj_Dialog *od;
   int ret = 0;

   switch (a)
     {
      case E_SYS_EXIT:
        // XXX TODO: check for e_fm_op_registry entries and confirm
        if (!e_util_immortal_check())
          ecore_main_loop_quit();
        else
          return 0;
        break;

      case E_SYS_RESTART:
        // XXX TODO: check for e_fm_op_registry entries and confirm
        // FIXME: we dont   share out immortal info to restarted e. :(
//	if (!e_util_immortal_check())
      {
         restart = 1;
         ecore_main_loop_quit();
      }
//        else
//          return 0;
      break;

      case E_SYS_EXIT_NOW:
        exit(0);
        break;

      case E_SYS_LOGOUT:
        // XXX TODO: check for e_fm_op_registry entries and confirm
        if (raw)
          {
             _e_sys_logout_after();
          }
        else
          {
             _e_sys_logout_begin(E_SYS_EXIT, raw);
          }
        break;

      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        /* shutdown -h now */
        if (e_util_immortal_check()) return 0;
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys halt",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else
          {
             if (raw)
               {
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_poweroff();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ret = 0;
                  _e_sys_begin_time = ecore_time_get();

                  od = e_obj_dialog_new(_("Power off"), "E", "_sys_halt");
                  e_obj_dialog_obj_theme_set(od, "base/theme/sys", "e/sys/halt");
                  e_obj_dialog_obj_part_text_set(od, "e.textblock.message",
                                                 _("Power off.<br>"
                                                   "<hilight>Please wait.</hilight>"));
                  e_obj_dialog_show(od);
                  e_obj_dialog_icon_set(od, "system-shutdown");
                  if (_e_sys_dialog) e_object_del(E_OBJECT(_e_sys_dialog));
                  e_obj_dialog_cb_delete_set(od, _e_sys_dialog_cb_delete);
                  _e_sys_dialog = od;
                  _e_sys_logout_begin(a, EINA_TRUE);
               }
             /* FIXME: display halt status */
          }
        break;

      case E_SYS_REBOOT:
        /* shutdown -r now */
        if (e_util_immortal_check()) return 0;
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys reboot",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else
          {
             if (raw)
               {
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_reboot();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ret = 0;
                  _e_sys_begin_time = ecore_time_get();
                  od = e_obj_dialog_new(_("Resetting"), "E", "_sys_reboot");
                  e_obj_dialog_obj_theme_set(od, "base/theme/sys", "e/sys/reboot");
                  e_obj_dialog_obj_part_text_set(od, "e.textblock.message",
                                                 _("Resetting.<br>"
                                                   "<hilight>Please wait.</hilight>"));
                  e_obj_dialog_show(od);
                  e_obj_dialog_icon_set(od, "system-restart");
                  if (_e_sys_dialog) e_object_del(E_OBJECT(_e_sys_dialog));
                  e_obj_dialog_cb_delete_set(od, _e_sys_dialog_cb_delete);
                  _e_sys_dialog = od;
                  _e_sys_logout_begin(a, EINA_TRUE);
               }
             /* FIXME: display reboot status */
          }
        break;

      case E_SYS_SUSPEND:
        /* /etc/acpi/sleep.sh force */
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys suspend",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else
          {
             if (raw)
               {
                  _e_sys_susp_hib_check();
                  if (e_config->desklock_on_suspend)
                    e_desklock_show(EINA_TRUE);
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_suspend();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ecore_event_add(E_EVENT_SYS_SUSPEND, NULL, NULL, NULL);
                  _e_sys_comp_suspend();
                  return 0;
               }
             /* FIXME: display suspend status */
          }
        break;

      case E_SYS_HIBERNATE:
        /* /etc/acpi/hibernate.sh force */
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys hibernate",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else
          {
             if (raw)
               {
                  _e_sys_susp_hib_check();
                  if (e_config->desklock_on_suspend)
                    e_desklock_show(EINA_TRUE);
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_hibernate();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ecore_event_add(E_EVENT_SYS_HIBERNATE, NULL, NULL, NULL);
                  _e_sys_comp_hibernate();
                  return 0;
               }
             /* FIXME: display hibernate status */
          }
        break;

      default:
        return 0;
     }
   return ret;
}

static void
_e_sys_dialog_cb_delete(E_Obj_Dialog *od __UNUSED__)
{
   /* If we don't NULL out the _e_sys_dialog, then the
    * ECORE_EXE_EVENT_DEL callback will trigger and segv if the window
    * is deleted in some other way. */
   _e_sys_dialog = NULL;
}

