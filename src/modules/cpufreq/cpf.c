#include "e_mod_main.h"
#include "cpf_cpu.h"

// time between polls
static Eina_Lock      _poll_lock;
static double         _poll_time = 0.2;

// the thread that polls and renders stat areas (really simple to do)
static Eina_Lock      _poll_thread_lock;
static Ecore_Thread  *_poll_thread = NULL;

// a list of requested renders (type and sizw) so these will bre produced
// next poll
static Eina_Lock      _renders_lock;
static int            _renders_num = 0;
static Cpf_Render    *_renders     = NULL;
static Evas_Object   *_renders_win = NULL;
static unsigned int  *_renders_data_cpu_usage = NULL;
static unsigned int  *_renders_data_cpu_freq = NULL;

// number of pending stats in the pipe
static Eina_Lock      _cpu_stats_lock;
static int            _cpu_stats_pending = 0;

// current sets.state of stats
static Cpf_Stats     *_cpu_stats = NULL;

// current cpu level
static int            _cpu_level = -1;

typedef struct
{
  Cpf_Event   event;
  void      (*cb) (void *data);
  void       *data;
  Eina_Bool   delete_me : 1;
} Callback;

static int            _callbacks_walking = 0;
static int            _callbacks_del = 0;
static Eina_List     *_callbacks = NULL;

//////////////////////////////////////////////////////////////////////////

static void
_render_colorbar_all(Cpf_Render *r, Cpf_Stats *c)
{
  int i, y = 0, u;

  if ((c->core_num <= 0) || (r->real_w <= 0) || (r->real_h <= 0)) return;
  if ((r->real_w != r->w) || (r->real_h != (c->core_num * 2)))
    {
      free(r->pixels);
      r->pixels = NULL;
    }
  if (!r->pixels)
    {
      r->real_w = r->w;
      r->real_h = c->core_num * 2;
      r->pos    = 0;
      r->pixels = calloc(r->real_w * r->real_h, sizeof(int));
    }
  if (!r->pixels) return;
  for (i = 0; i < c->core_num; i++)
    {
      u = c->core_perf[i].usage;
      if (u >= 1000) u = 999;
      else if (u < 0) u = 0;
      r->pixels[r->pos + (r->real_w * y)] = _renders_data_cpu_usage[u];
      y++;
    }
  for (i = 0; i < c->core_num; i++)
    {
      if (c->core_perf[i].freq_max > c->core_perf[i].freq_min)
        u = ((c->core_perf[i].freq - c->core_perf[i].freq_min) * 1000)
            / (c->core_perf[i].freq_max - c->core_perf[i].freq_min);
      else u = 0;
      if (u >= 1000) u = 999;
      else if (u < 0) u = 0;
      r->pixels[r->pos + (r->real_w * y)] = _renders_data_cpu_freq[u];
      y++;
    }
  r->pos++;
  if (r->pos >= r->real_w) r->pos = 0;
}

static void
_render_colorbar_cpu_usage(Cpf_Render *r, Cpf_Stats *c)
{
  int i, y = 0, u;

  if ((c->core_num <= 0) || (r->real_w <= 0) || (r->real_h <= 0)) return;
  if ((r->real_w != r->w) || (r->real_h != (c->core_num)))
    {
      free(r->pixels);
      r->pixels = NULL;
    }
  if (!r->pixels)
    {
      r->real_w = r->w;
      r->real_h = c->core_num;
      r->pos    = 0;
      r->pixels = calloc(r->real_w * r->real_h, sizeof(int));
    }
  if (!r->pixels) return;
  for (i = 0; i < c->core_num; i++)
    {
      u = c->core_perf[i].usage;
      if (u >= 1000) u = 999;
      else if (u < 0) u = 0;
      r->pixels[r->pos + (r->real_w * y)] = _renders_data_cpu_usage[u];
      y++;
    }
  r->pos++;
  if (r->pos >= r->real_w) r->pos = 0;
}

static void
_render_render(Cpf_Render *r, Cpf_Stats *c)
{
  switch (r->type)
    {
    case CPF_RENDER_COLORBAR_ALL:
      _render_colorbar_all(r, c);
      break;
    case CPF_RENDER_COLORBAR_CPU_USAGE:
      _render_colorbar_cpu_usage(r, c);
      break;
    default:
      break;
    }
}

