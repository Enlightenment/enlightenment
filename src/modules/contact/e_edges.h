#ifndef E_EDGES_H
# define E_EDGES_H

typedef enum
{
   E_EDGES_LEFT_IN_BEGIN,
   E_EDGES_RIGHT_IN_BEGIN,
   E_EDGES_TOP_IN_BEGIN,
   E_EDGES_BOTTOM_IN_BEGIN,
   E_EDGES_LEFT_IN_SLIDE,
   E_EDGES_RIGHT_IN_SLIDE,
   E_EDGES_TOP_IN_SLIDE,
   E_EDGES_BOTTOM_IN_SLIDE
} E_Edges_Event;

void e_edges_init(void);
void e_edges_shutdown(void);
void e_edges_handler_set(E_Edges_Event event, void (*func) (void *data, int d, double v), void *data);

#endif
