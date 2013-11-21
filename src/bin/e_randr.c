#include "e.h"

/* TODO: Handle orientation in stored config */
/* TODO: Do we need e_randr_cfg->screen.{width,height} ? */
/* TODO: Check so we can rely on connected only, not connected && exists */
/* TODO: Ignore xrandr events triggered by changes in acpi cb */

/* local function prototypes */
static Eina_Bool _e_randr_config_load(void);
static void      _e_randr_config_new(void);
static void      _e_randr_config_update(void);
static void      _e_randr_config_free(void);
static Eina_Bool _e_randr_config_cb_timer(void *data);
static void      _e_randr_config_apply(void);
static void                   _e_randr_config_output_mode_update(E_Randr_Output_Config *cfg);
static E_Randr_Crtc_Config   *_e_randr_config_crtc_new(unsigned int id);
static void                   _e_randr_config_crtc_update(E_Randr_Crtc_Config *cfg);
static E_Randr_Output_Config *_e_randr_config_output_new(unsigned int id);
static E_Randr_Crtc_Config   *_e_randr_config_crtc_find(Ecore_X_Randr_Crtc crtc);
static E_Randr_Output_Config *_e_randr_config_output_find(Ecore_X_Randr_Output output);
static E_Randr_Crtc_Config   *_e_randr_config_output_crtc_find(E_Randr_Output_Config *cfg);

static void _e_randr_config_mode_geometry(Ecore_X_Randr_Orientation orient, Eina_Rectangle *rect);
static void _e_randr_config_primary_update(void);

static Eina_Bool _e_randr_event_cb_screen_change(void *data, int type, void *event);
static Eina_Bool _e_randr_event_cb_crtc_change(void *data, int type, void *event);
static Eina_Bool _e_randr_event_cb_output_change(void *data, int type, void *event);

static void      _e_randr_acpi_handler_add(void *data);
static int       _e_randr_is_lid(E_Randr_Output_Config *cfg);
static void      _e_randr_config_outputs_from_crtc_set(E_Randr_Crtc_Config *crtc_cfg);
static void      _e_randr_config_crtc_from_outputs_set(E_Randr_Crtc_Config *crtc_cfg);
static Eina_Bool _e_randr_config_lid_update(void);
static Eina_Bool _e_randr_output_mode_valid(Ecore_X_Randr_Mode mode, Ecore_X_Randr_Mode *modes, int nmodes);
static void      _e_randr_output_connected_set(E_Randr_Output_Config *cfg, Eina_Bool connected);
static int       _e_randr_config_output_cmp(const void *a, const void *b);

/* local variables */
static Eina_List *_randr_event_handlers = NULL;
static E_Config_DD *_e_randr_edd = NULL;
static E_Config_DD *_e_randr_crtc_edd = NULL;
static E_Config_DD *_e_randr_output_edd = NULL;

static int _e_randr_lid_is_closed = 0;

/* external variables */
EAPI E_Randr_Config *e_randr_cfg = NULL;

/* private internal functions */
EINTERN Eina_Bool
e_randr_init(void)
{
   /* check if randr is available */
   if (!ecore_x_randr_query()) return EINA_FALSE;

   /* get initial lid status */
   _e_randr_lid_is_closed = (e_acpi_lid_status_get() == E_ACPI_LID_CLOSED);

   /* try to load config */
   if (!_e_randr_config_load())
     {
        /* NB: We should probably print an error here */
        return EINA_FALSE;
     }

   /* tell randr that we are interested in receiving events
    *
    * NB: Requires RandR >= 1.2 */
   if (ecore_x_randr_version_get() >= E_RANDR_VERSION_1_2)
     {
        Ecore_X_Window root = 0;

        if ((root = ecore_x_window_root_first_get()))
          ecore_x_randr_events_select(root, EINA_TRUE);

        /* setup randr event listeners */
        E_LIST_HANDLER_APPEND(_randr_event_handlers,
                              ECORE_X_EVENT_SCREEN_CHANGE,
                              _e_randr_event_cb_screen_change, NULL);
        E_LIST_HANDLER_APPEND(_randr_event_handlers,
                              ECORE_X_EVENT_RANDR_CRTC_CHANGE,
                              _e_randr_event_cb_crtc_change, NULL);
        E_LIST_HANDLER_APPEND(_randr_event_handlers,
                              ECORE_X_EVENT_RANDR_OUTPUT_CHANGE,
                              _e_randr_event_cb_output_change, NULL);
     }

   /* delay setting up acpi handler, as acpi is init'ed after randr */
   ecore_job_add(_e_randr_acpi_handler_add, NULL);

   return EINA_TRUE;
}

EINTERN int
e_randr_shutdown(void)
{
   /* check if randr is available */
   if (!ecore_x_randr_query()) return 1;

   if (ecore_x_randr_version_get() >= E_RANDR_VERSION_1_2)
     {
        Ecore_X_Window root = 0;

        /* tell randr that we are not interested in receiving events anymore */
        if ((root = ecore_x_window_root_first_get()))
          ecore_x_randr_events_select(root, EINA_FALSE);
     }

   /* remove event listeners */
   E_FREE_LIST(_randr_event_handlers, ecore_event_handler_del);

   /* free config */
   _e_randr_config_free();

   /* free edd */
   E_CONFIG_DD_FREE(_e_randr_output_edd);
   E_CONFIG_DD_FREE(_e_randr_crtc_edd);
   E_CONFIG_DD_FREE(_e_randr_edd);

   return 1;
}

