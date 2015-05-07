#ifdef E_TYPEDEFS
#else
#ifndef E_CANVAS_H
#define E_CANVAS_H

E_API void        e_canvas_add(Ecore_Evas *ee);
E_API void        e_canvas_del(Ecore_Evas *ee);
E_API void        e_canvas_recache(void);
E_API void        e_canvas_cache_flush(void);
E_API void        e_canvas_cache_reload(void);
E_API void        e_canvas_idle_flush(void);
E_API void        e_canvas_rehint(void);
E_API Ecore_Evas *e_canvas_new(Ecore_Window win, int x, int y, int w, int h, int direct_resize, int override, Ecore_Window *win_ret);

E_API const Eina_List *e_canvas_list(void);
#endif
#endif
