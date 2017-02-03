#include "e.h"

static Ecore_Event_Handler *_e_screensaver_handler_on = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_off = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_config_mode = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_border_fullscreen = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_border_unfullscreen = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_border_remove = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_border_iconify = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_border_uniconify = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_border_desk_set = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_desk_show = NULL;
static Ecore_Event_Handler *_e_screensaver_handler_powersave = NULL;
static E_Dialog *_e_screensaver_ask_presentation_dia = NULL;
static int _e_screensaver_ask_presentation_count = 0;

static int _e_screensaver_timeout = 0;
//static int _e_screensaver_interval = 0;
static int _e_screensaver_blanking = 0;
static int _e_screensaver_expose = 0;

static Ecore_Timer *_e_screensaver_suspend_timer = NULL;
static Eina_Bool _e_screensaver_on = EINA_FALSE;

static Ecore_Timer *screensaver_idle_timer = NULL;
static Eina_Bool screensaver_dimmed = EINA_FALSE;

#ifdef HAVE_WAYLAND
static Ecore_Timer *_e_screensaver_timer;
static Eina_Bool _e_screensaver_inhibited = EINA_FALSE;
#endif

E_API int E_EVENT_SCREENSAVER_ON = -1;
E_API int E_EVENT_SCREENSAVER_OFF = -1;
E_API int E_EVENT_SCREENSAVER_OFF_PRE = -1;

#ifdef HAVE_WAYLAND
static Eina_Bool
_e_screensaver_idle_timeout_cb(void *d)
{
   e_screensaver_eval(!!d);
   _e_screensaver_timer = NULL;
   return EINA_FALSE;
}
#endif

E_API int
e_screensaver_timeout_get(Eina_Bool use_idle)
{
   int timeout = 0, count = (1 + _e_screensaver_ask_presentation_count);

   if ((e_config->screensaver_enable) && (!e_config->mode.presentation))
     timeout = e_config->screensaver_timeout * count;

   if ((use_idle) && (!e_config->mode.presentation))
     {
        if (e_config->backlight.idle_dim)
          {
             if (timeout > 0)
               {
                  if (e_config->backlight.timer < timeout)
                    timeout = e_config->backlight.timer;
               }
             else
               timeout = e_config->backlight.timer;
          }
     }
   return timeout;
}

E_API void
e_screensaver_update(void)
{
   int timeout;
   Eina_Bool changed = EINA_FALSE;

   timeout = e_screensaver_timeout_get(EINA_TRUE);
   if (_e_screensaver_timeout != timeout)
     {
        _e_screensaver_timeout = timeout;
        changed = EINA_TRUE;
     }
#ifndef HAVE_WAYLAND_ONLY
   int interval = 0, blanking = 0, expose = 0;

   interval = e_config->screensaver_interval;
   blanking = e_config->screensaver_blanking;
   expose = e_config->screensaver_expose;

//   if (_e_screensaver_interval != interval)
//     {
//        _e_screensaver_interval = interval;
//        changed = EINA_TRUE;
//     }
   if (_e_screensaver_blanking != blanking)
     {
        _e_screensaver_blanking = blanking;
        changed = EINA_TRUE;
     }
   if (_e_screensaver_expose != expose)
     {
        _e_screensaver_expose = expose;
        changed = EINA_TRUE;
     }

   if (e_comp->comp_type != E_PIXMAP_TYPE_WL)
     {
        if (changed)
          ecore_x_screensaver_set(timeout, interval, blanking, expose);
     }
#endif
#ifdef HAVE_WAYLAND
   if (changed && (e_comp->comp_type == E_PIXMAP_TYPE_WL))
     {
        E_FREE_FUNC(_e_screensaver_timer, ecore_timer_del);
        if (timeout)
          _e_screensaver_timer = ecore_timer_loop_add(timeout, _e_screensaver_idle_timeout_cb, (void*)1);
     }
#endif
}

