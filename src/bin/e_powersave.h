#ifdef E_TYPEDEFS

typedef enum _E_Powersave_Mode
{
   E_POWERSAVE_MODE_NONE,
   E_POWERSAVE_MODE_LOW,
   E_POWERSAVE_MODE_MEDIUM,
   E_POWERSAVE_MODE_HIGH,
   E_POWERSAVE_MODE_EXTREME,
   E_POWERSAVE_MODE_FREEZE
} E_Powersave_Mode;

typedef struct _E_Powersave_Deferred_Action E_Powersave_Deferred_Action;
typedef struct _E_Event_Powersave_Update E_Event_Powersave_Update;
typedef struct _E_Powersave_Sleeper E_Powersave_Sleeper;

#else
#ifndef E_POWERSAVE_H
#define E_POWERSAVE_H

extern E_API int E_EVENT_POWERSAVE_UPDATE;
extern E_API int E_EVENT_POWERSAVE_CONFIG_UPDATE;

struct _E_Event_Powersave_Update
{
   E_Powersave_Mode	mode;
};

EINTERN int e_powersave_init(void);
EINTERN int e_powersave_shutdown(void);

E_API E_Powersave_Deferred_Action *e_powersave_deferred_action_add(void (*func) (void *data), const void *data);
E_API void                         e_powersave_deferred_action_del(E_Powersave_Deferred_Action *pa);
E_API void                         e_powersave_mode_set(E_Powersave_Mode mode);
E_API E_Powersave_Mode             e_powersave_mode_get(void);
E_API void                         e_powersave_mode_force(E_Powersave_Mode mode);
E_API void                         e_powersave_mode_unforce(void);
E_API E_Powersave_Sleeper         *e_powersave_sleeper_new(void);
E_API void                         e_powersave_sleeper_free(E_Powersave_Sleeper *sleeper);
E_API void                         e_powersave_defer_suspend(void);
E_API void                         e_powersave_defer_hibernate(void);
E_API void                         e_powersave_defer_cancel(void);
// the below function is INTENDED to be called from a thread
E_API void                         e_powersave_sleeper_sleep(E_Powersave_Sleeper *sleeper, int poll_interval);

/* FIXME: in the powersave system add things like pre-loading entire files
 * int memory for pre-caching to avoid disk spinup, when in an appropriate
 * powersave mode */

/* FIXME: in powersave mode also add the ability to reduce framerate when
 * at a particular powersave mode */

/* FIXME: in powersave mode also reduce ecore timer precision, so timers
 * will be dispatched together and system will wakeup less. */

/* FIXME: in powersave mode also add the ability to change screenblanker
 * preferences when in powersave mode as well as check in the screensaver
 * has kicked in */

/* FIXME: in powersave mode also if screenblanker has kicked in be able to
 * auto-suspend etc. etc. */

#endif
#endif
