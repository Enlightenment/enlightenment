#ifdef E_TYPEDEFS

typedef struct _E_Config_Randr_Output E_Config_Randr_Output;
typedef struct _E_Config_Randr E_Config_Randr;

typedef struct _E_Randr_Output E_Randr_Output;
typedef struct _E_Randr_Crtc E_Randr_Crtc;
typedef struct _E_Randr E_Randr;

#else
# ifndef E_RANDR_H
#  define E_RANDR_H

#define E_RANDR_VERSION_1_1 ((1 << 16) | 1)
#define E_RANDR_VERSION_1_2 ((1 << 16) | 2)
#define E_RANDR_VERSION_1_3 ((1 << 16) | 3)
#define E_RANDR_VERSION_1_4 ((1 << 16) | 4)

#define E_RANDR_CONFIG_FILE_EPOCH 5
#define E_RANDR_CONFIG_FILE_GENERATION 1
#define E_RANDR_CONFIG_FILE_VERSION \
   ((E_RANDR_CONFIG_FILE_EPOCH * 1000000) + E_RANDR_CONFIG_FILE_GENERATION)

struct _E_Config_Randr_Output
{
   const char *name;
   const char *edid;

   unsigned int orient; // value of the ecore_x_randr_orientation
   Eina_Rectangle geo;  // geometry
   double refresh_rate; // calculated refresh rate
   Eina_Bool connect;   // does the user want this output connected
};

struct _E_Config_Randr
{
   int version; // INTERNAL CONFIG VERSION

   Eina_List *outputs;

   unsigned char restore;
   unsigned long config_timestamp;
   unsigned int primary;
};

struct _E_Randr_Output
{
   unsigned int xid;  // ecore_x_randr output id (xid)
   unsigned int mode; // ecore_x_randr mode id (xid)
   char *name;        // name of output
   char *edid;        // edid as a hex string
   Eina_Bool is_lid;  // is this a laptop panel
   Eina_Bool active;  // if this output is active
   Ecore_X_Randr_Connection_Status status;

   E_Config_Randr_Output *cfg;
   E_Randr_Crtc *crtc;
};

struct _E_Randr_Crtc
{
   unsigned int xid; // ecore_x_randr output id (xid)

   Eina_Rectangle geo;  // geometry
   unsigned int orient; // value of the ecore_x_randr_orientation
   unsigned int mode;   // ecore_x_randr mode id (xid)
   Eina_List *outputs;  // list of outputs for this crtc
};

struct _E_Randr
{
   int active;         // number of active outputs
   Eina_List *crtcs;   // list of crtcs
   Eina_List *outputs; // list of outputs
};

EINTERN Eina_Bool e_randr_init(void);
EINTERN int e_randr_shutdown(void);

EAPI Eina_Bool e_randr_config_save(void);
EAPI void      e_randr_config_apply(void);
EAPI double    e_randr_mode_refresh_rate_get(Ecore_X_Randr_Mode_Info *mode);

extern EAPI E_Config_Randr *e_randr_cfg;
extern EAPI E_Randr *e_randr;

# endif
#endif