static Eina_Bool
_e_screensaver_handler_config_mode_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   e_screensaver_update();
   return ECORE_CALLBACK_PASS_ON;
}

static void
_e_screensaver_ask_presentation_del(void *data)
{
   if (_e_screensaver_ask_presentation_dia == data)
     _e_screensaver_ask_presentation_dia = NULL;
}

static void
_e_screensaver_ask_presentation_yes(void *data EINA_UNUSED, E_Dialog *dia)
{
   e_config->mode.presentation = 1;
   e_config_mode_changed();
   e_config_save_queue();
   e_object_del(E_OBJECT(dia));
   _e_screensaver_ask_presentation_count = 0;
}

static void
_e_screensaver_ask_presentation_no(void *data EINA_UNUSED, E_Dialog *dia)
{
   e_object_del(E_OBJECT(dia));
   _e_screensaver_ask_presentation_count = 0;
}

static void
_e_screensaver_ask_presentation_no_increase(void *data EINA_UNUSED, E_Dialog *dia)
{
   _e_screensaver_ask_presentation_count++;
   e_screensaver_update();
   e_object_del(E_OBJECT(dia));
}

static void
_e_screensaver_ask_presentation_no_forever(void *data EINA_UNUSED, E_Dialog *dia)
{
   e_config->screensaver_ask_presentation = 0;
   e_config_save_queue();
   e_object_del(E_OBJECT(dia));
   _e_screensaver_ask_presentation_count = 0;
}

static void
_e_screensaver_ask_presentation_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event)
{
   Evas_Event_Key_Down *ev = event;
   E_Dialog *dia = data;

   if (strcmp(ev->key, "Return") == 0)
     _e_screensaver_ask_presentation_yes(NULL, dia);
   else if (strcmp(ev->key, "Escape") == 0)
     _e_screensaver_ask_presentation_no(NULL, dia);
}

static void
_e_screensaver_ask_presentation_mode(void)
{
   E_Dialog *dia;

   if (_e_screensaver_ask_presentation_dia) return;

   if (!(dia = e_dialog_new(NULL, "E", "_screensaver_ask_presentation"))) return;

   e_dialog_title_set(dia, _("Activate Presentation Mode?"));
   e_dialog_icon_set(dia, "dialog-ask", 64);
   e_dialog_text_set(dia,
                     _("You disabled the screensaver too fast.<br><br>"
                       "Would you like to enable <b>presentation</b> mode and "
                       "temporarily disable screen saver, lock and power saving?"));

   e_object_del_attach_func_set(E_OBJECT(dia),
                                _e_screensaver_ask_presentation_del);
   e_dialog_button_add(dia, _("Yes"), NULL,
                       _e_screensaver_ask_presentation_yes, NULL);
   e_dialog_button_add(dia, _("No"), NULL,
                       _e_screensaver_ask_presentation_no, NULL);
   e_dialog_button_add(dia, _("No, but increase timeout"), NULL,
                       _e_screensaver_ask_presentation_no_increase, NULL);
   e_dialog_button_add(dia, _("No, and stop asking"), NULL,
                       _e_screensaver_ask_presentation_no_forever, NULL);

   e_dialog_button_focus_num(dia, 0);
   e_widget_list_homogeneous_set(dia->box_object, 0);
   elm_win_center(dia->win, 1, 1);
   e_dialog_show(dia);

   evas_object_event_callback_add(dia->bg_object, EVAS_CALLBACK_KEY_DOWN,
                                  _e_screensaver_ask_presentation_key_down, dia);

   _e_screensaver_ask_presentation_dia = dia;
}

static Eina_Bool
_e_screensaver_suspend_cb(void *data EINA_UNUSED)
{
   _e_screensaver_suspend_timer = NULL;
   if (e_config->screensaver_suspend)
     {
        if ((e_config->screensaver_suspend_on_ac) ||
            (e_powersave_mode_get() > E_POWERSAVE_MODE_LOW))
          e_sys_action_do(E_SYS_SUSPEND, NULL);
     }
   return EINA_FALSE;
}

