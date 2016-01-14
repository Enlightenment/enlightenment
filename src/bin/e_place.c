#include "e.h"

E_API void
e_place_zone_region_smart_cleanup(E_Zone *zone)
{
   E_Desk *desk;
   Eina_List *clients = NULL;
   E_Client *ec;

   E_OBJECT_CHECK(zone);
   desk = e_desk_current_get(zone);
   E_CLIENT_FOREACH(ec)
     {
        /* Build a list of windows on this desktop and not iconified. */
        if ((ec->desk == desk) && (!ec->iconic) &&
            (!ec->lock_user_location) && (!e_client_util_ignored_get(ec)))
          {
             int area;
             Eina_List *ll;
             E_Client *ec2;

             /* Ordering windows largest to smallest gives better results */
             area = ec->w * ec->h;
             EINA_LIST_FOREACH(clients, ll, ec2)
               {
                  int testarea;

                  testarea = ec2->w * ec2->h;
                  /* Insert the ec if larger than the current ec */
                  if (area >= testarea)
                    {
                       clients = eina_list_prepend_relative(clients, ec2, ec);
                       break;
                    }
               }
             /* Looped over all clients without placing, so place at end */
             if (!ll) clients = eina_list_append(clients, ec);
          }
     }

   /* Loop over the clients moving each one using the smart placement */
   EINA_LIST_FREE(clients, ec)
     {
        int new_x, new_y;

        e_place_zone_region_smart(zone, clients, ec->x, ec->y,
                                  ec->w, ec->h, &new_x, &new_y);
        evas_object_move(ec->frame, new_x, new_y);
     }
}

static int
_e_place_cb_sort_cmp(const void *v1, const void *v2)
{
   return (*((int *)v1)) - (*((int *)v2));
}

static int
_e_place_coverage_client_add(E_Desk *desk, Eina_List *skiplist, int ar, int x, int y, int w, int h)
{
   E_Client *ec;
   int x2, y2, w2, h2;
   int iw, ih;
   int x0, x00, yy0, y00;

   E_CLIENT_FOREACH(ec)
     {
        if (eina_list_data_find(skiplist, ec)) continue;
        if (e_client_util_ignored_get(ec)) continue;
        x2 = (ec->x - desk->zone->x); y2 = (ec->y - desk->zone->y); w2 = ec->w; h2 = ec->h;
        if (E_INTERSECTS(x, y, w, h, x2, y2, w2, h2) &&
            ((ec->sticky) || (ec->desk == desk)) &&
            (!ec->iconic) && (ec->visible))
          {
             x0 = x;
             if (x < x2) x0 = x2;
             x00 = (x + w);
             if ((x2 + w2) < (x + w)) x00 = (x2 + w2);
             yy0 = y;
             if (y < y2) yy0 = y2;
             y00 = (y + h);
             if ((y2 + h2) < (y + h)) y00 = (y2 + h2);
             iw = x00 - x0;
             ih = y00 - yy0;
             ar += (iw * ih);
          }
     }
   return ar;
}

static int
_e_place_coverage_zone_obstacles_add_single(E_Zone_Obstacle *obs, int ar, int x, int y, int w, int h)
{
   int x2, y2, w2, h2;
   int x0, x00, yy0, y00;
   int iw, ih;

   x2 = obs->x; y2 = obs->y; w2 = obs->w; h2 = obs->h;
   if (!E_INTERSECTS(x, y, w, h, x2, y2, w2, h2)) return ar;

   /* FIXME: this option implies that windows should be resized when
    * an autohide shelf toggles its visibility, but it is not used correctly
    * and is instead used to determine whether shelves can be overlapped
    */
   if (!e_config->border_fix_on_shelf_toggle) return 0x7fffffff;

   x0 = x;
   if (x < x2) x0 = x2;
   x00 = (x + w);
   if ((x2 + w2) < (x + w)) x00 = (x2 + w2);
   yy0 = y;
   if (y < y2) yy0 = y2;
   y00 = (y + h);
   if ((y2 + h2) < (y + h)) y00 = (y2 + h2);
   iw = x00 - x0;
   ih = y00 - yy0;
   ar += (iw * ih);
   return ar;
}

