#ifndef E_GRAPH_H
#define E_GRAPH_H 1

#include <Elementary.h>

E_API Evas_Object *e_graph_add(Evas_Object *parent);
E_API void         e_graph_values_set(Evas_Object *obj, int num, int *vals, int min, int max);
// cc:
// cc:name_of_color_class - use this named colorclass
// #f80                   - 12bit rgb val
// #f804                  - 16bit rgba val
// #ff8800                - 24bit rgb val
// #ff880044              - 32bit rgba val
E_API void         e_graph_colorspec_set(Evas_Object *obj, const char *cc);
E_API void         e_graph_colorspec_down_set(Evas_Object *obj, const char *cc);

#endif