static void
_thread_main(void *data EINA_UNUSED, Ecore_Thread *eth)
{ // thread
  double tim;
  Cpu_Perf *cp = cpu_perf_add();
  long cpu_perc, cpu_diff;
  int i, y, pending;
  Cpf_Stats *cpf_stat;
  Cpf_Render *r;

  // lock we can wait on until this func is done
  eina_lock_take(&_poll_thread_lock);
  for (;;)
    {
      if (!cp) goto skip;

      // check if too many pending stats messages in the pipe
      eina_lock_take(&_cpu_stats_lock);
      pending = _cpu_stats_pending;
      eina_lock_release(&_cpu_stats_lock);
      if (pending > 2) goto skip;

      // update perf from system
      cpu_perf_update(cp);
      cpf_stat = calloc(1, sizeof(Cpf_Stats));
      if (!cpf_stat) goto skip;
      cpf_stat->core_perf = calloc(cp->cur.num, sizeof(Cpf_Core_Perf));
      if (!cpf_stat->core_perf)
        {
          free(cpf_stat);
          goto skip;
        }
      // fill in cpu usage and freq per core found
      for (i = 0; i < cp->cur.num; i++)
        {
          int cpu = cp->cur.counters[i].cpu;

          if (cpu >= 0)
            {
#define D(_f) (cp->cur.counters[i].fields[_f] - cp->prev.counters[i].fields[_f])
              cpu_perc =
                D(CPU_USER) + D(CPU_SYSTEM) + D(CPU_NICE) +
                D(CPU_IRQ) + D(CPU_SOFTIRQ);
#undef D
              cpu_diff = cp->cur.counters[i].total - cp->prev.counters[i].total;
              if ((cpu_diff > 0) && (cpu < cp->cur.num))
                {
                  cpu_perc = 1000 * cpu_perc / cpu_diff; // 0-1000 cpu usage
                  cpf_stat->core_perf[cpu].usage = cpu_perc;
                  cpf_stat->core_perf[cpu].freq
                    = cp->cur.freqinfo[cpu].cur / 1000;
                  cpf_stat->core_perf[cpu].freq_min
                    = cp->cur.freqinfo[cpu].min / 1000;
                  cpf_stat->core_perf[cpu].freq_max
                    = cp->cur.freqinfo[cpu].max / 1000;
                  if ((cpu + 1) > cpf_stat->core_num)
                    cpf_stat->core_num = cpu + 1;
                }
            }
        }
      eina_lock_take(&_renders_lock);
      cpf_stat->rend = calloc(_renders_num, sizeof(Cpf_Render *));
      if (cpf_stat->rend)
        {
          cpf_stat->rend_num = _renders_num;
          for (i = 0; i < _renders_num; i++)
            {
              _render_render(&(_renders[i]), cpf_stat);
              r = cpf_stat->rend[i] = calloc(1, sizeof(Cpf_Render));
              if (!r) continue;
              *r = _renders[i];
              if ((r->real_w <= 0) || (r->real_h <= 0)) continue;
              r->pixels = malloc(r->real_w * r->real_h * sizeof(int));
              if (!r->pixels) continue;
              for (y = 0; y < r->real_h; y++)
                {
                  unsigned int *src, *dst, len;

                  // r->pos is one pixel to the right of the last update
                  // and of course wraps around (pos 0 == last update at
                  // right end)
                  // copy from 0 to pos
                  dst = r->pixels;
                  src = _renders[i].pixels;
                  if ((!dst) || (!src)) break;
                  dst += (y * r->real_w) + r->real_w - r->pos;
                  src += (y * r->real_w);
                  len = r->pos;
                  if (len > 0) memcpy(dst, src, len * sizeof(int));
                  // copy from pos to 0
                  dst = r->pixels;
                  src = _renders[i].pixels;
                  dst += (y * r->real_w);
                  src += (y * r->real_w) + r->pos;
                  len = r->real_w - r->pos;
                  if (len > 0) memcpy(dst, src, len * sizeof(int));
                }
            }
        }
      eina_lock_release(&_renders_lock);
      // inc stats messages
      eina_lock_take(&_cpu_stats_lock);
      _cpu_stats_pending++;
      eina_lock_release(&_cpu_stats_lock);

      ecore_thread_feedback(eth, cpf_stat); // send stat to mainloop to handle
skip:
      if (ecore_thread_check(eth)) break;
      eina_lock_take(&_poll_lock);
      tim = _poll_time;
      eina_lock_release(&_poll_lock);
      usleep(tim * 1000000);
    }
  eina_lock_release(&_poll_thread_lock);
  cpu_perf_del(cp);
}

