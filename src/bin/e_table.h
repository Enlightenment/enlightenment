#ifdef E_TYPEDEFS
#else
#ifndef E_TABLE_H
#define E_TABLE_H

E_API Evas_Object *e_table_add               (Evas *evas);
E_API int          e_table_freeze            (Evas_Object *obj);
E_API int          e_table_thaw              (Evas_Object *obj);
E_API void         e_table_homogenous_set    (Evas_Object *obj, int homogenous);
E_API void         e_table_pack              (Evas_Object *obj, Evas_Object *child, int col, int row, int colspan, int rowspan);
E_API void         e_table_pack_options_set  (Evas_Object *obj, int fill_w, int fill_h, int expand_w, int expand_h, double align_x, double align_y, Evas_Coord min_w, Evas_Coord min_h, Evas_Coord max_w, Evas_Coord max_h);
E_API void         e_table_unpack            (Evas_Object *obj);
E_API void         e_table_col_row_size_get  (Evas_Object *obj, int *cols, int *rows);
E_API void         e_table_size_min_get      (Evas_Object *obj, Evas_Coord *minw, Evas_Coord *minh);
E_API void         e_table_size_max_get      (Evas_Object *obj, Evas_Coord *maxw, Evas_Coord *maxh);
E_API void         e_table_align_get         (Evas_Object *obj, double *ax, double *ay);
E_API void         e_table_align_set         (Evas_Object *obj, double ax, double ay);

#endif
#endif
