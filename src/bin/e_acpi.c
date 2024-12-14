#include "e.h"

/* local structures */
/* for simple acpi device mapping */
typedef struct _E_ACPI_Device_Simple       E_ACPI_Device_Simple;
typedef struct _E_ACPI_Device_Simple_State E_ACPI_Device_Simple_State;

struct _E_ACPI_Device_Simple
{
   const char *name;
   // ->
   int         type;
};

struct _E_ACPI_Device_Simple_State
{
   const char *name;
   const char *bus;
   const char *state;
   // ->
   int         type;
};

/* local function prototypes */
static void      _e_acpi_cb_event_free(void *data EINA_UNUSED, void *event);
static int       _e_acpi_lid_status_get(const char *device, const char *bus);
static Eina_Bool _e_acpi_cb_event(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);

/* local variables */
static int _e_acpi_events_frozen = 0;
static Eina_List *_e_acpid_hdls = NULL;
static int lid_is_closed = -1;

static E_ACPI_Device_Simple _devices_simple[] =
{
   /* NB: DO NOT TRANSLATE THESE. */
//   {"ac_adapter", E_ACPI_TYPE_AC_ADAPTER},
//   {"battery", E_ACPI_TYPE_BATTERY},
   {"button/lid", E_ACPI_TYPE_LID},
   {"button/power", E_ACPI_TYPE_POWER},
   {"button/sleep", E_ACPI_TYPE_SLEEP},
   {"button/volumedown", E_ACPI_TYPE_VOLUME_DOWN},
   {"button/volumeup", E_ACPI_TYPE_VOLUME_UP},
   {"button/mute", E_ACPI_TYPE_MUTE},
   {"button/wlan", E_ACPI_TYPE_WIFI},
//   {"fan", E_ACPI_TYPE_FAN},
//   {"processor", E_ACPI_TYPE_PROCESSOR},
//   {"thermal_zone", E_ACPI_TYPE_THERMAL},
//   {"video", E_ACPI_TYPE_VIDEO},
   {"video/brightnessdown", E_ACPI_TYPE_BRIGHTNESS_DOWN},
   {"video/brightnessup", E_ACPI_TYPE_BRIGHTNESS_UP},
   {"video/switchmode", E_ACPI_TYPE_VIDEO},
   {"button/zoom", E_ACPI_TYPE_ZOOM},
   {"button/screenlock", E_ACPI_TYPE_SCREENLOCK},
   {"button/battery", E_ACPI_TYPE_BATTERY_BUTTON},
   {"video/tabletmode", E_ACPI_TYPE_TABLET},

   //bluetooth virtual input devices for A/V Remote Control
   {"cd/next", E_ACPI_TYPE_CD_NEXT},
   {"cd/prev", E_ACPI_TYPE_CD_PREV},
   {"cd/stop", E_ACPI_TYPE_CD_STOP},
   {"cd/play", E_ACPI_TYPE_CD_PLAY},

   {NULL, E_ACPI_TYPE_UNKNOWN}
};

static E_ACPI_Device_Simple_State _devices_simple_state[] =
{
   /* NB: DO NOT TRANSLATE THESE. */
   {"video/tabletmode", "TBLT", "on", E_ACPI_TYPE_TABLET_ON},
   {"video/tabletmode", "TBLT", "off", E_ACPI_TYPE_TABLET_OFF},

   {NULL, NULL, NULL, E_ACPI_TYPE_UNKNOWN}
};

/* public variables */
E_API int E_EVENT_ACPI = 0;

static void
_cb_sys_acpi_event(void *data EINA_UNUSED, const char *params)
{
  E_Event_Acpi                *acpi_event;
  int                          sig, status, i, done = 0;
  char                         device[1024], bus[1024], state[1024];

  printf("ACPI: [%s]\n", params);
  // parse out this acpi string into separate pieces
  if (sscanf(params, "%1023s %1023s %x %x", device, bus, &sig, &status) != 4)
    {
      sig    = -1;
      status = -1;
      if (sscanf(params, "%1023s %1023s", device, bus) != 2)
        {
          if (sscanf(params, "%1023s %1023s %1023s", device, bus, state) != 3)
            return;
        }
    }
  // create new event structure to raise
  acpi_event         = E_NEW(E_Event_Acpi, 1);
  acpi_event->bus_id = eina_stringshare_add(bus);
  acpi_event->signal = sig;
  acpi_event->status = status;

  if ((!done) && (state[0]))
    {
      for (i = 0; _devices_simple_state[i].name; i++)
        {
          if ((!strcmp(device, _devices_simple_state[i].name))
              && ((!_devices_simple_state[i].bus)
                  || (!strcmp(bus, _devices_simple_state[i].bus)))
              && (!strcmp(state, _devices_simple_state[i].state)))
            {
              acpi_event->type = _devices_simple_state[i].type;
              done             = 1;
              break;
            }
        }
    }
  if (!done)
    {
      // if device name matches
      for (i = 0; _devices_simple[i].name; i++)
        {
          if (!strcmp(device, _devices_simple[i].name))
            {
              acpi_event->type = _devices_simple[i].type;
              done             = 1;
              break;
            }
        }
    }
  if (!done) free(acpi_event);
  else
    {
      switch (acpi_event->type)
        {
        case E_ACPI_TYPE_LID:
          acpi_event->status = _e_acpi_lid_status_get(device, bus);
          printf("RRR: acpi event @%1.8f\n", ecore_time_get());
          /* no change in lid state */
          if (lid_is_closed == (acpi_event->status == E_ACPI_LID_CLOSED))
            break;
          lid_is_closed = (acpi_event->status == E_ACPI_LID_CLOSED);
          printf("RRR: lid event for lid %i\n", lid_is_closed);
          if (!e_randr2_cfg->ignore_acpi_events)
            e_randr2_screen_refresh_queue(EINA_TRUE);
          if (!lid_is_closed) e_powersave_defer_cancel();
          break;
         default:
          break;
        }
      /* actually raise the event */
      ecore_event_add(E_EVENT_ACPI, acpi_event, _e_acpi_cb_event_free,
                      NULL);
    }
}

