#ifdef E_TYPEDEFS

typedef struct _E_Randr2        E_Randr2;
typedef struct _E_Randr2_Screen E_Randr2_Screen;
typedef struct _E_Randr2_Mode   E_Randr2_Mode;

typedef struct _E_Config_Randr2        E_Config_Randr2;
typedef struct _E_Config_Randr2_Screen E_Config_Randr2_Screen;

#else
#ifndef E_RANDR2_H
#define E_RAND2R_H

#define E_RANDR_VERSION_1_1 ((1 << 16) | 1)
#define E_RANDR_VERSION_1_2 ((1 << 16) | 2)
#define E_RANDR_VERSION_1_3 ((1 << 16) | 3)
#define E_RANDR_VERSION_1_4 ((1 << 16) | 4)

typedef enum _E_Randr2_Relative
{
   E_RANDR2_RELATIVE_UNKNOWN,
   E_RANDR2_RELATIVE_NONE,
   E_RANDR2_RELATIVE_CLONE,
   E_RANDR2_RELATIVE_TO_LEFT,
   E_RANDR2_RELATIVE_TO_RIGHT,
   E_RANDR2_RELATIVE_TO_ABOVE,
   E_RANDR2_RELATIVE_TO_BELOW
} E_Randr2_Relative;

typedef enum _E_Randr2_Connector
{
   E_RANDR2_CONNECTOR_UNDEFINED,
   E_RANDR2_CONNECTOR_DVI,
   E_RANDR2_CONNECTOR_HDMI_A,
   E_RANDR2_CONNECTOR_HDMI_B,
   E_RANDR2_CONNECTOR_MDDI,
   E_RANDR2_CONNECTOR_DISPLAY_PORT
} E_Randr2_Connector;

struct _E_Randr2
{
   Eina_List *screens; // available screens
   int        w, h; // virtual resolution needed for screens (calculated)
};

struct _E_Randr2_Mode
{
   int    w, h; // resolution width and height
   double refresh; // refresh in hz
   Eina_Bool preferred : 1; // is this the preferred mode for the device?
};

struct _E_Randr2_Screen
{
   char *id; // string id which is "name/edid";
   struct {
      char                 *screen; // name of the screen device attached
      char                 *name; // name of the output itself
      char                 *edid; // full edid data
      E_Randr2_Connector    connector; // the connector type
      Eina_Bool             is_lid : 1; // is an internal screen
      Eina_Bool             lid_closed : 1; // is lid closed when screen qury'd
      Eina_Bool             connected : 1; // some screen is plugged in or not
      Eina_Bool             backlight : 1; // does it have backlight controls?
      Eina_Bool             can_rot_0 : 1; // can it do this rotation?
      Eina_Bool             can_rot_90 : 1; // can it do this rotation?
      Eina_Bool             can_rot_180 : 1; // can it do this rotation?
      Eina_Bool             can_rot_270 : 1; // can it do this rotation?
      Eina_List            *modes; // available screen modes here
      struct {
         int                w, h; // physical width and height in mm
      } size;
   } info;
   struct {
      struct {
         E_Randr2_Relative  mode; // what relative mode to the screen below
         char              *to; // what screen this one is relative to
         double             align; // alignment along the edge
      } relative;
      Eina_Rectangle        geom; // the geometry that is set (as a result)
      E_Randr2_Mode         mode; // screen res/refresh to use
      int                   rotation; // 0, 90, 180, 270
      int                   priority; // larger num == more important
      Eina_Bool             enabled : 1; // should this monitor be enabled?
      Eina_Bool             configured : 1; // has screen been configured by e?
   } config;
};

struct _E_Config_Randr2
{
   int            version;
   Eina_List     *screens;
   unsigned char  restore;
   unsigned char  ignore_hotplug_events;
   unsigned char  ignore_acpi_events;
};

struct _E_Config_Randr2_Screen
{
   const char   *id;
   const char   *rel_to;
   double        rel_align;
   double        mode_refresh;
   int           mode_w;
   int           mode_h;
   int           rotation;
   int           priority;
   unsigned char rel_mode;
   unsigned char enabled;
};

extern E_API E_Config_Randr2 *e_randr2_cfg;
extern E_API E_Randr2        *e_randr2;

extern E_API int              E_EVENT_RANDR_CHANGE;

EINTERN Eina_Bool             e_randr2_init(void);
EINTERN int                   e_randr2_shutdown(void);

E_API    Eina_Bool            e_randr2_config_save(void);
E_API    void                 e_randr2_config_apply(void);
E_API    void                 e_randr2_screeninfo_update(void);

E_API void                    e_randr2_screen_refresh_queue(Eina_Bool lid_event);
E_API E_Config_Randr2_Screen *e_randr2_config_screen_find(E_Randr2_Screen *s, E_Config_Randr2 *cfg);
E_API void                    e_randr2_screens_setup(int rw, int rh);
E_API E_Randr2_Screen        *e_randr2_screen_id_find(const char *id);
E_API double                  e_randr2_screen_dpi_get(E_Randr2_Screen *s);

#endif
#endif
