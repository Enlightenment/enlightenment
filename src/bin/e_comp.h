#ifdef E_TYPEDEFS
typedef struct _E_Comp      E_Comp;
typedef struct _E_Comp_Win  E_Comp_Win;
typedef struct _E_Comp_Zone E_Comp_Zone;
typedef struct E_Event_Comp E_Event_Comp;

#else
#ifndef E_MOD_COMP_H
#define E_MOD_COMP_H

# include "e_comp_cfdata.h"
# include "e_comp_update.h"

struct E_Event_Comp
{
   E_Comp_Win *cw;
};

extern EAPI int E_EVENT_COMP_SOURCE_VISIBILITY;
extern EAPI int E_EVENT_COMP_SOURCE_ADD;
extern EAPI int E_EVENT_COMP_SOURCE_DEL;
extern EAPI int E_EVENT_COMP_SOURCE_CONFIGURE;

typedef enum
{
   E_COMP_ENGINE_NONE = 0,
   E_COMP_ENGINE_SW = 1,
   E_COMP_ENGINE_GL = 2
} E_Comp_Engine;

EINTERN Eina_Bool e_comp_init(void);
EINTERN int      e_comp_shutdown(void);
EINTERN Eina_Bool e_comp_manager_init(E_Manager *man);

EAPI int e_comp_internal_save(void);
EAPI E_Comp_Config *e_comp_config_get(void);
EAPI void e_comp_shadows_reset(void);

EAPI Evas *e_comp_evas_get(E_Comp *c);
EAPI void e_comp_update(E_Comp *c);
EAPI E_Comp_Win *e_comp_border_src_get(Ecore_X_Window win);
EAPI E_Comp_Win *e_comp_src_get(Ecore_X_Window win);
EAPI const Eina_List *e_comp_src_list_get(E_Comp *c);
EAPI Evas_Object *e_comp_src_image_get(E_Comp_Win *cw);
EAPI Evas_Object *e_comp_src_shadow_get(E_Comp_Win *cw);
EAPI Evas_Object *e_comp_src_image_mirror_add(E_Comp_Win *cw);
EAPI Eina_Bool e_comp_src_visible_get(E_Comp_Win *cw);
EAPI void e_comp_src_hidden_set(E_Comp_Win *cw, Eina_Bool hidden);
EAPI Eina_Bool e_comp_src_hidden_get(E_Comp_Win *cw);
EAPI E_Popup *e_comp_src_popup_get(E_Comp_Win *cw);
EAPI E_Border *e_comp_src_border_get(E_Comp_Win *cw);
EAPI Ecore_X_Window e_comp_src_window_get(E_Comp_Win *cw);

#endif
#endif
