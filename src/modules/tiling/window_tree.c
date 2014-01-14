#include "e.h"

#include "window_tree.h"

void
tiling_window_tree_walk(Window_Tree *root, void (*func)(void *))
{
   Eina_Inlist *itr_safe;
   Window_Tree *itr;
   if (!root)
      return;

   EINA_INLIST_FOREACH_SAFE(root->children, itr_safe, itr)
     {
        tiling_window_tree_free(itr);
     }
   func(root);
}

void
tiling_window_tree_free(Window_Tree *root)
{
   tiling_window_tree_walk(root, free);
}

static void
_tiling_window_tree_split_add(Window_Tree *parent, Window_Tree *new_node)
{
   /* Make a new node for the parent client and split the weights in half. */
   Window_Tree *new_parent_client = calloc(1, sizeof(*new_node));
   new_node->parent = parent;
   new_parent_client->parent = parent;
   new_parent_client->client = parent->client;
   parent->client = NULL;
   new_parent_client->weight = 0.5;
   new_node->weight = 0.5;

   parent->children = eina_inlist_append(parent->children,
         EINA_INLIST_GET(new_parent_client));
   parent->children = eina_inlist_append(parent->children,
         EINA_INLIST_GET(new_node));
}

static void
_tiling_window_tree_parent_add(Window_Tree *parent, Window_Tree *new_node)
{
   /* Adjust existing children's weights */
   Window_Tree *itr;
   int children_count = eina_inlist_count(parent->children);
   float weight = 1.0 / (children_count + 1);

   new_node->parent = parent;
   new_node->weight = weight;

   weight *= children_count;
   EINA_INLIST_FOREACH(parent->children, itr)
     {
        itr->weight *= weight;
     }

   parent->children = eina_inlist_append(parent->children,
         EINA_INLIST_GET(new_node));
}

static int
_tiling_window_tree_split_type_get(Window_Tree *node)
{
   int ret = 0;
   while (node->parent)
     {
        ret++;
        node = node->parent;
     }

   return ret % 2;
}

Window_Tree *
tiling_window_tree_add(Window_Tree *root, Window_Tree *parent, E_Client *client, Tiling_Split_Type split_type)
{
   Window_Tree *new_node = calloc(1, sizeof(*new_node));
   new_node->client = client;
   Tiling_Split_Type parent_split_type;

   if (!root)
     {
        new_node->weight = 1.0;
        return new_node;
     }
   else if (!parent)
     {
        parent = root;
     }

   parent_split_type = _tiling_window_tree_split_type_get(parent);

   if (parent_split_type == split_type)
     {
        if (parent->children)
          {
             _tiling_window_tree_parent_add(parent, new_node);
          }
        else
          {
             _tiling_window_tree_split_add(parent, new_node);
          }
     }
   else
     {
        Window_Tree *grand_parent = parent->parent;
        if (grand_parent && grand_parent->children)
          {
             _tiling_window_tree_parent_add(grand_parent, new_node);
          }
        else
          {
             root = calloc(1, sizeof(*root));
             _tiling_window_tree_split_add(parent, new_node);
             root->weight = 1.0;
             root->children = eina_inlist_append(root->children,
                   EINA_INLIST_GET(parent));
             parent->parent = root;
          }
     }

   return root;
}

Window_Tree *
tiling_window_tree_remove(Window_Tree *root, Window_Tree *item)
{
   if (root == item)
     {
        free(item);
        return NULL;
     }
   else if (!item->client)
     {
        ERR("Tried deleting node %p that doesn't have a client.", item);
        return root;
     }


   Window_Tree *parent = item->parent;
   int children_count = eina_inlist_count(item->parent->children);

   if (children_count <= 2)
     {
        Window_Tree *grand_parent = parent->parent;
        Window_Tree *item_keep = NULL;
        /* Adjust existing children's weights */
        EINA_INLIST_FOREACH(parent->children, item_keep)
          {
             if (item_keep != item)
                break;
          }

        if (!item_keep->children)
          {
             parent->client = item_keep->client;

             free(item_keep);
          }
        else if (grand_parent)
          {
             /* Update the children's parent. */
               {
                  Eina_Inlist *itr_safe;
                  Window_Tree *itr;

                  EINA_INLIST_FOREACH_SAFE(item_keep->children, itr_safe, itr)
                    {
                       grand_parent->children =
                          eina_inlist_append_relative(grand_parent->children,
                                EINA_INLIST_GET(itr), EINA_INLIST_GET(parent));
                       itr->weight *= parent->weight;
                       itr->parent = grand_parent;
                    }

                  grand_parent->children = eina_inlist_remove(grand_parent->children,
                        EINA_INLIST_GET(parent));
                  free(parent);
               }
          }
        else
          {
             /* This is fine, as this is a child of the root so we allow two
              * levels. */
          }

        parent->children = eina_inlist_remove(parent->children, EINA_INLIST_GET(item));
     }
   else
     {
        Window_Tree *itr;
        float weight = 1.0 - item->weight;

        parent->children = eina_inlist_remove(parent->children,
              EINA_INLIST_GET(item));

        /* Adjust existing children's weights */
        EINA_INLIST_FOREACH(parent->children, itr)
          {
             itr->weight /= weight;
          }
     }

  free(item);
  return root;
}

Window_Tree *
tiling_window_tree_client_find(Window_Tree *root, E_Client *client)
{
   Window_Tree *itr;

   if (!client)
      return NULL;

   if (!root || (root->client == client))
      return root;

   EINA_INLIST_FOREACH(root->children, itr)
     {
        Window_Tree *ret;
        ret = tiling_window_tree_client_find(itr, client);
        if (ret)
           return ret;
     }

   return NULL;
}

/* FIXME: Deduplicate this func. */
static void
_e_client_move_resize(E_Client *ec,
                      int       x,
                      int       y,
                      int       w,
                      int       h)
{
    DBG("%p -> %dx%d+%d+%d", ec, w, h, x, y);
    evas_object_geometry_set(ec->frame, x, y, w, h);
}

void
_tiling_window_tree_level_apply(Window_Tree *root, Evas_Coord x, Evas_Coord y,
      Evas_Coord w, Evas_Coord h, int level)
{
   Window_Tree *itr;
   Tiling_Split_Type split_type = level % 2;

   if (root->client)
      _e_client_move_resize(root->client, x, y, w, h);

   if (split_type == TILING_SPLIT_HORIZONTAL)
     {
        EINA_INLIST_FOREACH(root->children, itr)
          {
             Evas_Coord itw = w * itr->weight;
             _tiling_window_tree_level_apply(itr, x, y, itw, h, level + 1);
             x += itw;
          }
     }
   else if (split_type == TILING_SPLIT_VERTICAL)
     {
        EINA_INLIST_FOREACH(root->children, itr)
          {
             Evas_Coord ith = h * itr->weight;
             _tiling_window_tree_level_apply(itr, x, y, w, ith, level + 1);
             y += ith;
          }
     }
}

void
tiling_window_tree_apply(Window_Tree *root, Evas_Coord x, Evas_Coord y,
      Evas_Coord w, Evas_Coord h)
{
   _tiling_window_tree_level_apply(root, x, y, w, h, 0);
}

void
tiling_window_tree_dump(Window_Tree *root, int level)
{
   int i;

   if (!root)
      return;

   for (i = 0 ; i < level ; i++)
      printf(" ");

   if (root->children)
      printf("\\-");
   else
      printf("|-");

   printf("%f\n", root->weight);

   Window_Tree *itr;
   EINA_INLIST_FOREACH(root->children, itr)
     {
        tiling_window_tree_dump(itr, level + 1);
     }
}