static void
_e_acpi_cb_event_free(void *data EINA_UNUSED, void *event)
{
  E_Event_Acpi *ev = event;
  if (!ev) return;
  if (ev->device) eina_stringshare_del(ev->device);
  if (ev->bus_id) eina_stringshare_del(ev->bus_id);
  E_FREE(ev);
}

static int
_e_acpi_lid_status_get(const char *device, const char *bus)
{
  FILE *f;
  int   i = 0;
  char  buff[PATH_MAX], *ret;

  /* the acpi driver code in the kernel has a nice acpi function to return
   * the lid status easily, but that function is not exposed for user_space
   * so we need to check the proc fs to get the actual status */
  // make sure we have a device and bus
  if ((!device) || (!bus)) return E_ACPI_LID_UNKNOWN;
  // open the state file from /proc
  snprintf(buff, sizeof(buff), "/proc/acpi/%s/%s/state", device, bus);
  if (!(f = fopen(buff, "r")))
    {
      /* hack around ppurka's Thinkpad (G460 + Linux) that reports lid
       * state as "/proc/acpi/button/lid/LID0/state" but where the lid
       * event says "button/lid LID close".
       *
       * so let's take the base device name "LID" and add a numeric like
       * 0, 1, 2, 3 so we have LID0, LID1, LID2 etc. - try up to LID9
       * and then give up.
       */
      for (i = 0; i < 10; i++)
        {
          snprintf(buff, sizeof(buff), "/proc/acpi/%s/%s%i/state",
                   device, bus, i);
          if ((f = fopen(buff, "r"))) break;
          f = NULL;
        }
      if (!f) return E_ACPI_LID_UNKNOWN;
  }
  // read the line from state file
  ret = fgets(buff, sizeof(buff), f);
  fclose(f);
  if (!ret) return E_ACPI_LID_UNKNOWN;
  // parse out state file
  i = 0;
  while (buff[i] != ':') i++;
  while (!isalnum(buff[i])) i++;
  ret = &(buff[i]);
  while (isalnum(buff[i])) i++;
  buff[i] = 0;
  // compare value from state file and return something sane
  if (!strcmp(ret, "open")) return E_ACPI_LID_OPEN;
  else if (!strcmp(ret, "closed")) return E_ACPI_LID_CLOSED;
  else return E_ACPI_LID_UNKNOWN;
}

/* public functions */
EINTERN int
e_acpi_init(void)
{
   E_EVENT_ACPI = ecore_event_type_new();

   e_system_handler_add("acpi-event", _cb_sys_acpi_event, NULL);
   // Add handlers for standard acpi events
   _e_acpid_hdls =
     eina_list_append(_e_acpid_hdls,
                      ecore_event_handler_add(E_EVENT_ACPI,
                                              _e_acpi_cb_event, NULL));
   return 1;
}

EINTERN int
e_acpi_shutdown(void)
{
   Ecore_Event_Handler *hdl;

   /* cleanup event handlers */
   EINA_LIST_FREE(_e_acpid_hdls, hdl)
     ecore_event_handler_del(hdl);
   return 1;
}

EINTERN E_Acpi_Lid_Status
e_acpi_lid_status_get(void)
{
   int i;

   for (i = 0; _devices_simple[i].name; i++)
     {
        if (_devices_simple[i].type == E_ACPI_TYPE_LID)
          {
             /* TODO: Can bus be anything other than LID? */
             return _e_acpi_lid_status_get(_devices_simple[i].name, "LID");
          }
     }

   return E_ACPI_LID_UNKNOWN;
}

E_API Eina_Bool
e_acpi_lid_is_closed(void)
{
   if (lid_is_closed == -1)
     lid_is_closed = (e_acpi_lid_status_get() == E_ACPI_LID_CLOSED);
   return lid_is_closed;
}

E_API void
e_acpi_events_freeze(void)
{
   _e_acpi_events_frozen++;
}

E_API void
e_acpi_events_thaw(void)
{
   _e_acpi_events_frozen--;
   if (_e_acpi_events_frozen < 0) _e_acpi_events_frozen = 0;
}

static Eina_Bool
_e_acpi_cb_event(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Acpi *ev;

   ev = event;
   if (_e_acpi_events_frozen > 0) return ECORE_CALLBACK_PASS_ON;
   e_bindings_acpi_event_handle(E_BINDING_CONTEXT_NONE, NULL, ev);
   return ECORE_CALLBACK_PASS_ON;
}

