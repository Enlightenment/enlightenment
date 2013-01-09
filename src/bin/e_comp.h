#ifdef E_TYPEDEFS
typedef struct _E_Comp      E_Comp;
typedef struct _E_Comp_Win  E_Comp_Win;
typedef struct _E_Comp_Zone E_Comp_Zone;

#define E_COMP_ENGINE_SW 1
#define E_COMP_ENGINE_GL 2

#else
#ifndef E_MOD_COMP_H
#define E_MOD_COMP_H

Eina_Bool e_comp_init(void);
void      e_comp_shutdown(void);

EAPI void e_comp_shadow_set(void);

#endif
#endif
