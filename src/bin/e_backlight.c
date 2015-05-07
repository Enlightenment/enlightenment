#include "e.h"
#ifdef HAVE_EEZE
# include <Eeze.h>
#endif

// FIXME: backlight should be tied per zone but this implementation is just
// a signleton right now as thats 99% of use cases. but api supports
// doing more. for now make it work in the singleton

#define MODE_NONE  -1
#define MODE_RANDR 0
#define MODE_SYS   1

EINTERN double e_bl_val = 1.0;
static double bl_animval = 1.0;
static double bl_anim_toval = 1.0;
static int sysmode = MODE_NONE;
static Ecore_Animator *bl_anim = NULL;
static Eina_List *bl_devs = NULL;

static void      _e_backlight_update(E_Zone *zone);
static void      _e_backlight_set(E_Zone *zone, double val);
static Eina_Bool _bl_anim(void *data, double pos);
static Eina_Bool bl_avail = EINA_TRUE;
#ifndef HAVE_WAYLAND_ONLY
static Eina_Bool xbl_avail = EINA_FALSE;
#endif
#ifdef HAVE_EEZE
static double bl_delayval = 1.0;
static const char *bl_sysval = NULL;
static Ecore_Event_Handler *bl_sys_exit_handler = NULL;
static Ecore_Exe *bl_sys_set_exe = NULL;
static Eina_Bool bl_sys_pending_set = EINA_FALSE;
static Eina_Bool bl_sys_set_exe_ready = EINA_TRUE;

static void      _bl_sys_find(void);
static void      _bl_sys_level_get(void);
static Eina_Bool _e_bl_cb_exit(void *data __UNUSED__, int type __UNUSED__, void *event);
static void      _bl_sys_level_set(double val);
#endif

E_API int E_EVENT_BACKLIGHT_CHANGE = -1;

EINTERN int
e_backlight_init(void)
{
#ifdef HAVE_EEZE
   eeze_init();
#endif

#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_get(NULL)->comp_type == E_PIXMAP_TYPE_X)
     xbl_avail = ecore_x_randr_output_backlight_available();
#endif
   e_backlight_update();
   if (!getenv("E_RESTART"))
     {
        e_backlight_level_set(NULL, 0.1, 0.0);
        e_backlight_level_set(NULL, e_config->backlight.normal, 0.0);
     }

   E_EVENT_BACKLIGHT_CHANGE = ecore_event_type_new();

   return 1;
}

EINTERN int
e_backlight_shutdown(void)
{
   const char *s;

   if (bl_anim) ecore_animator_del(bl_anim);
   bl_anim = NULL;

   if (e_config->backlight.mode != E_BACKLIGHT_MODE_NORMAL)
     e_backlight_level_set(NULL, e_config->backlight.normal, 0.0);

   EINA_LIST_FREE(bl_devs, s)
     eina_stringshare_del(s);
#ifdef HAVE_EEZE
   if (bl_sysval) eina_stringshare_del(bl_sysval);
   bl_sysval = NULL;
   if (bl_sys_exit_handler) ecore_event_handler_del(bl_sys_exit_handler);
   bl_sys_exit_handler = NULL;
   bl_sys_set_exe = NULL;
   bl_sys_pending_set = EINA_FALSE;
   eeze_shutdown();
#endif

   return 1;
}

E_API Eina_Bool
e_backlight_exists(void)
{
   if (sysmode == MODE_NONE) return EINA_FALSE;
   return EINA_TRUE;
}

E_API void
e_backlight_update(void)
{
   const Eina_List *l, *ll;
   E_Comp *c;
   E_Zone *zone;

   if (bl_avail == EINA_FALSE) return;

   EINA_LIST_FOREACH(e_comp_list(), l, c)
     EINA_LIST_FOREACH(c->zones, ll, zone)
       _e_backlight_update(zone);
}

