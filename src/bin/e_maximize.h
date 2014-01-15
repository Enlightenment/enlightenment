#ifdef E_TYPEDEFS
#else
#ifndef E_MAXIMIZE_H
#define E_MAXIMIZE_H

EAPI void e_maximize_client_shelf_fit(E_Client *ec, int *x1, int *y1, int *x2, int *y2, E_Maximize dir);
EAPI void e_maximize_client_dock_fit(E_Client *ec, int *x1, int *y1, int *x2, int *y2);
EAPI void e_maximize_client_shelf_fill(E_Client *ec, int *x1, int *y1, int *x2, int *y2, E_Maximize dir);
EAPI void e_maximize_client_client_fill(E_Client *ec, int *x1, int *y1, int *x2, int *y2, E_Maximize dir);

#endif
#endif
