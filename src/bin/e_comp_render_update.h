#ifdef E_TYPEDEFS
typedef struct _E_Comp_Render_Update      E_Comp_Render_Update;
typedef struct _E_Comp_Render_Update_Rect E_Comp_Render_Update_Rect;
typedef enum _E_Comp_Render_Update_Policy
{
   E_COMP_RENDER_UPDATE_POLICY_RAW,
   E_COMP_RENDER_UPDATE_POLICY_HALF_WIDTH_OR_MORE_ROUND_UP_TO_FULL_WIDTH,
} E_Comp_Render_Update_Policy;
#else
#ifndef E_COMP_RENDER_UPDATE_H
#define E_COMP_RENDER_UPDATE_H

struct _E_Comp_Render_Update_Rect
{
   int x, y, w, h;
};

struct _E_Comp_Render_Update
{
   int             w, h;
   int             tw, th;
   int             tsw, tsh;
   unsigned char  *tiles;
   E_Comp_Render_Update_Policy pol;
};

E_Comp_Render_Update *e_comp_render_update_new(void);
void      e_comp_render_update_free(E_Comp_Render_Update *up);
void      e_comp_render_update_policy_set(E_Comp_Render_Update       *up,
                                   E_Comp_Render_Update_Policy pol);
void      e_comp_render_update_tile_size_set(E_Comp_Render_Update *up,
                                      int       tsw,
                                      int       tsh);
void e_comp_render_update_resize(E_Comp_Render_Update *up,
                          int       w,
                          int       h);
void e_comp_render_update_add(E_Comp_Render_Update *up,
                       int       x,
                       int       y,
                       int       w,
                       int       h);
E_Comp_Render_Update_Rect *e_comp_render_update_rects_get(E_Comp_Render_Update *up);
void           e_comp_render_update_clear(E_Comp_Render_Update *up);

#endif
#endif
