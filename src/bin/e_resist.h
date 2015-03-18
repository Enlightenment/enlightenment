#ifdef E_TYPEDEFS
#else
#ifndef E_RESIST_H
#define E_RESIST_H

EAPI int e_resist_client_position(Eina_List *skiplist, int px, int py, int pw, int ph, int x, int y, int w, int h, int *rx, int *ry, int *rw, int *rh);
EAPI int e_resist_gadman_position(Eina_List *skiplist, int px, int py, int pw, int ph, int x, int y, int w, int h, int *rx, int *ry);

#endif
#endif
