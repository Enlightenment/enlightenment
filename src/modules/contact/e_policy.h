#ifndef E_POLICY_H
# define E_POLICY_H

void e_policy_init(void);
void e_policy_shutdown(void);
void e_policy_kbd_override_set(Eina_Bool override);
const Eina_List *e_policy_clients_get(void);
const E_Client *e_policy_client_active_get(void);

#endif