/* public API functions */
EAPI Eina_Bool
e_randr_config_save(void)
{
   /* save the new config */
   return e_config_domain_save("e_randr", _e_randr_edd, e_randr_cfg);
}

/* local functions */
static Eina_Bool
_e_randr_config_load(void)
{
   Eina_Bool do_restore = EINA_TRUE;

   /* define edd for output config */
   _e_randr_output_edd =
     E_CONFIG_DD_NEW("E_Randr_Output_Config", E_Randr_Output_Config);
#undef T
#undef D
#define T E_Randr_Output_Config
#define D _e_randr_output_edd
   E_CONFIG_VAL(D, T, xid, UINT);
   E_CONFIG_VAL(D, T, crtc, UINT);
   E_CONFIG_VAL(D, T, orient, UINT);
   E_CONFIG_VAL(D, T, geo.x, INT);
   E_CONFIG_VAL(D, T, geo.y, INT);
   E_CONFIG_VAL(D, T, geo.w, INT);
   E_CONFIG_VAL(D, T, geo.h, INT);
   E_CONFIG_VAL(D, T, connected, UCHAR);

   /* define edd for crtc config */
   _e_randr_crtc_edd =
     E_CONFIG_DD_NEW("E_Randr_Crtc_Config", E_Randr_Crtc_Config);
#undef T
#undef D
#define T E_Randr_Crtc_Config
#define D _e_randr_crtc_edd
   E_CONFIG_VAL(D, T, xid, UINT);

   /* define edd for randr config */
   _e_randr_edd = E_CONFIG_DD_NEW("E_Randr_Config", E_Randr_Config);
#undef T
#undef D
#define T E_Randr_Config
#define D _e_randr_edd
   E_CONFIG_VAL(D, T, version, INT);
   E_CONFIG_VAL(D, T, screen.width, INT);
   E_CONFIG_VAL(D, T, screen.height, INT);
   E_CONFIG_LIST(D, T, crtcs, _e_randr_crtc_edd);
   E_CONFIG_LIST(D, T, outputs, _e_randr_output_edd);
   E_CONFIG_VAL(D, T, restore, UCHAR);
   E_CONFIG_VAL(D, T, poll_interval, INT);
   E_CONFIG_VAL(D, T, config_timestamp, ULL);
   E_CONFIG_VAL(D, T, primary, UINT);

   /* try to load the randr config */
   if ((e_randr_cfg = e_config_domain_load("e_randr", _e_randr_edd)))
     {
        /* check randr config version */
        if (e_randr_cfg->version < (E_RANDR_CONFIG_FILE_EPOCH * 1000000))
          {
             /* config is too old */
             do_restore = EINA_FALSE;
             _e_randr_config_free();
             ecore_timer_add(1.0, _e_randr_config_cb_timer,
                             _("Settings data needed upgrading. Your old settings have<br>"
                               "been wiped and a new set of defaults initialized. This<br>"
                               "will happen regularly during development, so don't report a<br>"
                               "bug. This simply means Enlightenment needs new settings<br>"
                               "data by default for usable functionality that your old<br>"
                               "settings simply lack. This new set of defaults will fix<br>"
                               "that by adding it in. You can re-configure things now to your<br>"
                               "liking. Sorry for the hiccup in your settings.<br>"));
          }
        else if (e_randr_cfg->version > E_RANDR_CONFIG_FILE_VERSION)
          {
             /* config is too new */
             do_restore = EINA_FALSE;
             _e_randr_config_free();
             ecore_timer_add(1.0, _e_randr_config_cb_timer,
                             _("Your settings are NEWER than Enlightenment. This is very<br>"
                               "strange. This should not happen unless you downgraded<br>"
                               "Enlightenment or copied the settings from a place where<br>"
                               "a newer version of Enlightenment was running. This is bad and<br>"
                               "as a precaution your settings have been now restored to<br>"
                               "defaults. Sorry for the inconvenience.<br>"));
          }
     }

   /* if config was too old or too new, then reload a fresh one */
   if (!e_randr_cfg)
     {
        _e_randr_config_new();
        do_restore = EINA_FALSE;
     }

   /* e_randr_config_new could return without actually creating a new config */
   if (!e_randr_cfg) return EINA_FALSE;

   _e_randr_config_update();

   if ((do_restore) && (e_randr_cfg->restore))
     _e_randr_config_apply();

   return EINA_TRUE;
}

static void
_e_randr_config_new(void)
{
   Ecore_X_Window root = 0;

   /* create new randr cfg */
   if (!(e_randr_cfg = E_NEW(E_Randr_Config, 1)))
     return;

   /* grab the root window once */
   root = ecore_x_window_root_first_get();

   /* set version */
   e_randr_cfg->version = E_RANDR_CONFIG_FILE_VERSION;

   /* by default, restore config */
   e_randr_cfg->restore = EINA_TRUE;

   /* by default, use 4 sec poll interval */
   e_randr_cfg->poll_interval = 32;

   /* set limits */
   E_CONFIG_LIMIT(e_randr_cfg->poll_interval, 1, 1024);

   /* record the current screen size in our config */
   ecore_x_randr_screen_current_size_get(root, &e_randr_cfg->screen.width,
                                         &e_randr_cfg->screen.height,
                                         NULL, NULL);

   /* remember current primary */
   e_randr_cfg->primary = ecore_x_randr_primary_output_get(root);

   /* update recorded config timestamp */
   e_randr_cfg->config_timestamp = ecore_x_randr_config_timestamp_get(root);
}

