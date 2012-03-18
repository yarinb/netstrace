#include "json_queue.h"

/* int main(void) */
/* { */
/*   struct json_list*  mt = NULL; */

/*   mt = queue_create(); */
/*   json_queue_push(mt, 1); */
/*   json_queue_push(mt, 2); */
/*   json_queue_push(mt, 3); */
/*   json_queue_push(mt, 4); */ 
  
/*   list_print(mt); */

/*   json_queue_pop(mt); */
/*   list_print(mt); */

/*   queue_free(mt);   /1* always remember to free() the malloc()ed memory *1/ */
/*   free(mt);        /1* free() if list is kept separate from free()ing the structure, I think its a good design *1/ */
/*   mt = NULL;      /1* after free() always set that pointer to NULL, C will run havon on you if you try to use a dangling pointer *1/ */

/*   list_print(mt); */

/*   return 0; */
/* } */



/* Will always return the pointer to json_list */
struct json_list* json_queue_push(struct json_list* s, struct json_object* json)
{
  struct json_node* p = malloc( 1 * sizeof(*p) );

  if( NULL == p )
    {
      fprintf(stderr, "IN %s, %s: malloc() failed\n", __FILE__, "list_add");
      return s; 
    }

  p->json= json;
  p->next = NULL;


  if( NULL == s )
    {
      printf("Queue not initialized\n");
      free(p);
      return s;
    }
  else if( NULL == s->head && NULL == s->tail )
    {
      s->head = s->tail = p;
      s->size++;
      return s;
    }
  else if( NULL == s->head || NULL == s->tail )
    {
      fprintf(stderr, "There is something seriously wrong with your assignment of head/tail to the list\n");
      free(p);
      return NULL;
    }
  else
    {
      s->tail->next = p;
      s->tail = p;
    }

  s->size++;
  return s;
}


/* This is a queue and it is FIFO, so we will always remove the first element */
struct json_node* json_queue_pop( struct json_list* s )
{
  struct json_node* h = NULL;
  struct json_node* p = NULL;

  if( NULL == s )
    {
      printf("List is empty\n");
      return NULL;
    }
  else if( NULL == s->head && NULL == s->tail )
    {
      printf("Well, List is empty\n");
      return NULL;
    }
  else if( NULL == s->head || NULL == s->tail )
    {
      printf("There is something seriously wrong with your list\n");
      printf("One of the head/tail is empty while other is not \n");
      return NULL;
    }

  h = s->head;
  p = h->next;
  /* free(h); */
  s->head = p;
  if( NULL == s->head )  s->tail = s->head;   /* The element tail was pointing to is free(), so we need an update */

  s->size--;
  return h;
}
  

/* ---------------------- small helper fucntions ---------------------------------- */
struct json_list* queue_free( struct json_list* s )
{
  while( s->head )
    {
      json_queue_pop(s);
    }

  return s;
}

int json_queue_size(struct json_list* s) {
  return s->size;
}
struct json_list* queue_create(void)
{
  struct json_list* p = malloc( 1 * sizeof(*p));

  if( NULL == p )
    {
      fprintf(stderr, "LINE: %d, malloc() failed\n", __LINE__);
    }

  p->head = p->tail = NULL;
  
  p->size = 0;
  return p;
}
