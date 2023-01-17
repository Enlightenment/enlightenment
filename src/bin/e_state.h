#ifdef E_TYPEDEFS

/* struct used to pass to event handlers */
typedef struct _E_Event_State        E_Event_State;

typedef struct _E_State_Item         E_State_Item;
typedef union  _E_State_Val          E_State_Val;

typedef enum
{
   E_STATE_UNKNOWN,
   E_STATE_BOOL,
   E_STATE_INT,
   E_STATE_UINT,
   E_STATE_DOUBLE,
   E_STATE_STRING
} E_State_Type;

typedef enum
{
   E_STATE_CHANGE,
   E_STATE_DEL
} E_State_Event;

#else
# ifndef E_STATE_H
#  define E_STATE_H

union _E_State_Val
{
   Eina_Bool     b;
   int           i;
   unsigned int  u;
   double        d;
   const char   *s;
};

struct _E_State_Item
{
   const char      *name;
   E_State_Type     type;
   E_State_Val      val;
};

struct _E_Event_State
{
   E_State_Event event;
   E_State_Item  item;
};

EINTERN int         e_state_init(void);
EINTERN int         e_state_shutdown(void);

E_API E_State_Item  e_state_item_get(const char *name);
E_API void          e_state_item_set(E_State_Item it);

E_API void          e_state_item_del(const char *name);

E_API Eina_Bool     e_state_item_val_bool_get(const char *name);
E_API int           e_state_item_val_int_get(const char *name);
E_API unsigned int  e_state_item_val_uint_get(const char *name);
E_API double        e_state_item_val_double_get(const char *name);
E_API const char   *e_state_item_val_string_get(const char *name);
E_API void          e_state_item_val_bool_set(const char *name, Eina_Bool b);
E_API void          e_state_item_val_int_set(const char *name, int i);
E_API void          e_state_item_val_uint_set(const char *name, unsigned int u);
E_API void          e_state_item_val_double_set(const char *name, double d);
E_API void          e_state_item_val_string_set(const char *name, const char *s);

extern E_API int E_EVENT_STATE_CHANGE;

# endif
#endif