static int
_e_place_coverage_zone_obstacles_add(E_Desk *desk, int ar, int x, int y, int w, int h)
{
   E_Zone_Obstacle *obs;

   EINA_INLIST_FOREACH(desk->obstacles, obs)
     ar = _e_place_coverage_zone_obstacles_add_single(obs, ar, x, y, w, h);
   EINA_INLIST_FOREACH(desk->zone->obstacles, obs)
     ar = _e_place_coverage_zone_obstacles_add_single(obs, ar, x, y, w, h);
   return ar;
}

E_API int
e_place_desk_region_smart(E_Desk *desk, Eina_List *skiplist, int x, int y, int w, int h, int *rx, int *ry)
{
   int a_w = 0, a_h = 0, a_alloc_w = 0, a_alloc_h = 0;
   int *a_x = NULL, *a_y = NULL;
   int zw, zh;
   char *u_x = NULL, *u_y = NULL;
   E_Client *ec;

   *rx = x;
   *ry = y;
#if 0
   /* DISABLE placement entirely for speed testing */
   return 1;
#endif

   if ((w <= 0) || (h <= 0))
     {
        printf("EEEK! trying to place 0x0 window!!!!\n");
        return 1;
     }

   /* FIXME: this NEEDS optimizing */
   a_w = 2;
   a_h = 2;
   a_x = E_NEW(int, 2);
   a_y = E_NEW(int, 2);
   a_alloc_w = 2;
   a_alloc_h = 2;

   zw = desk->zone->w;
   zh = desk->zone->h;

   u_x = calloc(zw + 1, sizeof(char));
   u_y = calloc(zh + 1, sizeof(char));

   a_x[0] = 0;
   a_x[1] = zw;
   a_y[0] = 0;
   a_y[1] = zh;

   u_x[0] = 1;
   u_x[zw] = 1;
   u_y[0] = 1;
   u_y[zh] = 1;

   if (e_config->window_placement_policy == E_WINDOW_PLACEMENT_SMART)
     {
        Eina_List *l;
        E_Shelf *es;

        l = e_shelf_list_all();
        EINA_LIST_FREE(l, es)
          {
             int bx, by, bw, bh;

             if (!e_shelf_desk_visible(es, desk)) continue;

             bx = es->x;
             by = es->y;
             bw = es->w;
             bh = es->h;
             if (!E_INTERSECTS(bx, by, bw, bh, 0, 0, zw, zh)) continue;

             if (bx < 0)
               {
                  bw += bx;
                  bx = 0;
               }
             if ((bx + bw) > zw) bw = zw - bx;
             if (bx >= zw) continue;
             if (by < 0)
               {
                  bh += by;
                  by = 0;
               }
             if ((by + bh) > zh) bh = zh - by;
             if (by >= zh) continue;
             if (!u_x[bx])
               {
                  a_w++;
                  if (a_w > a_alloc_w)
                    {
                       a_alloc_w += 32;
                       E_REALLOC(a_x, int, a_alloc_w);
                    }
                  a_x[a_w - 1] = bx;
                  u_x[bx] = 1;
               }
             if (!u_x[bx + bw])
               {
                  a_w++;
                  if (a_w > a_alloc_w)
                    {
                       a_alloc_w += 32;
                       E_REALLOC(a_x, int, a_alloc_w);
                    }
                  a_x[a_w - 1] = bx + bw;
                  u_x[bx + bw] = 1;
               }
             if (!u_y[by])
               {
                  a_h++;
                  if (a_h > a_alloc_h)
                    {
                       a_alloc_h += 32;
                       E_REALLOC(a_y, int, a_alloc_h);
                    }
                  a_y[a_h - 1] = by;
                  u_y[by] = 1;
               }
             if (!u_y[by + bh])
               {
                  a_h++;
                  if (a_h > a_alloc_h)
                    {
                       a_alloc_h += 32;
                       E_REALLOC(a_y, int, a_alloc_h);
                    }
                  a_y[a_h - 1] = by + bh;
                  u_y[by + bh] = 1;
               }
          }
     }

   E_CLIENT_FOREACH(ec)
     {
        int bx, by, bw, bh;

        if (e_client_util_ignored_get(ec)) continue;

        if (eina_list_data_find(skiplist, ec)) continue;

        if (!((ec->sticky) || (ec->desk == desk))) continue;

        bx = ec->x - desk->zone->x;
        by = ec->y - desk->zone->y;
        bw = ec->w;
        bh = ec->h;

        if (E_INTERSECTS(bx, by, bw, bh, 0, 0, zw, zh))
          {
             if (bx < 0)
               {
                  bw += bx;
                  bx = 0;
               }
             if ((bx + bw) > zw) bw = zw - bx;
             if (bx >= zw) continue;
             if (by < 0)
               {
                  bh += by;
                  by = 0;
               }
             if ((by + bh) > zh) bh = zh - by;
             if (by >= zh) continue;
             if (!u_x[bx])
               {
                  a_w++;
                  if (a_w > a_alloc_w)
                    {
                       a_alloc_w += 32;
                       E_REALLOC(a_x, int, a_alloc_w);
                    }
                  a_x[a_w - 1] = bx;
                  u_x[bx] = 1;
               }
             if (!u_x[bx + bw])
               {
                  a_w++;
                  if (a_w > a_alloc_w)
                    {
                       a_alloc_w += 32;
                       E_REALLOC(a_x, int, a_alloc_w);
                    }
                  a_x[a_w - 1] = bx + bw;
                  u_x[bx + bw] = 1;
               }
             if (!u_y[by])
               {
                  a_h++;
                  if (a_h > a_alloc_h)
                    {
                       a_alloc_h += 32;
                       E_REALLOC(a_y, int, a_alloc_h);
                    }
                  a_y[a_h - 1] = by;
                  u_y[by] = 1;
               }
             if (!u_y[by + bh])
               {
                  a_h++;
                  if (a_h > a_alloc_h)
                    {
                       a_alloc_h += 32;
                       E_REALLOC(a_y, int, a_alloc_h);
                    }
                  a_y[a_h - 1] = by + bh;
                  u_y[by + bh] = 1;
               }
          }
     }
   qsort(a_x, a_w, sizeof(int), _e_place_cb_sort_cmp);
   qsort(a_y, a_h, sizeof(int), _e_place_cb_sort_cmp);
   free(u_x);
   free(u_y);

   {
      int i, j;
      int area = 0x7fffffff;

      if ((x <= (zw - w)) &&
          (y <= (zh - h)))
        {
           int ar = 0;

           ar = _e_place_coverage_client_add(desk, skiplist, ar,
                                             x, y,
                                             w, h);
           if (e_config->window_placement_policy == E_WINDOW_PLACEMENT_SMART)
             ar = _e_place_coverage_zone_obstacles_add(desk, ar,
                                              x, y,
                                              w, h);
           if (ar < area)
             {
                area = ar;
                *rx = x;
                *ry = y;
                if (ar == 0) goto done;
             }
        }

      for (j = 0; j < a_h - 1; j++)
        {
           for (i = 0; i < a_w - 1; i++)
             {
                if ((a_x[i] <= (zw - w)) &&
                    (a_y[j] <= (zh - h)))
                  {
                     int ar = 0;

                     ar = _e_place_coverage_client_add(desk, skiplist, ar,
                                                       a_x[i], a_y[j],
                                                       w, h);
                     if (e_config->window_placement_policy == E_WINDOW_PLACEMENT_SMART)
                       ar = _e_place_coverage_zone_obstacles_add(desk, ar,
                                                        a_x[i], a_y[j],
                                                        w, h);
                     if (ar < area)
                       {
                          area = ar;
                          *rx = a_x[i];
                          *ry = a_y[j];
                          if (ar == 0) goto done;
                       }
                  }
                if ((a_x[i + 1] - w > 0) && (a_y[j] <= (zh - h)))
                  {
                     int ar = 0;

                     ar = _e_place_coverage_client_add(desk, skiplist, ar,
                                                       a_x[i + 1] - w, a_y[j],
                                                       w, h);
                     if (e_config->window_placement_policy == E_WINDOW_PLACEMENT_SMART)
                       ar = _e_place_coverage_zone_obstacles_add(desk, ar,
                                                        a_x[i + 1] - w, a_y[j],
                                                        w, h);
                     if (ar < area)
                       {
                          area = ar;
                          *rx = a_x[i + 1] - w;
                          *ry = a_y[j];
                          if (ar == 0) goto done;
                       }
                  }
                if ((a_x[i + 1] - w > 0) && (a_y[j + 1] - h > 0))
                  {
                     int ar = 0;

                     ar = _e_place_coverage_client_add(desk, skiplist, ar,
                                                       a_x[i + 1] - w, a_y[j + 1] - h,
                                                       w, h);
                     if (e_config->window_placement_policy == E_WINDOW_PLACEMENT_SMART)
                       ar = _e_place_coverage_zone_obstacles_add(desk, ar,
                                                        a_x[i + 1] - w, a_y[j + 1] - h,
                                                        w, h);
                     if (ar < area)
                       {
                          area = ar;
                          *rx = a_x[i + 1] - w;
                          *ry = a_y[j + 1] - h;
                          if (ar == 0) goto done;
                       }
                  }
                if ((a_x[i] <= (zw - w)) && (a_y[j + 1] - h > 0))
                  {
                     int ar = 0;

                     ar = _e_place_coverage_client_add(desk, skiplist, ar,
                                                       a_x[i], a_y[j + 1] - h,
                                                       w, h);
                     if (e_config->window_placement_policy == E_WINDOW_PLACEMENT_SMART)
                       ar = _e_place_coverage_zone_obstacles_add(desk, ar,
                                                        a_x[i], a_y[j + 1] - h,
                                                        w, h);
                     if (ar < area)
                       {
                          area = ar;
                          *rx = a_x[i];
                          *ry = a_y[j + 1] - h;
                          if (ar == 0) goto done;
                       }
                  }
             }
        }
   }
done:
   E_FREE(a_x);
   E_FREE(a_y);

   if ((*rx + w) > desk->zone->w) *rx = desk->zone->w - w;
   if (*rx < 0) *rx = 0;
   if ((*ry + h) > desk->zone->h) *ry = desk->zone->h - h;
   if (*ry < 0) *ry = 0;

//   printf("0 - PLACE %i %i | %ix%i\n", *rx, *ry, w, h);

   *rx += desk->zone->x;
   *ry += desk->zone->y;
   return 1;
}

