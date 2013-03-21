#ifdef E_TYPEDEFS

typedef struct E_Event_Configure_Option E_Event_Configure_Option_Changed;
typedef struct E_Event_Configure_Option E_Event_Configure_Option_Add;
typedef struct E_Event_Configure_Option E_Event_Configure_Option_Del;
typedef struct E_Event_Configure_Category E_Event_Configure_Option_Category_Add;
typedef struct E_Event_Configure_Category E_Event_Configure_Option_Category_Del;
typedef struct E_Event_Configure_Tag E_Event_Configure_Option_Tag_Add;
typedef struct E_Event_Configure_Tag E_Event_Configure_Option_Tag_Del;
typedef struct E_Configure_Option_Info E_Configure_Option_Info;
typedef struct E_Configure_Option E_Configure_Option;
typedef struct E_Configure_Option_Ctx E_Configure_Option_Ctx;
typedef void (*E_Configure_Option_Set_Cb)();
typedef Eina_List *(*E_Configure_Option_Info_Cb)(E_Configure_Option *);
typedef Evas_Object *(*E_Configure_Option_Info_Thumb_Cb)(E_Configure_Option_Info *, Evas *);

#else
# ifndef E_CONFIGURE_OPTION_H
#  define E_CONFIGURE_OPTION_H

#define E_CONFIGURE_OPTION_TAG_LENGTH 128

