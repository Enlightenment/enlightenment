/* xdg_popup.reposition and reactive positioners (xdg_wm_base version 3).
 *
 * reposition places a mapped popup again, and answers with repositioned,
 * naming the token, before the configure that carries the new position.
 *
 * A reactive popup is placed again whenever its parent moves, here into a
 * spot where it no longer fits and has to flip. A popup that is not
 * reactive keeps the position it was given. */
#include <stdio.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-popup-reposition"
#define APP_ID "e.test.popup_reposition"
#define TOKEN 7
#define PW 100
#define PH 40
#define TW 300
#define TH 200

typedef struct
{
   Tk *tk;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_popup *popup;
   int configures, x, y;
   int repositioned, token, repositioned_at;
} Popup;

static void
_surface_configure(void *data, struct xdg_surface *s, uint32_t serial)
{
   Popup *p = data;

   xdg_surface_ack_configure(s, serial);
   if (!p->configures) tk_surface_paint(p->tk, p->surface, PW, PH);
   else wl_surface_commit(p->surface);
   p->configures++;
}

static const struct xdg_surface_listener _surface_listener =
{
   _surface_configure
};

static void
_configure(void *data, struct xdg_popup *x, int32_t px, int32_t py,
           int32_t w, int32_t h)
{
   Popup *p = data;

   (void)x; (void)w; (void)h;
   p->x = px;
   p->y = py;
}

static void
_done(void *d, struct xdg_popup *x)
{
   (void)d; (void)x;
}

static void
_repositioned(void *data, struct xdg_popup *x, uint32_t token)
{
   Popup *p = data;

   (void)x;
   p->repositioned++;
   p->token = token;
   p->repositioned_at = p->configures;
}

static const struct xdg_popup_listener _popup_listener =
{
   _configure, _done, _repositioned
};

static struct xdg_positioner *
_positioner(Tk *tk, int ax, int ay, enum xdg_positioner_anchor anchor,
            int reactive)
{
   struct xdg_positioner *pos;

   pos = xdg_wm_base_create_positioner(tk_wm_base(tk));
   xdg_positioner_set_size(pos, PW, PH);
   xdg_positioner_set_anchor_rect(pos, ax, ay, 1, 1);
   xdg_positioner_set_anchor(pos, anchor);
   xdg_positioner_set_gravity(pos, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
   xdg_positioner_set_constraint_adjustment
     (pos, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X);
   if (reactive) xdg_positioner_set_reactive(pos);
   return pos;
}

static void
_popup_map(Tk *tk, Popup *p, Tk_Toplevel *parent, struct xdg_positioner *pos)
{
   memset(p, 0, sizeof(*p));
   p->tk = tk;
   p->surface = tk_surface_new(tk);
   p->xdg_surface = xdg_wm_base_get_xdg_surface(tk_wm_base(tk), p->surface);
   xdg_surface_add_listener(p->xdg_surface, &_surface_listener, p);
   p->popup = xdg_surface_get_popup(p->xdg_surface,
                                    tk_toplevel_xdg_surface(parent), pos);
   xdg_popup_add_listener(p->popup, &_popup_listener, p);
   wl_surface_commit(p->surface);
   tk_settle(tk);
   if (!p->configures) tk_fail(tk, "a popup was never configured");
}

static void
_popup_del(Popup *p)
{
   xdg_popup_destroy(p->popup);
   xdg_surface_destroy(p->xdg_surface);
   wl_surface_destroy(p->surface);
}

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   Popup p, reactive, still;
   struct xdg_positioner *pos;
   int tx, ty, x, y, zone_w, configures;
   unsigned int id;
   char by[32];

   tk = tk_connect(PROG);
   if (tk_global_version(tk, "xdg_wm_base") < 3)
     tk_fail(tk, "xdg_wm_base is advertised at version %u, not 3",
             tk_global_version(tk, "xdg_wm_base"));

   top = tk_toplevel_new(tk, APP_ID, PROG, TW, TH);
   c = tk_expect(tk, APP_ID);
   id = c->id;

   tk_action(tk, id, "window_maximized_toggle", "default both");
   zone_w = tk_expect(tk, APP_ID)->w;
   tk_action(tk, id, "window_maximized_toggle", "default both");
   tk_settle(tk);

   pos = _positioner(tk, 10, 10, XDG_POSITIONER_ANCHOR_TOP_LEFT, 0);
   _popup_map(tk, &p, top, pos);
   xdg_positioner_destroy(pos);
   if ((p.x != 10) || (p.y != 10))
     tk_fail(tk, "the popup was configured at %d,%d, not 10,10", p.x, p.y);

   pos = _positioner(tk, 50, 30, XDG_POSITIONER_ANCHOR_TOP_LEFT, 0);
   xdg_popup_reposition(p.popup, pos, TOKEN);
   xdg_positioner_destroy(pos);
   tk_settle(tk);
   if (p.repositioned != 1)
     tk_fail(tk, "reposition was answered by %d repositioned events",
             p.repositioned);
   if (p.token != TOKEN)
     tk_fail(tk, "repositioned named token %d, not %d", p.token, TOKEN);
   if (p.repositioned_at != 1)
     tk_fail(tk, "repositioned did not come before the configure it names");
   if ((p.x != 50) || (p.y != 30))
     tk_fail(tk, "the repositioned popup was configured at %d,%d, not 50,30",
             p.x, p.y);
   tk_surface_geometry(tk, tk_toplevel_surface(top), &tx, &ty, NULL, NULL);
   tk_surface_geometry(tk, p.surface, &x, &y, NULL, NULL);
   if ((x - tx != 50) || (y - ty != 30))
     tk_fail(tk, "the popup was told 50,30 but is at %d,%d of its parent",
             x - tx, y - ty);
   _popup_del(&p);

   /* Both hang off the parent's right edge, and flip to its left once the
    * parent is moved too close to the edge of the screen for them to fit. */
   pos = _positioner(tk, TW - 1, 0, XDG_POSITIONER_ANCHOR_TOP_RIGHT, 1);
   _popup_map(tk, &reactive, top, pos);
   xdg_positioner_destroy(pos);
   pos = _positioner(tk, TW - 1, 0, XDG_POSITIONER_ANCHOR_TOP_RIGHT, 0);
   _popup_map(tk, &still, top, pos);
   xdg_positioner_destroy(pos);
   if ((reactive.x != TW) || (still.x != TW))
     tk_fail(tk, "the popups were placed at x %d and %d, not %d",
             reactive.x, still.x, TW);

   c = tk_expect(tk, APP_ID);
   snprintf(by, sizeof(by), "%d 0", zone_w - TW - (PW / 2) - c->x);
   configures = still.configures;
   tk_action(tk, id, "window_move_by", by);
   tk_settle(tk);
   if (reactive.x + PW > TW)
     tk_fail(tk, "the parent moved so the reactive popup no longer fits, and "
                 "it was left at x %d", reactive.x);
   if (still.configures != configures)
     tk_fail(tk, "a popup that is not reactive was configured again when "
                 "its parent moved");
   _popup_del(&reactive);
   _popup_del(&still);

   tk_toplevel_del(top);
   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
