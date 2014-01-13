#ifndef WINDOW_TREE_H
#define WINDOW_TREE_HO
#include <e.h>

typedef struct _Window_Tree Window_Tree;

struct _Window_Tree
{
   EINA_INLIST;
   Window_Tree *parent;
   /* FIXME: client is falid iff children is null. Sholud enforce it. */
   Eina_Inlist *children; /* Window_Tree * type */
   E_Client *client;
   float weight;
};

#endif