static void
_cpf_stats_free(Cpf_Stats *c)
{
  int i;

  free(c->core_perf);
  for (i = 0; i < c->rend_num; i++)
    {
      if (c->rend[i])
        {
          free(c->rend[i]->pixels);
          free(c->rend[i]);
        }
    }
  free(c->rend);
  free(c);
}

static void
_thread_notify(void *data EINA_UNUSED, Ecore_Thread *eth EINA_UNUSED, void *msgdata)
{ // messaages from thread
  Callback *c;
  Eina_List *l, *ll;

  // reduce pendingh count as it arrived at main loop
  eina_lock_take(&_cpu_stats_lock);
  if (msgdata) _cpu_stats_pending--;
  eina_lock_release(&_cpu_stats_lock);
  if (_cpu_stats) _cpf_stats_free(_cpu_stats);
  _cpu_stats = msgdata;
  _callbacks_walking++;
  EINA_LIST_FOREACH(_callbacks, l, c)
    {
      if (c->event == CPF_EVENT_STATS) c->cb(c->data);
    }
  _callbacks_walking--;
  if (_callbacks_del)
    {
      EINA_LIST_FOREACH_SAFE(_callbacks, l, ll, c)
        {
          if (c->delete_me)
            {
              _callbacks = eina_list_remove_list(_callbacks, l);
              free(c);
            }
        }
      _callbacks_del = 0;
    }
}

static void
_thread_end(void *data EINA_UNUSED, Ecore_Thread *eth EINA_UNUSED)
{ // thread ended in main loop
}

static void
_cancel(void *data EINA_UNUSED, Ecore_Thread *eth EINA_UNUSED)
{ // thread ended after cancel in main loop
}

//////////////////////////////////////////////////////////////////////////

static void *
_init_grad(Evas_Object *o_win, const char *grp, int len)
{
  Evas_Object *o_subwin, *o_rend, *o;
  const char  *file;

  o_subwin = elm_win_add(o_win, "inlined", ELM_WIN_INLINED_IMAGE);
  evas_object_resize(o_subwin, len, 1);
  evas_object_show(o_subwin);
  o_rend = elm_win_inlined_image_object_get(o_subwin);

  o    = edje_object_add(evas_object_evas_get(o_subwin));
  file = elm_theme_group_path_find(NULL, grp);
  edje_object_file_set(o, file, grp);
  evas_object_resize(o, len, 1);
  evas_object_show(o);

  return evas_object_image_data_get(o_rend, EINA_FALSE);
}

static void
_cb_system_pwr_get(void *data EINA_UNUSED, const char *params)
{
  int v;

  if (!params) return;
  if (!strcmp(params, "err")) _cpu_level = -1;
  else
    {
      v = atoi(params);
      _cpu_level = v;
    }
}

void
cpf_init(void)
{ // set everything up
  Evas_Object *o;

  elm_config_preferred_engine_set("buffer");
  _renders_win = o = elm_win_add(NULL, "", ELM_WIN_BASIC);
  elm_config_preferred_engine_set(NULL);

  _renders_data_cpu_usage
    = _init_grad(o, "e/modules/cpufreq/grad/cpu_usage", 1000);
  _renders_data_cpu_freq
    = _init_grad(o, "e/modules/cpufreq/grad/cpu_freq", 1000);

  // XXX: _cpu_level query via e_system
  eina_lock_new(&_poll_lock);
  eina_lock_new(&_poll_thread_lock);
  eina_lock_new(&_renders_lock);
  eina_lock_new(&_cpu_stats_lock);
  _poll_thread = ecore_thread_feedback_run
    (_thread_main, _thread_notify, _thread_end,
     _cancel, NULL, EINA_TRUE);
  e_system_handler_add("cpufreq-pwr-get", _cb_system_pwr_get, NULL);
  e_system_send("cpufreq-pwr-get", NULL);
}

void
cpf_shutdown(void)
{ // shut down everything
  int i;

  e_system_handler_add("cpufreq-pwr-get", _cb_system_pwr_get, NULL);
  ecore_thread_cancel(_poll_thread);
  _poll_thread = NULL;
  eina_lock_take(&_poll_thread_lock);
  eina_lock_release(&_poll_thread_lock);
  eina_lock_free(&_poll_thread_lock);
  eina_lock_free(&_poll_lock);
  eina_lock_free(&_renders_lock);
  eina_lock_free(&_cpu_stats_lock);
  evas_object_del(_renders_win);
  if (_cpu_stats) _cpf_stats_free(_cpu_stats);
  _cpu_stats              = NULL;
  _renders_win            = NULL;
  _renders_data_cpu_usage = NULL;
  _renders_data_cpu_freq  = NULL;
  for (i = 0; i < _renders_num; i++)
    {
      free(_renders[i].pixels);
    }
  free(_renders);
  _renders = NULL;
  _renders_num = 0;
}

