#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e.h"

/**
 * https://phab.enlightenment.org/w/teamwork_api/
 */

/* Increment for Major Changes */
#define MOD_CONFIG_FILE_EPOCH      1
/* Increment for Minor Changes (ie: user doesn't need a new config) */
#define MOD_CONFIG_FILE_GENERATION 0
#define MOD_CONFIG_FILE_VERSION    ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

typedef struct Teamwork_Config
{
   unsigned int config_version;

   Eina_Bool disable_media_fetch;
   Eina_Bool disable_video;
   long long int allowed_media_size;
   long long int allowed_media_fetch_size;
   int allowed_media_age;

   double mouse_out_delay;
   double popup_size;
   double popup_opacity;
} Teamwork_Config;

typedef struct Mod
{
   E_Module *module;
   E_Config_Dialog *cfd;
   size_t media_size;
   Eina_Inlist *media_list;
   Eina_Hash *media;
   Evas_Object *pop;
   Eina_Bool sticky : 1;
   Eina_Bool force : 1;
   Eina_Bool hidden : 1;
} Mod;

extern Teamwork_Config *tw_config;
extern Mod *tw_mod;
extern int _e_teamwork_log_dom;

EINTERN const char *sha1_encode(const unsigned char *data, size_t len);

EINTERN int e_tw_init(void);
EINTERN void e_tw_shutdown(void);
EINTERN Eina_Bool tw_hide(void *d EINA_UNUSED);
EINTERN void tw_popup_opacity_set(void);
EINTERN void tw_uri_show(const char *uri);

EINTERN void tw_link_detect(E_Client *ec, const char *uri);
EINTERN void tw_link_show(E_Client *ec, const char *uri, int x, int y);
EINTERN void tw_link_hide(E_Client *ec, const char *uri);
EINTERN void tw_link_open(E_Client *ec, const char *uri);

typedef void (*Teamwork_Signal_Cb)(E_Client *, const char *);
typedef void (*Teamwork_Signal_Progress_Cb)(E_Client *, const char *, uint32_t);

extern Teamwork_Signal_Cb tw_signal_link_complete[E_PIXMAP_TYPE_NONE];
extern Teamwork_Signal_Cb tw_signal_link_invalid[E_PIXMAP_TYPE_NONE];
extern Teamwork_Signal_Progress_Cb tw_signal_link_progress[E_PIXMAP_TYPE_NONE];
extern Teamwork_Signal_Cb tw_signal_link_downloading[E_PIXMAP_TYPE_NONE];

EINTERN E_Config_Dialog *e_int_config_teamwork_module(Evas_Object *parent, const char *params EINA_UNUSED); 

E_API int e_modapi_shutdown(E_Module *m EINA_UNUSED);

#ifdef HAVE_WAYLAND
EINTERN Eina_Bool wl_tw_init(void);
EINTERN void wl_tw_shutdown(void);
#endif

#ifndef HAVE_WAYLAND_ONLY
EINTERN Eina_Bool x11_tw_init(void);
EINTERN void x11_tw_shutdown(void);
#endif

#define E_TW_VERSION 2

#undef DBG
#undef INF
#undef WRN
#undef ERR
#undef CRIT
#define DBG(...)  EINA_LOG_DOM_DBG(_e_teamwork_log_dom, __VA_ARGS__)
#define INF(...)  EINA_LOG_DOM_INFO(_e_teamwork_log_dom, __VA_ARGS__)
#define WRN(...)  EINA_LOG_DOM_WARN(_e_teamwork_log_dom, __VA_ARGS__)
#define ERR(...)  EINA_LOG_DOM_ERR(_e_teamwork_log_dom, __VA_ARGS__)
#define CRIT(...) EINA_LOG_DOM_CRIT(_e_teamwork_log_dom, __VA_ARGS__)

#endif