static Eina_Bool
_e_screensaver_handler_powersave_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   if ((e_config->screensaver_suspend) && (_e_screensaver_on))
     {
        if (_e_screensaver_suspend_timer)
          ecore_timer_del(_e_screensaver_suspend_timer);
        _e_screensaver_suspend_timer =
          ecore_timer_loop_add(e_config->screensaver_suspend_delay,
                          _e_screensaver_suspend_cb, NULL);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static double last_start = 0.0;

static Eina_Bool
_e_screensaver_handler_screensaver_on_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   _e_screensaver_on = EINA_TRUE;
   if (_e_screensaver_suspend_timer)
     {
        ecore_timer_del(_e_screensaver_suspend_timer);
        _e_screensaver_suspend_timer = NULL;
     }
   if (e_config->screensaver_suspend)
     _e_screensaver_suspend_timer =
       ecore_timer_loop_add(e_config->screensaver_suspend_delay,
                       _e_screensaver_suspend_cb, NULL);
   last_start = ecore_loop_time_get();
   _e_screensaver_ask_presentation_count = 0;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_screensaver_handler_screensaver_off_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
//   e_dpms_force_update();

   _e_screensaver_on = EINA_FALSE;
   if (_e_screensaver_suspend_timer)
     {
        ecore_timer_del(_e_screensaver_suspend_timer);
        _e_screensaver_suspend_timer = NULL;
     }
   if ((last_start > 0.0) && (e_config->screensaver_ask_presentation))
     {
        double current = ecore_loop_time_get();

        if ((last_start + e_config->screensaver_ask_presentation_timeout)
            >= current)
          _e_screensaver_ask_presentation_mode();
        last_start = 0.0;
     }
   else if (_e_screensaver_ask_presentation_count)
     _e_screensaver_ask_presentation_count = 0;
#ifdef HAVE_WAYLAND
   if (_e_screensaver_timeout && (e_comp->comp_type == E_PIXMAP_TYPE_WL))
     _e_screensaver_timer = ecore_timer_loop_add(_e_screensaver_timeout, _e_screensaver_idle_timeout_cb, (void*)1);
#endif
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_screensaver_handler_border_fullscreen_check_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   e_screensaver_update();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_screensaver_handler_border_desk_set_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   e_screensaver_update();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_screensaver_handler_desk_show_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   e_screensaver_update();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_screensaver_idle_timer_cb(void *d EINA_UNUSED)
{
   ecore_event_add(E_EVENT_SCREENSAVER_ON, NULL, NULL, NULL);
   screensaver_idle_timer = NULL;
   return EINA_FALSE;
}

EINTERN void
e_screensaver_preinit(void)
{
   E_EVENT_SCREENSAVER_ON = ecore_event_type_new();
   E_EVENT_SCREENSAVER_OFF = ecore_event_type_new();
   E_EVENT_SCREENSAVER_OFF_PRE = ecore_event_type_new();
}

EINTERN int
e_screensaver_init(void)
{
   _e_screensaver_handler_on = ecore_event_handler_add
       (E_EVENT_SCREENSAVER_ON, _e_screensaver_handler_screensaver_on_cb, NULL);
   _e_screensaver_handler_off = ecore_event_handler_add
       (E_EVENT_SCREENSAVER_OFF, _e_screensaver_handler_screensaver_off_cb, NULL);
   _e_screensaver_handler_config_mode = ecore_event_handler_add
       (E_EVENT_CONFIG_MODE_CHANGED, _e_screensaver_handler_config_mode_cb, NULL);

   _e_screensaver_handler_border_fullscreen = ecore_event_handler_add
       (E_EVENT_CLIENT_FULLSCREEN, _e_screensaver_handler_border_fullscreen_check_cb, NULL);
   _e_screensaver_handler_border_unfullscreen = ecore_event_handler_add
       (E_EVENT_CLIENT_UNFULLSCREEN, _e_screensaver_handler_border_fullscreen_check_cb, NULL);
   _e_screensaver_handler_border_remove = ecore_event_handler_add
       (E_EVENT_CLIENT_REMOVE, _e_screensaver_handler_border_fullscreen_check_cb, NULL);
   _e_screensaver_handler_border_iconify = ecore_event_handler_add
       (E_EVENT_CLIENT_ICONIFY, _e_screensaver_handler_border_fullscreen_check_cb, NULL);
   _e_screensaver_handler_border_uniconify = ecore_event_handler_add
       (E_EVENT_CLIENT_UNICONIFY, _e_screensaver_handler_border_fullscreen_check_cb, NULL);
   _e_screensaver_handler_border_desk_set = ecore_event_handler_add
       (E_EVENT_CLIENT_DESK_SET, _e_screensaver_handler_border_desk_set_cb, NULL);
   _e_screensaver_handler_desk_show = ecore_event_handler_add
       (E_EVENT_DESK_SHOW, _e_screensaver_handler_desk_show_cb, NULL);

   _e_screensaver_handler_powersave = ecore_event_handler_add
       (E_EVENT_POWERSAVE_UPDATE, _e_screensaver_handler_powersave_cb, NULL);

   return 1;
}

EINTERN int
e_screensaver_shutdown(void)
{
   if (_e_screensaver_handler_on)
     {
        ecore_event_handler_del(_e_screensaver_handler_on);
        _e_screensaver_handler_on = NULL;
     }

   if (_e_screensaver_handler_off)
     {
        ecore_event_handler_del(_e_screensaver_handler_off);
        _e_screensaver_handler_off = NULL;
     }

   if (_e_screensaver_suspend_timer)
     {
        ecore_timer_del(_e_screensaver_suspend_timer);
        _e_screensaver_suspend_timer = NULL;
     }

   if (_e_screensaver_handler_powersave)
     {
        ecore_event_handler_del(_e_screensaver_handler_powersave);
        _e_screensaver_handler_powersave = NULL;
     }

   if (_e_screensaver_handler_config_mode)
     {
        ecore_event_handler_del(_e_screensaver_handler_config_mode);
        _e_screensaver_handler_config_mode = NULL;
     }

   if (_e_screensaver_handler_border_fullscreen)
     {
        ecore_event_handler_del(_e_screensaver_handler_border_fullscreen);
        _e_screensaver_handler_border_fullscreen = NULL;
     }

   if (_e_screensaver_handler_border_unfullscreen)
     {
        ecore_event_handler_del(_e_screensaver_handler_border_unfullscreen);
        _e_screensaver_handler_border_unfullscreen = NULL;
     }

   if (_e_screensaver_handler_border_remove)
     {
        ecore_event_handler_del(_e_screensaver_handler_border_remove);
        _e_screensaver_handler_border_remove = NULL;
     }

   if (_e_screensaver_handler_border_iconify)
     {
        ecore_event_handler_del(_e_screensaver_handler_border_iconify);
        _e_screensaver_handler_border_iconify = NULL;
     }

   if (_e_screensaver_handler_border_uniconify)
     {
        ecore_event_handler_del(_e_screensaver_handler_border_uniconify);
        _e_screensaver_handler_border_uniconify = NULL;
     }

   if (_e_screensaver_handler_border_desk_set)
     {
        ecore_event_handler_del(_e_screensaver_handler_border_desk_set);
        _e_screensaver_handler_border_desk_set = NULL;
     }

   if (_e_screensaver_handler_desk_show)
     {
        ecore_event_handler_del(_e_screensaver_handler_desk_show);
        _e_screensaver_handler_desk_show = NULL;
     }

   return 1;
}

E_API void
e_screensaver_attrs_set(int timeout, int blanking, int expose)
{
   _e_screensaver_timeout = timeout;
//   _e_screensaver_interval = ecore_x_screensaver_interval_get();
   _e_screensaver_blanking = blanking;
   _e_screensaver_expose = expose;
}

E_API Eina_Bool
e_screensaver_on_get(void)
{
   return _e_screensaver_on;
}

E_API void
e_screensaver_activate(void)
{
   if (e_screensaver_on_get()) return;

   E_FREE_FUNC(screensaver_idle_timer, ecore_timer_del);
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type != E_PIXMAP_TYPE_WL)
     ecore_x_screensaver_activate();
#endif
#ifdef HAVE_WAYLAND
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     e_screensaver_eval(1);
   E_FREE_FUNC(_e_screensaver_timer, ecore_timer_del);
#endif
}