E_API void
e_backlight_level_set(E_Zone *zone, double val, double tim)
{
   double bl_now;
   // zone == NULL == everything
   // set backlight associated with zone to val over period of tim
   // if tim == 0.0 - then do it instantnly, if time == -1 use some default
   // transition time
   if (val < 0.0) val = 0.0;
   else if (val > 1.0)
     val = 1.0;
   if ((fabs(val - e_bl_val) < DBL_EPSILON) && (!bl_anim)) return;
   if (!zone) zone = e_util_zone_current_get(e_manager_current_get());
   ecore_event_add(E_EVENT_BACKLIGHT_CHANGE, NULL, NULL, NULL);
   bl_now = e_bl_val;

   if (sysmode != MODE_RANDR)
     e_bl_val = val;

   if (fabs(tim) < DBL_EPSILON)
     {
        _e_backlight_set(zone, val);
        e_backlight_update();
        return;
     }
//   if (e_config->backlight.mode != E_BACKLIGHT_MODE_NORMAL) return;
   if (e_config->backlight.mode == E_BACKLIGHT_MODE_NORMAL)
     tim = 0.5;
   else
   if (tim < 0.0)
     tim = e_config->backlight.transition;

   E_FREE_FUNC(bl_anim, ecore_animator_del);
   bl_anim = ecore_animator_timeline_add(tim, _bl_anim, zone);
   bl_animval = bl_now;
   bl_anim_toval = val;
}

E_API double
e_backlight_level_get(E_Zone *zone __UNUSED__)
{
   // zone == NULL == everything
   return e_bl_val;
}

E_API void
e_backlight_mode_set(E_Zone *zone, E_Backlight_Mode mode)
{
   E_Backlight_Mode pmode;
   
   // zone == NULL == everything
   if (e_config->backlight.mode == mode) return;
   pmode = e_config->backlight.mode;
   e_config->backlight.mode = mode;
   if (e_config->backlight.mode == E_BACKLIGHT_MODE_NORMAL)
     {
        e_backlight_level_set(zone, e_config->backlight.normal, -1.0);
     }
   else if (e_config->backlight.mode == E_BACKLIGHT_MODE_OFF)
     {
        e_backlight_level_set(zone, 0.0, -1.0);
     }
   else if (e_config->backlight.mode == E_BACKLIGHT_MODE_DIM)
     {
        if ((pmode != E_BACKLIGHT_MODE_NORMAL) ||
            ((pmode == E_BACKLIGHT_MODE_NORMAL) &&
             (e_config->backlight.normal > e_config->backlight.dim)))
          e_backlight_level_set(zone, e_config->backlight.dim, -1.0);
     }
   else if (e_config->backlight.mode == E_BACKLIGHT_MODE_MAX)
     e_backlight_level_set(zone, 1.0, -1.0);
}

E_API E_Backlight_Mode
e_backlight_mode_get(E_Zone *zone __UNUSED__)
{
   // zone == NULL == everything
   return e_config->backlight.mode;
}

E_API const Eina_List *
e_backlight_devices_get(void)
{
   return bl_devs;
}

/* local subsystem functions */

static void
_e_backlight_update(E_Zone *zone)
{
#ifndef HAVE_WAYLAND_ONLY
   double x_bl = -1.0;
   Ecore_X_Window root;
   Ecore_X_Randr_Output *out;
   int i, num = 0;

   root = zone->comp->man->root;
   // try randr
   if (root && xbl_avail)
     {
        out = ecore_x_randr_window_outputs_get(root, &num);
        if ((out) && (num > 0))
          {
             char *name;
             Eina_Bool gotten = EINA_FALSE;

             E_FREE_LIST(bl_devs, eina_stringshare_del);
             for (i = 0; i < num; i++)
               {
                  Eina_Stringshare *n;

                  name = ecore_x_randr_output_name_get(root, out[i], NULL);
                  if (!name) continue;
                  n = eina_stringshare_add(name);
                  free(name);
                  bl_devs = eina_list_append(bl_devs, n);
                  if (!e_util_strcmp(n, e_config->backlight.sysdev))
                    {
                       x_bl = ecore_x_randr_output_backlight_level_get(root, out[i]);
                       gotten = EINA_TRUE;
                    }
               }
             if (!gotten)
               x_bl = ecore_x_randr_output_backlight_level_get(root, out[0]);
          }
        free(out);
     }
   if (x_bl >= 0.0)
     {
        e_bl_val = x_bl;
        sysmode = MODE_RANDR;
        return;
     }
#else
   (void)zone;
#endif
#ifdef HAVE_EEZE
   _bl_sys_find();
   if (bl_sysval)
     {
        sysmode = MODE_SYS;
        _bl_sys_level_get();
        return;
     }
#endif
}