E_API int
e_place_zone_region_smart(E_Zone *zone, Eina_List *skiplist, int x, int y, int w, int h, int *rx, int *ry)
{
   return e_place_desk_region_smart(e_desk_current_get(zone), skiplist,
                                    x, y, w, h, rx, ry);
}

E_API int
e_place_zone_cursor(E_Zone *zone, int x EINA_UNUSED, int y EINA_UNUSED, int w, int h, int it, int *rx, int *ry)
{
   int cursor_x = 0, cursor_y = 0;
   int zone_right, zone_bottom;

   E_OBJECT_CHECK_RETURN(zone, 0);

   ecore_evas_pointer_xy_get(e_comp->ee, &cursor_x, &cursor_y);
   *rx = cursor_x - (w >> 1);
   *ry = cursor_y - (it >> 1);

   if (*rx < zone->x)
     *rx = zone->x;

   if (*ry < zone->y)
     *ry = zone->y;

   zone_right = zone->x + zone->w;
   zone_bottom = zone->y + zone->h;

   if ((*rx + w) > zone_right)
     *rx = zone_right - w;

   if ((*ry + h) > zone_bottom)
     *ry = zone_bottom - h;

   return 1;
}

E_API int
e_place_zone_manual(E_Zone *zone, int w, int h, int *rx, int *ry)
{
   int cursor_x = 0, cursor_y = 0;

   E_OBJECT_CHECK_RETURN(zone, 0);

   ecore_evas_pointer_xy_get(e_comp->ee, &cursor_x, &cursor_y);
   if (rx) *rx = cursor_x - (w >> 1);
   if (ry) *ry = cursor_y - (h >> 1);

   return 1;
}

