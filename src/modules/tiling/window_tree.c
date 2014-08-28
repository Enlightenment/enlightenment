#include "e.h"

#include "window_tree.h"
#include "e_mod_tiling.h"

void
tiling_window_tree_walk(Window_Tree *root, void (*func)(void *))
{
   Eina_Inlist *itr_safe;
   Window_Tree *itr;

   if (!root)
     return;

   EINA_INLIST_FOREACH_SAFE(root->children, itr_safe, itr)
     {
        tiling_window_tree_walk(itr, func);
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

   parent->children =
     eina_inlist_append(parent->children, EINA_INLIST_GET(new_parent_client));
   parent->children =
     eina_inlist_append(parent->children, EINA_INLIST_GET(new_node));
}

static void
_tiling_window_tree_parent_add(Window_Tree *parent, Window_Tree *new_node, Window_Tree *rel)
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

   parent->children =
     eina_inlist_append_relative(parent->children, EINA_INLIST_GET(new_node),
           EINA_INLIST_GET(rel));
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
tiling_window_tree_add(Window_Tree *root, Window_Tree *parent,
                       E_Client *client, Tiling_Split_Type split_type)
{
   Window_Tree *orig_parent = parent;
   Window_Tree *new_node = calloc(1, sizeof(*new_node));

   new_node->client = client;
   Tiling_Split_Type parent_split_type;

   if (split_type > TILING_SPLIT_VERTICAL)
     {
        free(new_node);
        return root;
     }
   else if (!root)
     {
        new_node->weight = 1.0;
        return new_node;
     }
   else if (!parent)
     {
        parent = root;
        parent_split_type = _tiling_window_tree_split_type_get(parent);
        if ((parent_split_type != split_type) && (parent->children))
          {
             parent = (Window_Tree *)parent->children;
          }
     }

   parent_split_type = _tiling_window_tree_split_type_get(parent);

   if (parent_split_type == split_type)
     {
        if (parent->children)
          {
             _tiling_window_tree_parent_add(parent, new_node, NULL);
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
             _tiling_window_tree_parent_add(grand_parent, new_node, orig_parent);
          }
        else
          {
             root = calloc(1, sizeof(*root));
             _tiling_window_tree_split_add(parent, new_node);
             root->weight = 1.0;
             root->children =
               eina_inlist_append(root->children, EINA_INLIST_GET(parent));
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

        if (!item_keep)
          {
             /* Special case of deleting the last vertical split item. */
             free(item);
             free(root);
             return NULL;
          }
        else if (!item_keep->children)
          {
             parent->client = item_keep->client;
             parent->children = NULL;

             free(item_keep);
          }
        else
          {

             parent->children =
               eina_inlist_remove(parent->children, EINA_INLIST_GET(item));
             if (grand_parent)
               {
                  /* Update the children's parent. */
                  {
                     Eina_Inlist *itr_safe;
                     Window_Tree *itr;

                     EINA_INLIST_FOREACH_SAFE(item_keep->children, itr_safe,
                                               itr)
                       {
                          /* We are prepending to double-reverse the order. */
                          grand_parent->children =
                            eina_inlist_prepend_relative(grand_parent->children,
                                                         EINA_INLIST_GET(itr), EINA_INLIST_GET(parent));
                          itr->weight *= parent->weight;
                          itr->parent = grand_parent;
                       }

                     grand_parent->children =
                       eina_inlist_remove(grand_parent->children,
                                          EINA_INLIST_GET(parent));
                     free(parent);
                  }
               }
             else
               {
                  /* This is fine, as this is a child of the root so we allow
                   * two levels. */
                  item_keep->weight = 1.0;
               }
          }
     }
   else
     {
        Window_Tree *itr;
        float weight = 1.0 - item->weight;

        parent->children =
          eina_inlist_remove(parent->children, EINA_INLIST_GET(item));

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

void
_tiling_window_tree_level_apply(Window_Tree *root, Evas_Coord x, Evas_Coord y,
                                Evas_Coord w, Evas_Coord h, int level, Evas_Coord padding,
                                Eina_List **floaters)
{
   Window_Tree *itr;
   Tiling_Split_Type split_type = level % 2;
   double total_weight = 0.0;

   if (root->client)
     {
        if (!e_object_is_del(E_OBJECT(root->client)))
          {
             if ((root->client->icccm.min_w > (w - padding)) ||
                   (root->client->icccm.min_h > (h - padding)))
               {
                  *floaters = eina_list_append(*floaters, root->client);
               }
             else
               {
                  tiling_e_client_move_resize_extra(root->client, x, y,
                        w - padding, h - padding);
               }
          }
        return;
     }

   if (split_type == TILING_SPLIT_HORIZONTAL)
     {
        EINA_INLIST_FOREACH(root->children, itr)
          {
             Evas_Coord itw = w * itr->weight;

             total_weight += itr->weight;
             _tiling_window_tree_level_apply(itr, x, y, itw, h, level + 1, padding, floaters);
             x += itw;
          }
     }
   else if (split_type == TILING_SPLIT_VERTICAL)
     {
        EINA_INLIST_FOREACH(root->children, itr)
          {
             Evas_Coord ith = h * itr->weight;

             total_weight += itr->weight;
             _tiling_window_tree_level_apply(itr, x, y, w, ith, level + 1, padding, floaters);
             y += ith;
          }
     }

   /* Adjust the last item's weight in case weight < 1.0 */
   ((Window_Tree *)root->children->last)->weight += 1.0 - total_weight;
}

void
tiling_window_tree_apply(Window_Tree *root, Evas_Coord x, Evas_Coord y,
                         Evas_Coord w, Evas_Coord h, Evas_Coord padding)
{
   Eina_List *floaters = NULL;
   E_Client *ec;

   x += padding;
   y += padding;
   w -= padding;
   h -= padding;
   _tiling_window_tree_level_apply(root, x, y, w, h, 0, padding, &floaters);

   EINA_LIST_FREE(floaters, ec)
     {
        tiling_e_client_does_not_fit(ec);
     }
}

static Window_Tree *
_inlist_next(Window_Tree *it)
{
   return (Window_Tree *)EINA_INLIST_GET(it)->next;
}

static Window_Tree *
_inlist_prev(Window_Tree *it)
{
   return (Window_Tree *)EINA_INLIST_GET(it)->prev;
}

static Eina_Bool
_tiling_window_tree_node_resize_direction(Window_Tree *node,
                                          Window_Tree *parent, double dir_diff, int dir)
{
   double weight = 0.0;
   double weight_diff;
   Window_Tree *children_start;
   Window_Tree *itr;
   Window_Tree *(*itr_func)(Window_Tree *);

   if (dir > 0)
     {
        itr_func = _inlist_prev;
        children_start = (Window_Tree *)parent->children->last;
     }
   else
     {
        itr_func = _inlist_next;
        children_start = (Window_Tree *)parent->children;
     }

   itr = (Window_Tree *)children_start;
   while (itr != node)
     {
        weight += itr->weight;

        itr = itr_func(itr);
     }

   /* If it's at the edge, try the grandpa of the parent. */
   if (weight == 0.0)
     {
        if (parent->parent && parent->parent->parent)
          {
             return _tiling_window_tree_node_resize_direction(parent->parent,
                                                              parent->parent->parent, 1.0 + ((dir_diff - 1.) * node->weight),
                                                              dir);
          }
        return EINA_FALSE;
     }

   weight_diff = itr->weight;
   itr->weight *= dir_diff;
   weight_diff -= itr->weight;
   weight_diff /= weight;

   for (itr = children_start; itr != node; itr = itr_func(itr))
     {
        itr->weight += itr->weight * weight_diff;
     }

   return EINA_TRUE;
}

Eina_Bool
tiling_window_tree_node_resize(Window_Tree *node, int w_dir, double w_diff,
                               int h_dir, double h_diff)
{
   Window_Tree *parent = node->parent;
   Window_Tree *w_parent, *h_parent;
   Eina_Bool ret = EINA_FALSE;

   /* If we have no parent, means we need to be full screen anyway. */
   if (!parent)
     return EINA_FALSE;

   Window_Tree *grand_parent = parent->parent;
   Tiling_Split_Type parent_split_type =
     _tiling_window_tree_split_type_get(parent);

   /* w_diff related changes. */
   if (parent_split_type == TILING_SPLIT_HORIZONTAL)
     {
        w_parent = parent;
        h_parent = grand_parent;
     }
   else
     {
        w_parent = grand_parent;
        h_parent = parent;
     }

   if ((h_diff != 1.0) && h_parent)
     {
        Window_Tree *tmp_node = (h_parent == parent) ? node : parent;

        ret = ret ||
          _tiling_window_tree_node_resize_direction(tmp_node, h_parent,
                                                    h_diff, h_dir);
     }

   if ((w_diff != 1.0) && w_parent)
     {
        Window_Tree *tmp_node = (w_parent == parent) ? node : parent;

        ret = ret ||
          _tiling_window_tree_node_resize_direction(tmp_node, w_parent,
                                                    w_diff, w_dir);
     }

   return ret;
}

int
_tiling_window_tree_edges_get_helper(Window_Tree *node,
                                     Tiling_Split_Type split_type, Eina_Bool gave_up_this,
                                     Eina_Bool gave_up_parent)
{
   int ret =
     TILING_WINDOW_TREE_EDGE_LEFT | TILING_WINDOW_TREE_EDGE_RIGHT |
     TILING_WINDOW_TREE_EDGE_TOP | TILING_WINDOW_TREE_EDGE_BOTTOM;
   if (!node->parent)
     {
        return ret;
     }
   else if (gave_up_this && gave_up_parent)
     {
        return 0;
     }
   else if (gave_up_this)
     {
        /* Mixed the gave_up vals on purpose, we do it on every call. */
        return _tiling_window_tree_edges_get_helper(node->parent, !split_type,
                                                    gave_up_parent, gave_up_this);
     }

   if (EINA_INLIST_GET(node)->prev)
     {
        gave_up_this = EINA_TRUE;
        ret ^=
          (split_type ==
           TILING_SPLIT_HORIZONTAL) ? TILING_WINDOW_TREE_EDGE_LEFT :
          TILING_WINDOW_TREE_EDGE_TOP;
     }

   if (EINA_INLIST_GET(node)->next)
     {
        gave_up_this = EINA_TRUE;
        ret ^=
          (split_type ==
           TILING_SPLIT_HORIZONTAL) ? TILING_WINDOW_TREE_EDGE_RIGHT :
          TILING_WINDOW_TREE_EDGE_BOTTOM;
     }

   /* Mixed the gave_up vals on purpose, we do it on every call. */
   return ret & _tiling_window_tree_edges_get_helper(node->parent, !split_type,
                                                     gave_up_parent, gave_up_this);
}

int
tiling_window_tree_edges_get(Window_Tree *node)
{
   Tiling_Split_Type split_type = _tiling_window_tree_split_type_get(node);

   return _tiling_window_tree_edges_get_helper(node, !split_type, EINA_FALSE,
                                               EINA_FALSE);
}

/* Node move */
static struct _Node_Move_Context
{
   Window_Tree *node;
   Window_Tree *ret;
   int          cross_edge;
   int          best_match;
} _node_move_ctx;

#define CNODE (_node_move_ctx.node)

#define IF_MATCH_SET_LR(node) _tiling_window_tree_node_move_if_match_set(node, \
                                                                         CNODE->client->y, CNODE->client->h, node->client->y, node->client->h)
#define IF_MATCH_SET_TB(node) _tiling_window_tree_node_move_if_match_set(node, \
                                                                         CNODE->client->x, CNODE->client->w, node->client->x, node->client->w)

static void
_tiling_window_tree_node_move_if_match_set(Window_Tree *node, Evas_Coord cx,
                                           Evas_Coord cw, Evas_Coord ox, Evas_Coord ow)
{
   Evas_Coord leftx, rightx;
   int match;

   leftx = _TILE_MAX(cx, ox);
   rightx = _TILE_MIN(cx + cw, ox + ow);
   match = rightx - leftx;

   if (match > _node_move_ctx.best_match)
     {
        _node_move_ctx.best_match = match;
        _node_move_ctx.ret = node;
     }
}

static void
_tiling_window_tree_node_move_walker(void *_node)
{
   Window_Tree *node = _node;
   int p = tiling_g.config->window_padding;
   /* We are only interested in nodes with clients. */
   if (!node->client)
     return;

   switch (_node_move_ctx.cross_edge)
     {
      case TILING_WINDOW_TREE_EDGE_LEFT:
        if ((node->client->x + node->client->w + p) == CNODE->client->x)
          IF_MATCH_SET_LR(node);
        break;

      case TILING_WINDOW_TREE_EDGE_RIGHT:
        if (node->client->x == (CNODE->client->x + CNODE->client->w + p))
          IF_MATCH_SET_LR(node);
        break;

      case TILING_WINDOW_TREE_EDGE_TOP:
        if ((node->client->y + node->client->h + p) == CNODE->client->y)
          IF_MATCH_SET_TB(node);
        break;

      case TILING_WINDOW_TREE_EDGE_BOTTOM:
        if (node->client->y == (CNODE->client->y + CNODE->client->h + p))
          IF_MATCH_SET_TB(node);
        break;

      default:
        break;
     }
}

#undef CNODE
#undef IF_MATCH_SET_LR
#undef IF_MATCH_SET_TB

void
tiling_window_tree_node_move(Window_Tree *node, int cross_edge)
{
   Window_Tree *root = node;

   /* FIXME: This is very slow and possibly buggy. Can be done much better, but
    * is very easy to implement. */

   while (root->parent)
     root = root->parent;

   _node_move_ctx.node = node;
   _node_move_ctx.cross_edge = cross_edge;
   _node_move_ctx.ret = NULL;
   _node_move_ctx.best_match = 0;

   tiling_window_tree_walk(root, _tiling_window_tree_node_move_walker);

   if (_node_move_ctx.ret)
     {
        E_Client *ec = node->client;

        node->client = _node_move_ctx.ret->client;
        _node_move_ctx.ret->client = ec;
     }
}

/* End Node move. */

void
tiling_window_tree_dump(Window_Tree *root, int level)
{
   int i;

   if (!root)
     return;

   for (i = 0; i < level; i++)
     printf(" ");

   if (root->children)
     printf("\\-");
   else
     printf("|-");

   printf("%f (%p)\n", root->weight, root->client);

   Window_Tree *itr;

   EINA_INLIST_FOREACH(root->children, itr)
     {
        tiling_window_tree_dump(itr, level + 1);
     }
}