static void
_e_backlight_set(E_Zone *zone, double val)
{
#ifdef HAVE_WAYLAND_ONLY
   if (0)
     {
        (void)zone;
        (void)val;
        return;
     }
#else
   if (sysmode == MODE_RANDR)
     {
        Ecore_X_Window root;
        Ecore_X_Randr_Output *out;
        int num = 0, i;
        char *name;

        root = zone->comp->man->root;
        out = ecore_x_randr_window_outputs_get(root, &num);
        if ((out) && (num > 0))
          {
             Eina_Bool gotten = EINA_FALSE;
             if (e_config->backlight.sysdev)
               {
                  for (i = 0; i < num; i++)
                    {
                       name = ecore_x_randr_output_name_get(root, out[i], NULL);
                       if (name)
                         {
                            if (!strcmp(name, e_config->backlight.sysdev))
                              {
                                 ecore_x_randr_output_backlight_level_set(root, out[i], val);
                                 gotten = EINA_TRUE;
                              }
                            free(name);
                         }
                    }
               }
             if (!gotten)
               {
                  for (i = 0; i < num; i++)
                    ecore_x_randr_output_backlight_level_set(root, out[i], val);
               }
          }
        free(out);
     }
#endif
#ifdef HAVE_EEZE
   else if (sysmode == MODE_SYS)
     {
        if (bl_sysval)
          {
             _bl_sys_level_set(val);
          }
     }
#endif
}