/* function to map X's settings with E's settings */
static void
_e_randr_config_update(void)
{
   E_Randr_Crtc_Config *crtc_cfg = NULL;
   E_Randr_Output_Config *output_cfg = NULL;
   Eina_List *l;
   Ecore_X_Window root = 0;
   Ecore_X_Randr_Output *outputs = NULL;
   int noutputs = 0;
   Ecore_X_Randr_Crtc *crtcs = NULL;
   int ncrtcs = 0, i = 0;

   /* grab the root window once */
   root = ecore_x_window_root_first_get();

   /* loop the config crtcs and set non-existing */
   EINA_LIST_FOREACH(e_randr_cfg->crtcs, l, crtc_cfg)
      crtc_cfg->exists = EINA_FALSE;

   /* loop the config outputs and set non-existing */
   EINA_LIST_FOREACH(e_randr_cfg->outputs, l, output_cfg)
      output_cfg->exists = EINA_FALSE;

   /* try to get the list of outputs from x */
   if ((outputs = ecore_x_randr_outputs_get(root, &noutputs)))
     {
        int j = 0;

        for (j = 0; j < noutputs; j++)
          {
             output_cfg = _e_randr_config_output_find(outputs[j]);
             if (!output_cfg)
               {
                  /* try to create new output config */
                  if (!(output_cfg = _e_randr_config_output_new(outputs[j])))
                    continue;
               }

             output_cfg->exists = EINA_TRUE;
             output_cfg->name = ecore_x_randr_output_name_get(root, output_cfg->xid, NULL);
             output_cfg->is_lid = _e_randr_is_lid(output_cfg);
          }

        free(outputs);
     }
   /* sort list by output id */
   e_randr_cfg->outputs = eina_list_sort(e_randr_cfg->outputs, -1, _e_randr_config_output_cmp);

   /* try to get the list of crtcs from x */
   if ((crtcs = ecore_x_randr_crtcs_get(root, &ncrtcs)))
     {
        /* loop the crtcs */
        for (i = 0; i < ncrtcs; i++)
          {
             /* get config for this crtc */
             crtc_cfg = _e_randr_config_crtc_find(crtcs[i]);
             if (!crtc_cfg)
               {
                  /* try to create new crtc config */
                  if (!(crtc_cfg = _e_randr_config_crtc_new(crtcs[i])))
                    continue;
               }
             /* get current values from X */
             _e_randr_config_crtc_update(crtc_cfg);
             crtc_cfg->exists = EINA_TRUE;

             /* try to get any outputs on this crtc */
             if ((outputs = ecore_x_randr_crtc_outputs_get(root, crtcs[i], &noutputs)))
               {
                  int j = 0;

                  for (j = 0; j < noutputs; j++)
                    {
                       output_cfg = _e_randr_config_output_find(outputs[j]);
                       if (!output_cfg)
                         {
                            /* this shouldn't be possible */
                            fprintf(stderr, "E_RANDR: Error, couldn't find output config for output: %d\n", outputs[j]);
                            continue;
                         }
                       /* we only want connected outputs on the crtc */
                       if (!output_cfg->connected) continue;

                       /* add this output to the list for this crtc */
                       crtc_cfg->outputs =
                          eina_list_append(crtc_cfg->outputs, output_cfg);
                       fprintf(stderr, "len outputs: %d %d\n", crtc_cfg->xid, eina_list_count(crtc_cfg->outputs));

                       /* assign crtc for this output */
                       if ((output_cfg->crtc) && (output_cfg->crtc != crtcs[i]))
                         {
                            fprintf(stderr, "E_RANDR: output %d has changed crtc: %d -> %d\n",
                                   output_cfg->xid, output_cfg->crtc, crtcs[i]);
                         }
                       output_cfg->crtc = crtcs[i];
                       e_randr_cfg->connected++;

                       /* get orientation from crtc if not set */
                       if (!output_cfg->orient)
                         output_cfg->orient = crtc_cfg->orient;
                       /* find mode for output */
                       _e_randr_config_output_mode_update(output_cfg);
                    }

                  free(outputs);
               }
          }

        free(crtcs);
     }

   /* find outputs which should be connected */
   EINA_LIST_FOREACH(e_randr_cfg->outputs, l, output_cfg)
     {
        if (!output_cfg->exists) continue;
        if (!output_cfg->connected) continue;
        /* we want to connect this output */
        _e_randr_output_connected_set(output_cfg, EINA_TRUE);
     }

   /* 
    * TODO:
    * If there are several outputs on one crtc, check if they have same mode/size
    * else move them to another crtc or disable
    */

   _e_randr_config_lid_update();

   /* update primary output */
   _e_randr_config_primary_update();

   /* update recorded config timestamp */
   e_randr_cfg->config_timestamp = ecore_x_randr_config_timestamp_get(root);

   /* save the config */
   e_randr_config_save();
}

static void
_e_randr_config_free(void)
{
   E_Randr_Crtc_Config *crtc = NULL;
   E_Randr_Output_Config *output = NULL;

   /* safety check */
   if (!e_randr_cfg) return;

   /* loop the config crtcs and free them */
   EINA_LIST_FREE(e_randr_cfg->crtcs, crtc)
     {
        eina_list_free(crtc->outputs);
        E_FREE(crtc);
     }

   /* loop the config outputs and free them */
   EINA_LIST_FREE(e_randr_cfg->outputs, output)
     {
        if (output->name) free(output->name);

        E_FREE(output);
     }

   /* free the config */
   E_FREE(e_randr_cfg);
}

