#ifndef CPF_H
#define CPF_H 1

typedef enum
{
  CPF_EVENT_STATS // statistics changed
} Cpf_Event;

typedef enum
{
  CPF_GRADIENT_DEFAULT, // used for "usage" (e.g. cpu % used)
  CPF_GRADIENT_FREQ, // used ro frequency
} Cpf_Gradient;

typedef enum
{
  CPF_RENDER_COLORBAR_ALL,   // all cores usage and freqs for cores
  CPF_RENDER_COLORBAR_CPU_USAGE // all cores usage
} Cpf_Render_Type;

typedef struct
{
  Cpf_Render_Type  type;
  int              w, h;
  int              real_w, real_h;
  int              pos;
  int              refs; // get ++, unget --
  unsigned int    *pixels; // argb32 w * h
} Cpf_Render;

typedef struct
{
  short usage; // 0-1000
  short freq;  // mhz
  short freq_min;  // mhz
  short freq_max;  // mhz
} Cpf_Core_Perf;

typedef struct
{
  int             core_num;
  int             rend_num;
  Cpf_Core_Perf  *core_perf;
  Cpf_Render    **rend;
} Cpf_Stats;

void             cpf_init(void);
void             cpf_shutdown(void);
void             cpf_poll_time_set(double tim);
double           cpf_poll_time_get(void);
void             cpf_event_callback_add(Cpf_Event event, void (*cb) (void *data), void *data);
void             cpf_event_callback_del(Cpf_Event event, void (*cb) (void *data), void *data);
void             cpf_perf_level_set(int level); // 0-100
int              cpf_perf_level_get(void); // 0-100
const Cpf_Stats *cpf_perf_stats_get(void); // per core stat/freq info
void             cpf_render_req(Cpf_Render_Type type, int w, int h);
void             cpf_render_unreq(Cpf_Render_Type type, int w, int h);

#endif