static Eina_Bool
_bl_anim(void *data, double pos)
{
   E_Zone *zone = data;
   double v;

   // FIXME: if zone is deleted while anim going... bad things.
   pos = ecore_animator_pos_map(pos, ECORE_POS_MAP_DECELERATE, 0.0, 0.0);
   v = (bl_animval * (1.0 - pos)) + (bl_anim_toval * pos);
   _e_backlight_set(zone, v);
   if (pos >= 1.0)
     {
        bl_anim = NULL;
        e_backlight_update();
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

#ifdef HAVE_EEZE
static void
_bl_sys_find(void)
{
   Eina_List *l, *devs, *pdevs = NULL;
   Eina_Bool use;
   const char *f, *s;
   int v;

   devs = eeze_udev_find_by_filter("backlight", NULL, NULL);
   if (!devs)
     {
        /* FIXME: need to make this more precise so we don't set keyboard LEDs or something */
        devs = eeze_udev_find_by_filter("leds", NULL, NULL);
        if (!devs) return;
     }
   if (eina_list_count(devs) > 1)
     {
        /* prefer backlights of type "firmware" where available */
        EINA_LIST_FOREACH(devs, l, f)
          {
             use = eeze_udev_syspath_check_sysattr(f, "type", "firmware");
             if (!use) continue;
             s = eeze_udev_syspath_get_sysattr(f, "brightness");
             if (!s) continue;
             v = atoi(s);
             eina_stringshare_del(s);
             if (v < 0) continue;
             pdevs = eina_list_append(pdevs, eina_stringshare_ref(f));
             eina_stringshare_del(f);
             l->data = NULL;
          }
        EINA_LIST_FOREACH(devs, l, f)
          {
             if (!l->data) continue;
             s = eeze_udev_syspath_get_sysattr(f, "brightness");
             if (!s) continue;
             v = atoi(s);
             eina_stringshare_del(s);
             if (v < 0) continue;
             pdevs = eina_list_append(pdevs, eina_stringshare_ref(f));
          }
     }
   if (!pdevs)
     {
        /* add the other backlight or led's if none found */
        EINA_LIST_FOREACH(devs, l, f)
          {
             use = EINA_FALSE;
             s = eeze_udev_syspath_get_sysattr(f, "brightness");
             if (!s) continue;
             v = atoi(s);
             eina_stringshare_del(s);
             if (v < 0) continue;
             pdevs = eina_list_append(pdevs, eina_stringshare_ref(f));
          }
     }
   /* clear out original devs list now we've filtered */
   E_FREE_LIST(devs, eina_stringshare_del);
   /* clear out old configured bl sysval */
   eina_stringshare_replace(&bl_sysval, NULL);
   E_FREE_LIST(bl_devs, eina_stringshare_del);
   /* if configured backlight is there - use it, or if not use first */
   EINA_LIST_FOREACH(pdevs, l, f)
     {
        bl_devs = eina_list_append(bl_devs, eina_stringshare_add(f));
        if (!bl_sysval)
          {
             if (!e_util_strcmp(e_config->backlight.sysdev, f))
               bl_sysval = eina_stringshare_ref(f);
          }
     }
   if (!bl_sysval)
     {
        EINA_LIST_FOREACH(pdevs, l, f)
          {
             if ((!strstr(f, "kbd")) && (!strstr(f, "mail")))
               {
                  bl_sysval = eina_stringshare_add(f);
                  break;
               }
          }
     }
   /* clear out preferred devs list */
   E_FREE_LIST(pdevs, eina_stringshare_del);
}

static void
_bl_sys_level_get(void)
{
   int maxval, val;
   const char *str;

   if (bl_anim) return;

   str = eeze_udev_syspath_get_sysattr(bl_sysval, "max_brightness");
   if (!str) return;

   maxval = atoi(str);
   eina_stringshare_del(str);
   if (maxval < 0) maxval = 255;
   str = eeze_udev_syspath_get_sysattr(bl_sysval, "brightness");
   if (!str) return;

   val = atoi(str);
   eina_stringshare_del(str);
   if ((!maxval) && (!val))
     {
        e_bl_val = 0;
        sysmode = MODE_NONE;
        return;
     }
   if (!maxval) maxval = 255;
   if ((val >= 0) && (val <= maxval))
     e_bl_val = (double)val / (double)maxval;
//   fprintf(stderr, "GET: %i/%i (%1.3f)\n", val, maxval, e_bl_val);
}

static Eina_Bool
_e_bl_cb_ext_delay(void *data __UNUSED__)
{
   bl_sys_set_exe_ready = EINA_TRUE;
   if (bl_sys_pending_set)
     {
        bl_sys_pending_set = EINA_FALSE;

        _bl_sys_level_set(bl_delayval);
     }
   return EINA_FALSE;
}

static Eina_Bool
_e_bl_cb_exit(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Exe_Event_Del *ev;

   ev = event;
   if (ev->exe == bl_sys_set_exe)
     {
        bl_sys_set_exe_ready = EINA_FALSE;
        bl_sys_set_exe = NULL;
        ecore_timer_add(0.05, _e_bl_cb_ext_delay, NULL);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_bl_sys_level_set(double val)
{
   char buf[PATH_MAX];

   if (!bl_sys_exit_handler)
     bl_sys_exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                                   _e_bl_cb_exit, NULL);
   bl_delayval = val;
   if ((bl_sys_set_exe) || (!bl_sys_set_exe_ready))
     {
        bl_sys_pending_set = EINA_TRUE;
        return;
     }
//   fprintf(stderr, "SET: %1.3f\n", val);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_backlight %i %s",
            e_prefix_lib_get(), (int)(val * 1000.0), bl_sysval);
   bl_sys_set_exe = ecore_exe_run(buf, NULL);
}

#endif
