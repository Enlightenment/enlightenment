#ifndef E_CONFIG_DESCRIPTOR_H
#define E_CONFIG_DESCRIPTOR_H

typedef struct _E_Color_Class
{
   const char	 *name; /* stringshared name */
   int		  r, g, b, a;
   int		  r2, g2, b2, a2;
   int		  r3, g3, b3, a3;
} E_Color_Class;

EINTERN void e_config_descriptor_init(Eina_Bool old);
EINTERN void e_config_descriptor_shutdown(void);
EINTERN E_Config_DD *e_config_descriptor_get(void);
EINTERN E_Config_DD *e_config_binding_descriptor_get(void);

#endif /* E_CONFIG_DESCRIPTOR_H */