static Eina_Bool
_e_randr_config_cb_timer(void *data)
{
   e_util_dialog_show(_("Randr Settings Upgraded"), "%s", (char *)data);
   return EINA_FALSE;
}

static void
_e_randr_config_apply(void)
{
   E_Randr_Crtc_Config *crtc_cfg;
   E_Randr_Output_Config *output_cfg;
   Eina_List *l, *ll;
   Ecore_X_Window root = 0;
   int cw = 0, ch = 0;
   int nw = 0, nh = 0;
   int minw = 0, minh = 0;
   int maxw = 0, maxh = 0;

   /* don't try to restore if we have fake screens */
   if (e_xinerama_fake_screens_exist()) return;

   fprintf(stderr, "E_RANDR: CONFIG RESTORE\n");

   /* grab the X server so that we can apply settings without triggering
    * any randr event updates until we are done */
   ecore_x_grab();

   /* get the root window */
   root = ecore_x_window_root_first_get();

   /* get existing screen size */
   ecore_x_randr_screen_current_size_get(root, &cw, &ch, NULL, NULL);

   /* get the min and max screen size */
   ecore_x_randr_screen_size_range_get(root, &minw, &minh, &maxw, &maxh);

   /* loop our lists of crtcs */
   EINA_LIST_FOREACH(e_randr_cfg->crtcs, l, crtc_cfg)
     {
        int x = 0, y = 0, w = 0, h = 0;
        Ecore_X_Randr_Mode mode = 0;
        Ecore_X_Randr_Orientation orient = ECORE_X_RANDR_ORIENTATION_ROT_0;
        Eina_Rectangle rect;
        int count = 0;
        Ecore_X_Randr_Output *coutputs;

        /* firstly, disable any crtcs which are disabled in our config OR
         * which are larger than the target size */
        _e_randr_config_crtc_from_outputs_set(crtc_cfg);

        x = crtc_cfg->geo.x;
        y = crtc_cfg->geo.y;
        mode = crtc_cfg->mode;
        orient = crtc_cfg->orient;

        /* at this point, we should have geometry, mode and orientation.
         * we can now proceed to calculate crtc size */
        rect = crtc_cfg->geo;
        _e_randr_config_mode_geometry(orient, &rect);
        w = rect.w;
        h = rect.h;

        /* if the crtc does not fit, or is disabled in config, disable it */
        if (((x + w) > maxw) || ((y + h) > maxh) || (mode == 0) || (!crtc_cfg->outputs))
          {
             fprintf(stderr, "disable crtc: %d\n", crtc_cfg->xid);
             fprintf(stderr, "\t%d > %d\n", (x + w), maxw);
             fprintf(stderr, "\t%d > %d\n", (y + h), maxh);
             fprintf(stderr, "\t%d\n", mode);
             fprintf(stderr, "\t%p\n", crtc_cfg->outputs);
             ecore_x_randr_crtc_settings_set(root, crtc_cfg->xid, NULL, 0, 0, 0, 0,
                                             ECORE_X_RANDR_ORIENTATION_ROT_0);
             continue;
          }

        /* find outputs to enable on crtc */
        coutputs = calloc(eina_list_count(crtc_cfg->outputs), sizeof(Ecore_X_Randr_Output));
        if (!coutputs)
          {
             /* TODO: ERROR! */
             continue;
          }
        EINA_LIST_FOREACH(crtc_cfg->outputs, ll, output_cfg)
          {
             coutputs[count] = output_cfg->xid;
             count++;
          }

        /* get new size */
        if ((x + w) > nw) nw = x + w;
        if ((y + h) > nh) nh = y + h;

        /* apply our stored crtc settings */
        fprintf(stderr, "enable crtc: %d %d\n", crtc_cfg->xid, count);
        fprintf(stderr, "\t%dx%d+%d+%d\n", crtc_cfg->geo.w, crtc_cfg->geo.h, crtc_cfg->geo.x, crtc_cfg->geo.y);
        fprintf(stderr, "\t%d %d\n", crtc_cfg->mode, crtc_cfg->orient);
        ecore_x_randr_crtc_settings_set(root, crtc_cfg->xid, coutputs,
                                        count, crtc_cfg->geo.x, crtc_cfg->geo.y,
                                        crtc_cfg->mode, crtc_cfg->orient);

        /* cleanup */
        free(coutputs);
     }

   /* apply the new screen size */
   fprintf(stderr, "size: (%dx%d) -> (%dx%d)\n", cw, ch, nw, nh);
   ecore_x_randr_screen_current_size_set(root, nw, nh, -1, -1);
   if ((nw != cw) || (nh != ch))
     {
        /* this sets screen size and moves screen to (0, 0) */
        ecore_x_randr_screen_reset(root);

        e_randr_cfg->screen.width = nw;
        e_randr_cfg->screen.height = nh;
        e_randr_config_save();
     }

   /* release the server grab */
   ecore_x_ungrab();
}

