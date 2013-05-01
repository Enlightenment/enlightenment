#ifdef E_TYPEDEFS
#else
# ifndef E_CANVAS_H
#  define E_CANVAS_H

EAPI void e_canvas_add(Ecore_Evas *ee);
EAPI void e_canvas_del(Ecore_Evas *ee);
EAPI void e_canvas_recache(void);
EAPI void e_canvas_cache_flush(void);
EAPI void e_canvas_cache_reload(void);
EAPI void e_canvas_idle_flush(void);
EAPI void e_canvas_rehint(void);
EAPI Ecore_Evas *e_canvas_new(unsigned int parent, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, Eina_Bool override, Eina_Bool frame, Ecore_Wl_Window **win_ret);
EAPI const Eina_List *e_canvas_list(void);

# endif
#endif
