#ifndef E_UUID_STORE_H
#define E_UUID_STORE_H

/* vim:ts=8 sw=3 sts=3 expandtab cino=>5n-3f0^-2{2(0W1st0
 */

#define UUID_STORE_TABLE_SIZE 100

struct table_entry {
   uuid_t uuid;
   /* data structure for per application properties */
   Evas_Coord x, y;
   Evas_Coord width, heigth;
   unsigned int virtual_desktop;
   int flags;
};

struct uuid_table {
   int version; /* Version to allow future extensions */
   unsigned int entry_count; /* Entry counter to allow excat memory consuptions needs? */
   /* Global settings like current virtual desktop, screen setup, etc */
   struct table_entry entries[UUID_STORE_TABLE_SIZE]; /* FIXME make this more adjustable */
};

struct uuid_store {
   struct uuid_table *table;
   int shmfd;
};

EINTERN int e_uuid_store_init(void);
EINTERN int e_uuid_store_shutdown(void);
E_API void e_uuid_dump(void);
E_API Eina_Bool e_uuid_store_reload(void);
E_API Eina_Bool e_uuid_store_entry_del(uuid_t uuid);
E_API Eina_Bool e_uuid_store_entry_update(uuid_t uuid, E_Client *ec);
#endif