E_API void
e_screensaver_deactivate(void)
{
   if (!e_screensaver_on_get()) return;

   E_FREE_FUNC(screensaver_idle_timer, ecore_timer_del);
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type != E_PIXMAP_TYPE_WL)
     ecore_x_screensaver_reset();
#endif
#ifdef HAVE_WAYLAND
   e_screensaver_notidle();
#endif
}

E_API void
e_screensaver_eval(Eina_Bool saver_on)
{
   if (saver_on)
     {
        if (e_config->backlight.idle_dim)
          {
             double t = e_config->screensaver_timeout -
               e_config->backlight.timer;

             if (t < 1.0) t = 1.0;
             E_FREE_FUNC(screensaver_idle_timer, ecore_timer_del);
             if (e_config->screensaver_enable)
               screensaver_idle_timer = ecore_timer_loop_add
                   (t, _e_screensaver_idle_timer_cb, NULL);
             if (e_backlight_mode_get(NULL) != E_BACKLIGHT_MODE_DIM)
               {
                  e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_DIM);
                  screensaver_dimmed = EINA_TRUE;
               }
          }
        else
          {
             if (!e_screensaver_on_get())
               ecore_event_add(E_EVENT_SCREENSAVER_ON, NULL, NULL, NULL);
          }
        return;
     }
   if (screensaver_idle_timer)
     {
        E_FREE_FUNC(screensaver_idle_timer, ecore_timer_del);
        if (e_config->backlight.idle_dim)
          {
             if (e_backlight_mode_get(NULL) != E_BACKLIGHT_MODE_NORMAL)
               e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_NORMAL);
          }
        return;
     }
   if (screensaver_dimmed)
     {
        if (e_backlight_mode_get(NULL) != E_BACKLIGHT_MODE_NORMAL)
          e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_NORMAL);
        screensaver_dimmed = EINA_FALSE;
     }
   if (e_screensaver_on_get())
     ecore_event_add(E_EVENT_SCREENSAVER_OFF, NULL, NULL, NULL);
}

