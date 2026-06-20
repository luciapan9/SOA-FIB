/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <list.h>
#include <hardware.h>

char initial_stack[KERNEL_STACK_SIZE]; // Space for the initial system stack
struct list_head readyqueue;

void cpu_idle(void)
{
	__sti();
	while(1)
	{
	;
	}
}

void init_idle (void)
{

}

void init_task1(void) {

}


void init_sched()
{
	INIT_LIST_HEAD(&readyqueue);
}


// dado el list_head l, nos devuelve el task_struct correspondiente
struct task_struct * list_head_to_task_struct(struct list_head* l) {
	return list_entry(l,struct task_struct,list);
}


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t)
{
       return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t)
{
    return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}

