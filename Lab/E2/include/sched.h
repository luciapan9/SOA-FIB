/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>

#define KERNEL_STACK_SIZE	1024


enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

/** Task struct **/
struct task_struct {
  int PID; /* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr; 
  int kernel_esp; 

  struct list_head list;   // Tiene que estar en offset 12!
  
  // Round Robin
  int quantum;
  enum state_t state;

  // fork
  struct task_struct *parent_struct;  // por eficiencia, tenemos el task_struct del padre
  struct list_head child; // Nodo para ubicar la lista de mis procesos hijos
  struct list_head parent; // Nodo para atarme a la lista de hijos del padre

  // Process blocking 
  int pending_unblocks;

};

/** Task union **/
union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};


extern struct list_head readyqueue;
extern struct list_head blocked;

extern struct task_struct *idle_task;
extern struct task_struct *init_task;

#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

extern char initial_stack[KERNEL_STACK_SIZE];
#define INITIAL_ESP             (DWord) &initial_stack[KERNEL_STACK_SIZE]


/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

struct task_struct * current();

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

// Para acceder a la task_struct dado el list head
struct task_struct *list_head_to_task_struct(struct list_head *l);

// Process scheduling 
void update_sched_data_rr(void);
int needs_sched_rr (void);
void update_process_state_rr (struct task_struct *t,struct list_head *dst_queue);
void sched_next_rr (void);

int get_quantum(struct task_struct *t);
void set_quantum(struct task_struct *t, int new_quantum);

void schedule();


#endif  /* __SCHED_H__ */
