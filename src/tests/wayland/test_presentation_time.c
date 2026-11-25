/* wp_presentation: every feedback ends once, presented with a time in the
 * advertised clock and the output it was shown on, or discarded when its
 * content never reached the screen. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "e_wl_testkit.h"
#include "presentation-time-client-protocol.h"

#define PROG "test-presentation-time"
#define APP_ID "e.test.presentation"
#define W 200
#define H 150
#define SLACK_NS 1000000000ll

typedef struct
{
   int presented, discarded, synced;
   struct wl_output *sync_output;
   int64_t committed, received, reported;
} Feedback;

static Tk *tk;
static clockid_t presentation_clock;
static int clock_ids;

static int64_t
_now(void)
{
   struct timespec ts;

   if (clock_gettime(presentation_clock, &ts))
     tk_fail(tk, "clock %d named by clock_id cannot be read", (int)presentation_clock);
   return ((int64_t)ts.tv_sec * 1000000000ll) + ts.tv_nsec;
}

static void
_clock_id(void *d __attribute__((unused)), struct wp_presentation *p __attribute__((unused)),
          uint32_t clk_id)
{
   presentation_clock = clk_id;
   clock_ids++;
}

static const struct wp_presentation_listener _presentation_listener = { _clock_id };

static void
_sync_output(void *data, struct wp_presentation_feedback *f __attribute__((unused)),
             struct wl_output *output)
{
   Feedback *fb = data;

   if (fb->presented) tk_fail(tk, "sync_output after presented");
   fb->sync_output = output;
   fb->synced++;
}

static void
_presented(void *data, struct wp_presentation_feedback *f __attribute__((unused)),
           uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec,
           uint32_t refresh __attribute__((unused)), uint32_t seq_hi __attribute__((unused)),
           uint32_t seq_lo __attribute__((unused)), uint32_t flags __attribute__((unused)))
{
   Feedback *fb = data;
   uint64_t sec = ((uint64_t)sec_hi << 32) | sec_lo;

   if (nsec > 999999999) tk_fail(tk, "tv_nsec %u out of range", nsec);
   fb->presented++;
   fb->received = _now();
   fb->reported = ((int64_t)sec * 1000000000ll) + nsec;
}

static void
_discarded(void *data, struct wp_presentation_feedback *f __attribute__((unused)))
{
   Feedback *fb = data;

   fb->discarded++;
}

static const struct wp_presentation_feedback_listener _feedback_listener =
{
   _sync_output,
   _presented,
   _discarded
};

/* The feedback is for the content of the next commit. */
static void
_feedback(struct wp_presentation *pres, struct wl_surface *surface, Feedback *fb)
{
   struct wp_presentation_feedback *obj;

   memset(fb, 0, sizeof(*fb));
   fb->committed = _now();
   obj = wp_presentation_feedback(pres, surface);
   wp_presentation_feedback_add_listener(obj, &_feedback_listener, fb);
}

static void
_commit(struct wp_presentation *pres, Tk_Toplevel *top, Feedback *fb)
{
   _feedback(pres, tk_toplevel_surface(top), fb);
   tk_surface_paint(tk, tk_toplevel_surface(top), W, H);
}

static void
_wait(Feedback *fb, const char *what)
{
   struct timespec step = { 0, 10000000 };
   int i;

   for (i = 0; i < TK_WAIT_SECONDS * 100; i++)
     {
        tk_sync(tk);
        if (fb->presented || fb->discarded) break;
        nanosleep(&step, NULL);
     }
   tk_sync(tk);
   if ((fb->presented + fb->discarded) != 1)
     tk_fail(tk, "%s: presented %d, discarded %d", what, fb->presented, fb->discarded);
}

static void
_expect_presented(Feedback *fb, struct wl_output *output, const char *what)
{
   _wait(fb, what);
   if (!fb->presented) tk_fail(tk, "%s: discarded", what);
   if ((fb->synced != 1) || (fb->sync_output != output))
     tk_fail(tk, "%s: %d sync_output, not for the bound output", what, fb->synced);
   if ((fb->reported < fb->committed - SLACK_NS) || (fb->reported > fb->received + SLACK_NS))
     tk_fail(tk, "%s: presented at %lld, outside %lld..%lld", what,
             (long long)fb->reported, (long long)fb->committed, (long long)fb->received);
}

int
main(void)
{
   struct wp_presentation *pres;
   struct wl_subcompositor *subcomp;
   struct wl_subsurface *subsurface;
   struct wl_surface *sub;
   struct wl_output *output;
   Tk_Toplevel *top;
   Feedback fb, first, second;
   int i;

   tk = tk_connect(PROG);
   pres = tk_bind(tk, &wp_presentation_interface, 1);
   output = tk_bind(tk, &wl_output_interface, 1);
   subcomp = tk_bind(tk, &wl_subcompositor_interface, 1);
   if ((!pres) || (!output) || (!subcomp)) tk_fail(tk, "missing global");
   wp_presentation_add_listener(pres, &_presentation_listener, NULL);
   tk_sync(tk);
   if (clock_ids != 1) tk_fail(tk, "%d clock_id events", clock_ids);
   top = tk_toplevel_new(tk, APP_ID, APP_ID, W, H);

   for (i = 0; i < 4; i++)
     {
        _commit(pres, top, &fb);
        _expect_presented(&fb, output, "a lone commit");
     }

   /* Replaced before any render could draw it. */
   _commit(pres, top, &first);
   _commit(pres, top, &second);
   _wait(&first, "the replaced commit");
   if (!first.discarded) tk_fail(tk, "content replaced before a render was presented");
   _expect_presented(&second, output, "the commit that replaced it");

   /* A synchronized subsurface's content is drawn once its parent commits. */
   sub = tk_surface_new(tk);
   subsurface = wl_subcompositor_get_subsurface(subcomp, sub, tk_toplevel_surface(top));
   wl_subsurface_set_position(subsurface, 10, 10);
   _feedback(pres, sub, &fb);
   tk_surface_paint(tk, sub, 40, 40);
   tk_surface_paint(tk, tk_toplevel_surface(top), W, H);
   _wait(&fb, "a synchronized subsurface");
   if (!fb.presented) tk_fail(tk, "content of a synchronized subsurface was discarded");

   /* Asked for, then the surface goes without committing. */
   _feedback(pres, sub, &fb);
   wl_subsurface_destroy(subsurface);
   wl_surface_destroy(sub);
   _wait(&fb, "a feedback on a surface destroyed before its commit");
   if (!fb.discarded) tk_fail(tk, "a feedback that was never committed was presented");

   /* The surface goes before its content is drawn. */
   _commit(pres, top, &fb);
   tk_toplevel_del(top);
   _wait(&fb, "a commit on a destroyed surface");
   if (!fb.discarded) tk_fail(tk, "content of a destroyed surface was presented");

   tk_disconnect(tk);
   printf(PROG ": ok\n");
   return 0;
}