static Eina_Bool
_e_randr_event_cb_screen_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Screen_Change *ev;
   Eina_Bool changed = EINA_FALSE;
   Ecore_X_Randr_Output primary = 0;

   fprintf(stderr, "E_RANDR: _e_randr_event_cb_screen_change\n");
   ev = event;

   /* check if this event's root window is Our root window */
   if (ev->root != e_manager_current_get()->root)
     return ECORE_CALLBACK_RENEW;

   primary = ecore_x_randr_primary_output_get(ev->root);

   if (e_randr_cfg->primary != primary)
     {
        E_Randr_Output_Config *cfg;

        e_randr_cfg->primary = primary;
        changed = EINA_TRUE;
     }

   if (e_randr_cfg->screen.width != ev->size.width)
     {
        e_randr_cfg->screen.width = ev->size.width;
        changed = EINA_TRUE;
     }

   if (e_randr_cfg->screen.height != ev->size.height)
     {
        e_randr_cfg->screen.height = ev->size.height;
        changed = EINA_TRUE;
     }

   if (e_randr_cfg->config_timestamp != ev->config_time)
     {
        e_randr_cfg->config_timestamp = ev->config_time;
        changed = EINA_TRUE;
     }

   fprintf(stderr, "E_RANDR: changed: %d, width: %d, height: %d\n", changed, e_randr_cfg->screen.width, e_randr_cfg->screen.height);

   if (changed) e_randr_config_save();

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_randr_event_cb_crtc_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Randr_Crtc_Change *ev;
   E_Randr_Crtc_Config *crtc_cfg;

   fprintf(stderr, "E_RANDR: _e_randr_event_cb_crtc_change\n");
   ev = event;

   crtc_cfg = _e_randr_config_crtc_find(ev->crtc);

   if (!crtc_cfg)
     {
        fprintf(stderr, "E_RANDR: Weird, a new crtc?\n");
     }
   else
     {
        /* check (and update if needed) our crtc config */
        if ((ev->mode != crtc_cfg->mode) ||
            (ev->orientation != crtc_cfg->orient) ||
            (ev->geo.x != crtc_cfg->geo.x) || (ev->geo.y != crtc_cfg->geo.y) ||
            (ev->geo.w != crtc_cfg->geo.w) || (ev->geo.h != crtc_cfg->geo.h))
          {
             crtc_cfg->mode = ev->mode;
             crtc_cfg->orient = ev->orientation;
             crtc_cfg->geo = ev->geo;

             /* propagate changes to stored outputs */
             _e_randr_config_outputs_from_crtc_set(crtc_cfg);
             /* save config */
             e_randr_config_save();
          }
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_randr_event_cb_output_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Randr_Output_Change *ev;
   E_Randr_Output_Config *output_cfg;
   Eina_Bool output_new = EINA_FALSE;
   Eina_Bool output_changed = EINA_FALSE;
   Eina_Bool output_removed = EINA_FALSE;

   fprintf(stderr, "E_RANDR: _e_randr_event_cb_output_change\n");
   ev = event;

   /* check if this event's root window is Our root window */
   if (ev->win != e_manager_current_get()->root)
     return ECORE_CALLBACK_RENEW;

   /* loop our crtcs and try to find this output */
   output_cfg = _e_randr_config_output_find(ev->output);
   if (!output_cfg)
     {
        fprintf(stderr, "E_RANDR: Weird, a new output?\n");
     }
   else
     {
        /* we know this output */
        if (output_cfg->is_lid && _e_randr_lid_is_closed)
          {
             /* ignore event from disconnected lid */
             fprintf(stderr, "ignore event from closed lid\n");
          }
        else if (ev->connection == ECORE_X_RANDR_CONNECTION_STATUS_CONNECTED)
          {
             /* connected */
             if ((ev->crtc != 0) && (output_cfg->crtc == ev->crtc))
               {
                  if (!output_cfg->connected)
                    {
                       fprintf(stderr, "E_RANDR: Output On Same Crtc\n");

                       /* connect to crtc */
                       _e_randr_output_connected_set(output_cfg, EINA_TRUE);
                       /* validate output mode */
                       _e_randr_config_output_mode_update(output_cfg);
                       output_new = EINA_TRUE;
                    }
               }
             else if (ev->crtc == 0)
               {
                  E_Randr_Crtc_Config *crtc_cfg = NULL;

                  fprintf(stderr, "E_RANDR: Output Without Crtc\n");
                  /* try to find old config */
                  if (output_cfg->crtc)
                    crtc_cfg = _e_randr_config_crtc_find(output_cfg->crtc);
                  /* else try to allocate new crtc */
                  if (!crtc_cfg)
                    crtc_cfg = _e_randr_config_output_crtc_find(output_cfg);
                  if (crtc_cfg)
                    {
                       output_cfg->crtc = crtc_cfg->xid;
                       /* connect to crtc */
                       _e_randr_output_connected_set(output_cfg, EINA_TRUE);
                       /* get orientation from crtc if not set */
                       if (!output_cfg->orient)
                         output_cfg->orient = crtc_cfg->orient;
                       /* validate output mode */
                       _e_randr_config_output_mode_update(output_cfg);
                       output_new = EINA_TRUE;
                       fprintf(stderr, "Connected to new crtc: %d (%dx%d+%d+%d)\n",
                               output_cfg->crtc, output_cfg->geo.w, output_cfg->geo.h,
                               output_cfg->geo.x, output_cfg->geo.y);
                    }
               }
             else
               {
                  fprintf(stderr, "E_RANDR: Output With New Crtc\n");
                  /* remove from old crtc */
                  _e_randr_output_connected_set(output_cfg, EINA_FALSE);
                  /* add to new crtc */
                  output_cfg->crtc = ev->crtc;
                  _e_randr_output_connected_set(output_cfg, EINA_TRUE);

                  /* validate output mode */
                  _e_randr_config_output_mode_update(output_cfg);
                  output_new = EINA_TRUE;
               }
          }
        else if (ev->connection == ECORE_X_RANDR_CONNECTION_STATUS_DISCONNECTED)
          {
             /* disconnected */
             if (output_cfg->connected)
               {
                  fprintf(stderr, "E_RANDR: Output Disconnected: %d\n", output_cfg->crtc);
                  _e_randr_output_connected_set(output_cfg, EINA_FALSE);
                  output_removed = EINA_TRUE;
               }
          }
     }

   /* save the config if anything changed or we added a new one */
   if ((output_changed) || (output_new) || (output_removed))
     {
        fprintf(stderr, "E_RANDR: Output Changed, Added, or Removed. Saving Config\n");
        e_randr_config_save();
     }

   /* if we added or removed any outputs, apply config */
   if ((output_new) || (output_removed))
     {
        _e_randr_config_primary_update();
        _e_randr_config_apply();
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_randr_event_cb_acpi(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Acpi *ev;

   fprintf(stderr, "E_RANDR: _e_randr_event_cb_acpi\n");
   ev = event;

   if (ev->type == E_ACPI_TYPE_LID)
     {
        Eina_Bool changed;

        _e_randr_lid_is_closed = (ev->status == E_ACPI_LID_CLOSED);
        changed = _e_randr_config_lid_update();
        fprintf(stderr, "ch: %d\n", changed);
        if (changed)
          {
             /* force new evaluation of primary */
             e_randr_cfg->primary = 0;
             _e_randr_config_primary_update();
             _e_randr_config_apply();
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_randr_config_output_mode_update(E_Randr_Output_Config *cfg)
{
   Ecore_X_Window root = 0;
   Ecore_X_Randr_Mode *modes = NULL;
   int nmodes = 0, pref = 0;
   Ecore_X_Randr_Mode_Info **mode_infos = NULL;
   int nmode_infos = 0;
   int i = 0;

   /* grab the root window */
   root = ecore_x_window_root_first_get();

   /* get mode infos */
   mode_infos = ecore_x_randr_modes_info_get(root, &nmode_infos);
   if (nmode_infos == 0)
     goto error;
   /* get the list of modes for this output */
   modes = ecore_x_randr_output_modes_get(root, cfg->xid, &nmodes, &pref);
   if (nmodes == 0)
     goto error;

   /* try to find the matching mode */
   cfg->mode = 0;
   if ((cfg->geo.w != 0) && (cfg->geo.h != 0))
     {
        for (i = 0; i < nmode_infos; i++)
          {
             if ((mode_infos[i]->width == cfg->geo.w) &&
                 (mode_infos[i]->height == cfg->geo.h))
               {
                  cfg->mode = mode_infos[i]->xid;
                  break;
               }
          }
        /* check if mode is available */
        if (!_e_randr_output_mode_valid(cfg->mode, modes, nmodes))
          cfg->mode = 0;
     }

   /* see if we can use the mode of the crtc */
   if (!cfg->mode)
     {
        E_Randr_Crtc_Config *crtc_cfg;

        crtc_cfg = _e_randr_config_crtc_find(cfg->crtc);
        if (crtc_cfg && crtc_cfg->mode)
          {
             if (_e_randr_output_mode_valid(crtc_cfg->mode, modes, nmodes))
               cfg->mode = crtc_cfg->mode;
             /* TODO: See if we have a mode of the same size with another mode id */
          }
     }

   /* set preferred mode */
   if (!cfg->mode)
     {
        if (pref > 0)
          cfg->mode = modes[pref - 1];
        else
          cfg->mode = modes[0];
     }

   for (i = 0; i < nmode_infos; i++)
     {
        if (mode_infos[i]->xid == cfg->mode)
          {
             cfg->geo.w = mode_infos[i]->width;
             cfg->geo.h = mode_infos[i]->height;
             break;
          }
     }

   if (modes) free(modes);
   if (mode_infos)
     {
        for (i = 0; i < nmode_infos; i++)
          free(mode_infos[i]);
        free(mode_infos);
     }
   return;
error:
   if (modes) free(modes);
   if (mode_infos)
     {
        for (i = 0; i < nmode_infos; i++)
          free(mode_infos[i]);
        free(mode_infos);
     }
   fprintf(stderr, "reset: %d = %dx%d+%d+%d\n",
           cfg->crtc,
           cfg->geo.w, cfg->geo.h,
           cfg->geo.x, cfg->geo.y);
   cfg->geo.x = cfg->geo.y = cfg->geo.w = cfg->geo.h = 0;
   cfg->mode = 0;
   return;
}

static E_Randr_Crtc_Config *
_e_randr_config_crtc_new(unsigned int id)
{
   E_Randr_Crtc_Config *cfg = NULL;

   if ((cfg = E_NEW(E_Randr_Crtc_Config, 1)))
     {
        /* assign output xid */
        cfg->xid = id;

        /* append this crtc config to randr config */
        e_randr_cfg->crtcs =
           eina_list_append(e_randr_cfg->crtcs, cfg);
     }

   return cfg;
}

static void
_e_randr_config_crtc_update(E_Randr_Crtc_Config *cfg)
{
   Ecore_X_Window root = 0;
   Ecore_X_Randr_Crtc_Info *cinfo;

   /* grab the root window */
   root = ecore_x_window_root_first_get();

   /* get crtc info from X */
   if ((cinfo = ecore_x_randr_crtc_info_get(root, cfg->xid)))
     {
        cfg->geo.x = cinfo->x;
        cfg->geo.y = cinfo->y;
        cfg->geo.w = cinfo->width;
        cfg->geo.h = cinfo->height;
        cfg->mode = cinfo->mode;
        cfg->orient = cinfo->rotation;

        ecore_x_randr_crtc_info_free(cinfo);
     }
}

static E_Randr_Output_Config *
_e_randr_config_output_new(unsigned int id)
{
   E_Randr_Output_Config *cfg = NULL;

   if ((cfg = E_NEW(E_Randr_Output_Config, 1)))
     {
        Ecore_X_Window root = 0;
        Ecore_X_Randr_Connection_Status status;

        /* assign output xid */
        cfg->xid = id;

        /* grab the root window */
        root = ecore_x_window_root_first_get();

        /* get the crtc for this output */
        cfg->crtc = ecore_x_randr_output_crtc_get(root, cfg->xid);

        /* check if connected */
        status = ecore_x_randr_output_connection_status_get(root, cfg->xid);
        cfg->connected = (status == ECORE_X_RANDR_CONNECTION_STATUS_CONNECTED);

        /* append this output config to randr config */
        e_randr_cfg->outputs =
           eina_list_append(e_randr_cfg->outputs, cfg);
     }

   return cfg;
}

static E_Randr_Crtc_Config *
_e_randr_config_crtc_find(Ecore_X_Randr_Crtc crtc)
{
   Eina_List *l;
   E_Randr_Crtc_Config *crtc_cfg;

   EINA_LIST_FOREACH(e_randr_cfg->crtcs, l, crtc_cfg)
     {
        if (crtc_cfg->xid == crtc)
          return crtc_cfg;
     }

   return NULL;
}

static E_Randr_Output_Config *
_e_randr_config_output_find(Ecore_X_Randr_Output output)
{
   Eina_List *l;
   E_Randr_Output_Config *output_cfg;

   EINA_LIST_FOREACH(e_randr_cfg->outputs, l, output_cfg)
     {
        if (output_cfg->xid == output)
          return output_cfg;
     }

   return NULL;
}


static E_Randr_Crtc_Config *
_e_randr_config_output_crtc_find(E_Randr_Output_Config *cfg)
{
   Ecore_X_Window root = 0;
   E_Randr_Crtc_Config *crtc_cfg = NULL;
   Ecore_X_Randr_Crtc *possible = NULL;
   Ecore_X_Randr_Mode *modes = NULL;
   int num = 0, i = 0;
   int nmodes, pref;

   /* grab the root window */
   root = ecore_x_window_root_first_get();

   /* get a list of possible crtcs for this output */
   possible = ecore_x_randr_output_possible_crtcs_get(root, cfg->xid, &num);
   if (num == 0) goto error;

   /* loop the possible crtcs */
   for (i = 0; i < num; i++)
     {
	if ((crtc_cfg = _e_randr_config_crtc_find(possible[i])))
	  {
             if (!crtc_cfg->outputs)
               goto done;
          }
     }
   crtc_cfg = NULL;

   /* get the list of modes for this output */
   modes = ecore_x_randr_output_modes_get(root, cfg->xid, &nmodes, &pref);
   if (nmodes == 0)
     goto error;

   /* can we clone this output */
   for (i = 0; i < num; i++)
     {
        if ((crtc_cfg = _e_randr_config_crtc_find(possible[i])))
          {
             /* check if mode is valid for this output */
             if (_e_randr_output_mode_valid(crtc_cfg->mode, modes, nmodes))
               goto done;
          }
     }
   crtc_cfg = NULL;

done:
   free(possible);
   free(modes);

   return crtc_cfg;

error:
   free(possible);
   free(modes);
   return NULL;
}

static void
_e_randr_config_mode_geometry(Ecore_X_Randr_Orientation orient, Eina_Rectangle *rect)
{
   int mw = 0, mh = 0;

   /* based on orientation, calculate mode sizes */
   switch (orient)
     {
      case ECORE_X_RANDR_ORIENTATION_ROT_0:
      case ECORE_X_RANDR_ORIENTATION_ROT_180:
        mw = rect->w;
        mh = rect->h;
        break;
      case ECORE_X_RANDR_ORIENTATION_ROT_90:
      case ECORE_X_RANDR_ORIENTATION_ROT_270:
        mw = rect->h;
        mh = rect->w;
        break;
      default:
        break;
     }

   rect->x = 0;
   rect->y = 0;
   rect->w = mw;
   rect->h = mh;
}

static void
_e_randr_config_primary_update(void)
{
   Ecore_X_Window root = 0;
   Eina_List *l;
   E_Randr_Output_Config *output_cfg;
   Ecore_X_Randr_Output primary = 0;

   /* check if we agree with system setup */
   root = ecore_x_window_root_first_get();
   primary = ecore_x_randr_primary_output_get(root);
   /* see if our selected primary exists and is connected */
   output_cfg = _e_randr_config_output_find(e_randr_cfg->primary);
   if ((!output_cfg) || (!output_cfg->exists) || (!output_cfg->connected))
     e_randr_cfg->primary = 0;

   /* check if system primary is the same as ours */
   if ((primary > 0) && (primary == e_randr_cfg->primary)) return;

   if ((e_randr_cfg->primary > 0) &&
       output_cfg && output_cfg->exists && output_cfg->connected)
     {
        /* prefer our primary */
        ecore_x_randr_primary_output_set(root, e_randr_cfg->primary);
     }
   else
     {
        /* find lid */
        e_randr_cfg->primary = 0;
        EINA_LIST_FOREACH(e_randr_cfg->outputs, l, output_cfg)
          {
             if (output_cfg->is_lid)
               {
                  if (output_cfg->exists && output_cfg->connected)
                    e_randr_cfg->primary = output_cfg->xid;
                  break;
               }
          }
        if (!e_randr_cfg->primary)
          {
             /* TODO: prefer top-left active monitor */
             /* no lid, use first existing */
             EINA_LIST_FOREACH(e_randr_cfg->outputs, l, output_cfg)
               {
                  if (output_cfg->exists && output_cfg->connected)
                    {
                       e_randr_cfg->primary = output_cfg->xid;
                       break;
                    }
               }
          }
        /* set primary */
        ecore_x_randr_primary_output_set(root, e_randr_cfg->primary);
     }

   output_cfg = _e_randr_config_output_find(e_randr_cfg->primary);
   fprintf(stderr, "primary output: %d %s\n", e_randr_cfg->primary, output_cfg->name);
}

static void
_e_randr_acpi_handler_add(void *data EINA_UNUSED)
{
   E_LIST_HANDLER_APPEND(_randr_event_handlers,
                         E_EVENT_ACPI,
                         _e_randr_event_cb_acpi, NULL);
}

static int
_e_randr_is_lid(E_Randr_Output_Config *cfg)
{
   /* TODO: ecore_x_randr_output_connector_type_get */
   int ret = 0;

   if (!cfg->name) return 0;
   if (strstr(cfg->name, "LVDS")) ret = 1;
   else if (strstr(cfg->name, "lvds")) ret = 1;
   else if (strstr(cfg->name, "Lvds")) ret = 1;
   else if (strstr(cfg->name, "LCD")) ret = 1;
   return ret;
}

static void
_e_randr_config_outputs_from_crtc_set(E_Randr_Crtc_Config *crtc_cfg)
{
   E_Randr_Output_Config *output_cfg;
   Eina_List *l;

   EINA_LIST_FOREACH(crtc_cfg->outputs, l, output_cfg)
     {
        output_cfg->mode = crtc_cfg->mode;
        output_cfg->orient = crtc_cfg->orient;
        output_cfg->geo = crtc_cfg->geo;
        fprintf(stderr, "set from crtc: %d = %dx%d+%d+%d\n",
                crtc_cfg->xid,
                output_cfg->geo.w, output_cfg->geo.h,
                output_cfg->geo.x, output_cfg->geo.y);
     }
}

static void
_e_randr_config_crtc_from_outputs_set(E_Randr_Crtc_Config *crtc_cfg)
{
   E_Randr_Output_Config *output_cfg;
   Eina_List *l;

   EINA_LIST_FOREACH(crtc_cfg->outputs, l, output_cfg)
     {
        /* TODO: Match all connected outputs, not only the first */
        crtc_cfg->mode = output_cfg->mode;
        crtc_cfg->orient = output_cfg->orient;
        crtc_cfg->geo = output_cfg->geo;
        break;
     }
}

static Eina_Bool
_e_randr_config_lid_update(void)
{
   E_Randr_Output_Config *output_cfg;
   Eina_List *l;
   Eina_Bool changed = EINA_FALSE;

   /* loop through connections to find lid */
   changed = EINA_FALSE;
   EINA_LIST_FOREACH(e_randr_cfg->outputs, l, output_cfg)
     {
        if (!output_cfg->is_lid) continue;
        fprintf(stderr, "hm: %d %d %d %d\n", _e_randr_lid_is_closed, output_cfg->exists, output_cfg->connected, e_randr_cfg->connected);
        if (_e_randr_lid_is_closed && output_cfg->exists && output_cfg->connected)
          {
             /* only disable lid if we got more than 1 connected output */
             if (e_randr_cfg->connected > 1)
               {
                  _e_randr_output_connected_set(output_cfg, EINA_FALSE);
                  changed = EINA_TRUE;
               }
          }
        else if (!output_cfg->connected)
          {
             _e_randr_output_connected_set(output_cfg, EINA_TRUE);
             changed = EINA_TRUE;
          }
     }

   return changed;
}

static Eina_Bool
_e_randr_output_mode_valid(Ecore_X_Randr_Mode mode, Ecore_X_Randr_Mode *modes, int nmodes)
{
   Eina_Bool valid = EINA_FALSE;
   int i;

   for (i = 0; i < nmodes; i++)
     {
        if (modes[i] == mode)
          {
             valid = EINA_TRUE;
             break;
          }
     }
   return valid;
}

static void
_e_randr_output_connected_set(E_Randr_Output_Config *cfg, Eina_Bool connected)
{
   E_Randr_Crtc_Config *crtc_cfg;

   if (cfg->connected == connected) return;
   cfg->connected = connected;

   crtc_cfg = _e_randr_config_crtc_find(cfg->crtc);
   if (crtc_cfg)
     {
        if (connected)
          {
             if (eina_list_data_find(crtc_cfg->outputs, cfg))
               fprintf(stderr, "E_RANDR: Already connected\n");
             else
               {
                  crtc_cfg->outputs =
                     eina_list_append(crtc_cfg->outputs, cfg);
                  e_randr_cfg->connected++;
               }
          }
        else
          {
             crtc_cfg->outputs =
                eina_list_remove(crtc_cfg->outputs, cfg);
             e_randr_cfg->connected--;
          }
        fprintf(stderr, "len outputs: %d %d\n", crtc_cfg->xid, eina_list_count(crtc_cfg->outputs));
     }

}

static int
_e_randr_config_output_cmp(const void *a, const void *b)
{
   const E_Randr_Output_Config *cfg1 = a;
   const E_Randr_Output_Config *cfg2 = b;

   if (cfg1->xid < cfg2->xid) return -1;
   if (cfg1->xid > cfg2->xid) return 1;
   return 0;
}