#define E_CONFIGURE_OPTION_ADD(OPT, TYPE, NAME, CFGPTR, DESC, ...) \
   OPT = e_configure_option_add(E_CONFIGURE_OPTION_TYPE_##TYPE, DESC, #NAME, &CFGPTR->NAME, NULL);\
   e_configure_option_tags_set(OPT, (const char*[]){__VA_ARGS__, NULL}, 0)
#define E_CONFIGURE_OPTION_ADD_CUSTOM(OPT, NAME, DESC, ...) \
   OPT = e_configure_option_add(E_CONFIGURE_OPTION_TYPE_CUSTOM, DESC, NAME, NULL, NULL);\
   e_configure_option_tags_set(OPT, (const char*[]){__VA_ARGS__, NULL}, 0)
#define E_CONFIGURE_OPTION_HELP(OPT, STR) \
   OPT->help = eina_stringshare_add(_(STR))
#define E_CONFIGURE_OPTION_MINMAX_STEP_FMT(OPT, MIN, MAX, STEP, FMT) \
   OPT->minmax[0] = (MIN), OPT->minmax[1] = (MAX), OPT->step = (STEP),\
   OPT->info = eina_stringshare_add(_(FMT))
#define E_CONFIGURE_OPTION_ICON(OPT, ICON) \
   e_configure_option_data_set(OPT, "icon", eina_stringshare_add(ICON))


EAPI extern int E_EVENT_CONFIGURE_OPTION_CHANGED;
EAPI extern int E_EVENT_CONFIGURE_OPTION_ADD;
EAPI extern int E_EVENT_CONFIGURE_OPTION_DEL;
EAPI extern int E_EVENT_CONFIGURE_OPTION_CATEGORY_ADD;
EAPI extern int E_EVENT_CONFIGURE_OPTION_CATEGORY_DEL;
EAPI extern int E_EVENT_CONFIGURE_OPTION_TAG_ADD;
EAPI extern int E_EVENT_CONFIGURE_OPTION_TAG_DEL;

typedef enum
{
   E_CONFIGURE_OPTION_TYPE_BOOL,
   E_CONFIGURE_OPTION_TYPE_INT,
   E_CONFIGURE_OPTION_TYPE_UINT,
   E_CONFIGURE_OPTION_TYPE_ENUM,
   E_CONFIGURE_OPTION_TYPE_DOUBLE,
   E_CONFIGURE_OPTION_TYPE_DOUBLE_UCHAR, //lround(double)
   E_CONFIGURE_OPTION_TYPE_DOUBLE_INT, //lround(double)
   E_CONFIGURE_OPTION_TYPE_DOUBLE_UINT, //lround(double)
   E_CONFIGURE_OPTION_TYPE_STR,
   E_CONFIGURE_OPTION_TYPE_CUSTOM,
} E_Configure_Option_Type;

struct E_Configure_Option
{
   EINA_INLIST;
   Eina_Value val;
   E_Configure_Option_Type type;
   void *valptr;
   Eina_Hash *data;

   double minmax[2]; //for sliders
   double step; //for sliders
   Eina_Stringshare *info; //for sliders, custom
   E_Configure_Option_Info_Cb info_cb; //for enums
   E_Configure_Option_Info_Thumb_Cb thumb_cb; //for custom thumbs

   Eina_Stringshare *name;
   Eina_Stringshare *desc;
   Eina_Stringshare *help;
   Eina_List *tags; //Eina_Stringshare

   int event_type; //event to emit if changed
   Eina_Stringshare *changed_action; //action to call if changed
   struct
   {
      void (*none)(void);
      void (*one)();
      void (*two)();
   } funcs[2]; //disable, enable
   Eina_Bool requires_restart : 1;
   Eina_Bool changed : 1;
};

struct E_Configure_Option_Info
{
   E_Configure_Option *co;
   Eina_Stringshare *name;
   void *value;
   Eina_Stringshare *thumb_file;
   Eina_Stringshare *thumb_key;
   Eina_Bool current : 1;
};

struct E_Event_Configure_Option
{
   E_Configure_Option *co;
};

struct E_Event_Configure_Category
{
   Eina_Stringshare *category;
};

struct E_Event_Configure_Tag
{
   Eina_Stringshare *tag;
};

struct E_Configure_Option_Ctx
{
   Eina_List *tags; // Eina_Stringshare
   Eina_List *match_tags; // Eina_Stringshare
   Eina_List *opts; // E_Configure_Option
   Eina_Stringshare *category;
   char *text;
   Eina_Bool changed : 1;
};

EAPI const Eina_List *e_configure_option_tags_list(void);
EAPI const Eina_List *e_configure_option_changed_list(void);
EAPI void e_configure_option_apply_all(void);
EAPI void e_configure_option_reset_all(void);

EAPI E_Configure_Option *e_configure_option_add(E_Configure_Option_Type type, const char *desc, const char *name, void *valptr, const void *data);
EAPI void e_configure_option_tags_set(E_Configure_Option *co, const char * const *tags, unsigned int num_tags);
EAPI void e_configure_option_del(E_Configure_Option *eci);
EAPI const Eina_List *e_configure_option_tag_list_options(const char *tag);
EAPI void e_configure_option_changed(E_Configure_Option *co);
EAPI void e_configure_option_apply(E_Configure_Option *co);
EAPI void e_configure_option_reset(E_Configure_Option *co);
EAPI void *e_configure_option_data_set(E_Configure_Option *co, const char *key, const void *data);
EAPI void *e_configure_option_data_get(E_Configure_Option *co, const char *key);
EAPI const void *e_configure_option_value_get(E_Configure_Option *co);

EAPI E_Configure_Option_Info *e_configure_option_info_new(E_Configure_Option *co, const char *name, const void *value);
EAPI void e_configure_option_info_free(E_Configure_Option_Info *oi);
EAPI Eina_List *e_configure_option_info_get(E_Configure_Option *co);
EAPI Evas_Object *e_configure_option_info_thumb_get(E_Configure_Option_Info *oi, Evas *evas);

EAPI void e_configure_option_tag_alias_add(const char *tag, const char *alias);
EAPI void e_configure_option_tag_alias_del(const char *tag, const char *alias);

EAPI const Eina_List *e_configure_option_category_list(void);
EAPI const Eina_List *e_configure_option_category_list_tags(const char *cat);
EAPI void e_configure_option_category_tag_add(const char *cat, const char *tag);
EAPI void e_configure_option_category_tag_del(const char *cat, const char *tag);
EAPI Eina_Stringshare *e_configure_option_category_icon_get(const char *cat);
EAPI void e_configure_option_category_icon_set(const char *cat, const char *icon);

EAPI E_Configure_Option_Ctx *e_configure_option_ctx_new(void);
EAPI void e_configure_option_ctx_free(E_Configure_Option_Ctx *ctx);
EAPI Eina_Bool e_configure_option_ctx_update(E_Configure_Option_Ctx *ctx, const char *str);
EAPI const Eina_List *e_configure_option_ctx_option_list(E_Configure_Option_Ctx *ctx);
EAPI const Eina_List *e_configure_option_ctx_match_tag_list(E_Configure_Option_Ctx *ctx);
EAPI Eina_Bool e_configure_option_ctx_tag_add(E_Configure_Option_Ctx *ctx, Eina_Stringshare *tag);
EAPI Eina_Bool e_configure_option_ctx_tag_pop(E_Configure_Option_Ctx *ctx);

EAPI void e_configure_option_domain_current_set(const char *domain);
EAPI Eina_Inlist *e_configure_option_domain_list(const char *domain);
EAPI void e_configure_option_domain_clear(const char *domain);

EAPI const Eina_List *e_configure_option_util_themes_get(void);
EAPI const Eina_List *e_configure_option_util_themes_system_get(void);
EAPI const Eina_List *e_configure_option_util_themes_gtk_get(void);

EINTERN int e_configure_option_init(void);
EINTERN int e_configure_option_shutdown(void);
# endif
#endif