void
cpf_poll_time_set(double tim)
{ // set time between cpufreq polls
  eina_lock_take(&_poll_lock);
  _poll_time = tim;
  eina_lock_release(&_poll_lock);
}

double
cpf_poll_time_get(void)
{ // get time between cpu polls
  double tim;

  eina_lock_take(&_poll_lock);
  tim = _poll_time;
  eina_lock_release(&_poll_lock);
  return tim;
}

void
cpf_event_callback_add(Cpf_Event event, void (*cb) (void *data), void *data)
{ // call this cb with the given data for the given event type when it happens
  Callback *c = calloc(1, sizeof(Callback));

  if (!c) return;
  c->event = event;
  c->cb = cb;
  c->data = data;
  _callbacks = eina_list_append(_callbacks, c);
}

void
cpf_event_callback_del(Cpf_Event event, void (*cb) (void *data), void *data)
{ // delete the callback added by cpf_event_callback_add()
  Callback *c;
  Eina_List *l;

  EINA_LIST_FOREACH(_callbacks, l, c)
    {
      if ((c->event == event) && (c->cb == cb) && (c->data == data))
        {
          if (_callbacks_walking)
            {
              c->delete_me = EINA_TRUE;
              _callbacks_del++;
            }
          else
            {
              _callbacks = eina_list_remove_list(_callbacks, l);
              free(c);
              break;
            } 
        }
    }
}

void
cpf_perf_level_set(int level)
{ // talk to e_system backed to set cpufreq stuff - 0 -> 100
  if (level < 0) level = 0;
  else if (level > 100) level = 100;
  if (_cpu_level == level) return;
  _cpu_level = level;
  e_system_send("cpufreq-pwr-set", "%i", level);
}

int
cpf_perf_level_get(void)
{ // get current level
  e_system_send("cpufreq-pwr-get", NULL);
  return _cpu_level;
}

const Cpf_Stats *
cpf_perf_stats_get(void)
{ // reuturn only valid until we return back to main loop
  return _cpu_stats;
}

void
cpf_render_req(Cpf_Render_Type type, int w, int h)
{ // register you want the renderer to produce this type
  int i;
  Eina_Bool found = EINA_FALSE;
  Cpf_Render *r;

  eina_lock_take(&_renders_lock);
  for (i = 0; i < _renders_num; i++)
    {
      if ((_renders[i].type == type) &&
          (_renders[i].w == w) &&
          (_renders[i].h == h))
        {
          _renders[i].refs++;
          found = EINA_TRUE;
          break;
        }
    }
  if (!found)
    {
      _renders_num++;

      r = realloc(_renders, _renders_num * sizeof(Cpf_Render));
      if (!r)
        {
          _renders_num--;
          fprintf(stderr, "Out of memory for Cpf_Renders array\n");
          goto done;
        }
      _renders = r;
      _renders[_renders_num - 1].type   = type;
      _renders[_renders_num - 1].w      = w;
      _renders[_renders_num - 1].h      = h;
      _renders[_renders_num - 1].real_w = w;
      _renders[_renders_num - 1].real_h = h;
      _renders[_renders_num - 1].pos    = 0;
      _renders[_renders_num - 1].refs   = 1;
      _renders[_renders_num - 1].pixels = NULL;
    }
done:
  eina_lock_release(&_renders_lock);
}

void
cpf_render_unreq(Cpf_Render_Type type, int w, int h)
{ // say you don't want this anymore - stop producing these
  int i, j;

  eina_lock_take(&_renders_lock);
  for (i = 0; i < _renders_num; i++)
    {
      if ((_renders[i].type == type) &&
          (_renders[i].w == w) &&
          (_renders[i].h == h))
        {
          _renders[i].refs--;
          if (_renders[i].refs <= 0)
            {
              _renders_num--;
              for (j = i; j < _renders_num; j++)
                {
                  _renders[j] = _renders[j + 1];
                }
              _renders = realloc(_renders, _renders_num * sizeof(Cpf_Render));
            }
          break;
        }
    }
  eina_lock_release(&_renders_lock);
}
