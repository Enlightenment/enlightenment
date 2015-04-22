#ifndef E_COMP_X_RANDR_H
# define E_COMP_X_RANDR_H

EAPI void e_comp_x_randr_init(void);
EAPI void e_comp_x_randr_shutdown(void);
EAPI void e_comp_x_randr_config_apply(void);
EAPI Eina_Bool e_comp_x_randr_available(void);
EAPI E_Randr2 *e_comp_x_randr_create(void);

EAPI Eina_Bool e_comp_x_randr_canvas_new(Ecore_Window parent, int w, int h);
#endif
