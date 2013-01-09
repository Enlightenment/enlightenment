#ifdef E_TYPEDEFS
#else
# ifndef E_COMP_WL_INPUT_H
#  define E_COMP_WL_INPUT_H

Eina_Bool e_comp_wl_input_init(void);
void e_comp_wl_input_shutdown(void);
Wayland_Input *e_comp_wl_input_get(void);

# endif
#endif
