#ifndef E_POLICY_H
# define E_POLICY_H

void e_policy_init(void);
void e_policy_shutdown(void);
void e_policy_kbd_override_set(Eina_Bool override);
const Eina_List *e_policy_borders_get(void);
const E_Border *e_polict_border_active_get(void);

#endif