E_API void
e_screensaver_notidle(void)
{
#ifdef HAVE_WAYLAND
   if (_e_screensaver_inhibited || (e_comp->comp_type != E_PIXMAP_TYPE_WL)) return;
   E_FREE_FUNC(_e_screensaver_timer, ecore_timer_del);
   if (e_screensaver_on_get())
     {
        ecore_event_add(E_EVENT_SCREENSAVER_OFF_PRE, NULL, NULL, NULL);
        _e_screensaver_timer = ecore_timer_loop_add(0.2, _e_screensaver_idle_timeout_cb, NULL);
     }
   else if (_e_screensaver_timeout)
     _e_screensaver_timer = ecore_timer_loop_add(_e_screensaver_timeout, _e_screensaver_idle_timeout_cb, (void*)1);
#endif
}

E_API void
e_screensaver_inhibit_toggle(Eina_Bool inhibit)
{
#ifdef HAVE_WAYLAND
   if (e_comp->comp_type != E_PIXMAP_TYPE_WL) return;
   E_FREE_FUNC(_e_screensaver_timer, ecore_timer_del);
   _e_screensaver_inhibited = !!inhibit;
   if (inhibit)
     e_screensaver_eval(0);
   else
     e_screensaver_notidle();
#else
   (void)inhibit;
#endif
}
