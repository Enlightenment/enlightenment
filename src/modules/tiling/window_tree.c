#include "window_tree.h"

void
tiling_window_tree_free(Window_Tree *root)
{
   Eina_Inlist *itr_safe;
   Window_Tree *itr;
   EINA_INLIST_FOREACH_SAFE(root->children, itr_safe, itr)
     {
        tiling_window_tree_free(itr);
     }
   free(root);
}

Window_Tree *
tiling_window_tree_add(Window_Tree *parent, E_Client *client)
{
   Window_Tree *new_node = calloc(1, sizeof(*new_node));
   new_node->parent = parent;
   new_node->client = client;
   if (!parent)
     {
        new_node->weight = 1.0;
        return new_node;
     }
   else if (parent->children)
     {
        /* Adjust existing children's weights */
        Window_Tree *itr;
        int children_count = eina_inlist_count(parent->children);
        float weight = 1.0 / (children_count + 1);

        new_node->weight = weight;

        weight *= children_count;
        EINA_INLIST_FOREACH(parent->children, itr)
          {
             itr->weight *= weight;
          }

        parent->children = eina_inlist_append(parent->children,
              EINA_INLIST_GET(new_node));
     }
   else
     {
        /* Make a new node for the parent client and split the weights in half. */
        Window_Tree *new_parent_client = calloc(1, sizeof(*new_node));
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

   return new_node;
}

void
tiling_window_tree_remove(Window_Tree *item)
{
   /* FIXME: Ignoring ilegal deletion of the rood node or items with children atm. */

   int children_count = eina_inlist_count(item->parent->children);
   float weight = (((float) children_count) - 1.0) / children_count;

   if (children_count == 1)
     {
        item->parent->client = item->client;
        item->parent->children = NULL;
     }
   else
     {
        Window_Tree *itr;

        item->parent->children = eina_inlist_remove(item->parent->children,
              EINA_INLIST_GET(item));

        /* Adjust existing children's weights */
        EINA_INLIST_FOREACH(item->parent->children, itr)
          {
             itr->weight /= weight;
          }
     }

  free(item);
}

