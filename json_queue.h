#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <json.h>

struct json_node
{
  struct json_object* json;
  struct json_node* next;
};


struct json_list
{
  struct json_node* head;
  struct json_node* tail;
	int size;
};


struct json_list* json_queue_push(struct json_list*, struct json_object*);
struct json_node* json_queue_pop(struct json_list*);
int json_queue_size(struct json_list*);


struct json_list* queue_create(void);
struct json_list* queue_free(struct json_list* );

