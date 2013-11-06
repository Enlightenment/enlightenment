#ifdef E_TYPEDEFS

typedef struct _E_Randr_Output_Config E_Randr_Output_Config;
typedef struct _E_Randr_Crtc_Config E_Randr_Crtc_Config;
typedef struct _E_Randr_Config E_Randr_Config;

#else
# ifndef E_RANDR_H
#  define E_RANDR_H

#define E_RANDR_VERSION_1_1 ((1 << 16) | 1)
#define E_RANDR_VERSION_1_2 ((1 << 16) | 2)
#define E_RANDR_VERSION_1_3 ((1 << 16) | 3)
#define E_RANDR_VERSION_1_4 ((1 << 16) | 4)

#define E_RANDR_CONFIG_FILE_EPOCH 3
#define E_RANDR_CONFIG_FILE_GENERATION 3
#define E_RANDR_CONFIG_FILE_VERSION \
   ((E_RANDR_CONFIG_FILE_EPOCH * 1000000) + E_RANDR_CONFIG_FILE_GENERATION)

struct _E_Randr_Output_Config
{
   /* Stored values */
   unsigned int xid; // ecore_x_randr output id (xid)
   unsigned int crtc; // ecore_x_randr crtc id (xid)
   unsigned int orient; // value of the ecore_x_randr_orientation
   Eina_Rectangle geo; // geometry
   Eina_Bool connected; // connection status

   /* Runtime values */
   Eina_Bool exists; // is this output present in X ?
   unsigned int mode; // ecore_x_randr mode id (xid)
   char *name; // Name of output
   Eina_Bool is_lid; // Is this a laptop panel
};

struct _E_Randr_Crtc_Config
{
   /* Stored values */
   unsigned int xid; // ecore_x_randr crtc id (xid)

   /* Runtime values */
   Eina_Rectangle geo; // geometry
   unsigned int orient; // value of the ecore_x_randr_orientation
   unsigned int mode; // ecore_x_randr mode id (xid)
   Eina_Bool exists; // is this crtc present in X ?
   Eina_List *outputs; // list of outputs for this crtc
};

struct _E_Randr_Config
{
   /* Store values */
   int version; // INTERNAL CONFIG VERSION

   struct
     {
        int width, height; // geometry
     } screen;

   Eina_List *crtcs;
   Eina_List *outputs;

   int poll_interval;
   unsigned char restore;
   unsigned long config_timestamp;
   unsigned int primary;

   /* Runtime values */
   int connected;
};

EINTERN Eina_Bool e_randr_init(void);
EINTERN int e_randr_shutdown(void);

EAPI Eina_Bool e_randr_config_save(void);

extern EAPI E_Randr_Config *e_randr_cfg;

# endif
#endif
