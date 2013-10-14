#ifdef E_TYPEDEFS

#else
#ifndef E_MOVERESIZE_H
#define E_MOVERESIZE_H

EINTERN int e_moveresize_init(void);
EINTERN int e_moveresize_shutdown(void);
EAPI void e_moveresize_replace(Eina_Bool enable);
#endif
#endif
