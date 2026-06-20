/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <list.h>
#include <hardware.h>
#include <stddef.h> 

union task_union task[20] __attribute__((__aligned__(8192)));

char initial_stack[KERNEL_STACK_SIZE]; // Space for the initial system stack

// para los procesos idle + init
struct task_struct *init_task = NULL;
struct task_struct *idle_task = NULL;

// funciones de asm externas
int writeMSR(unsigned long dir,int value); // entry.S
void switch_stack(int *old_esp, int new_esp); // task_switch.S
void task_switch(union task_union* new); // task_switch.S

// variables
struct list_head readyqueue;
int current_quantum;


void cpu_idle(void)
{
	__sti();
	while(1)
	{
	;
	}
}


void init_idle (void) {
	page_table_entry *shared_SPT = get_PT(init_task);

	// Task union
    int idle_frame = alloc_frame();
    set_ss_pag(shared_SPT, idle_frame, idle_frame, 0);  
    union task_union* idle_union = (union task_union*)(idle_frame << 12);
	
	// Directorio
	int Dir = alloc_frame(); // reservamos marco físico
	set_ss_pag(shared_SPT, Dir, Dir, 0);
	page_table_entry* DirAddress = (page_table_entry*)(Dir << 12);
	clear_page_table(DirAddress);
	idle_union->task.dir_pages_baseAddr = DirAddress;


	// Kernel 
	DirAddress[0] = init_task->dir_pages_baseAddr[0];


	// Preparamos su contexto de ejecución
	/**
	 * Cuando el task_switch decide ejecutar idle hace:
	 * 			el esp apunta  al kernel_esp de idle
	 * 			pop ebp
	 *  		ret
	 * Cuando haga pop ebp hacemos que borre un 0(basura) para evitar que nos borre la @ret 
	 * 
	 * ? 
	 * El problema es que idle no entra al principio del task switch, si init se esta ejecutando
	 * y le llega int. de reloj, el hw hace que llame a task switch -> por lo que solo ejecutara la segunda
	 * mitad. Es por eso que al hacer el pop ebp no eliminamos el push ebp que se hace en el task_switch,
	 * sino la dirección de retorno.
	 * 
	 * Pila de Idle: 
	 * 
	 * |-----------------|   						    direcciones bajas (0x00000)
	 * | task_struct     |
	 * |      kernel_esp | ----------
	 * |      .....      |			|
	 * |-----------------|		    |
	 * |                 |			|
	 * |      ......     |			|
	 * |				 |			|
	 * |-----------------|			|
	 * |  	   ebp       | <--------- stack[1022]
	 * |    @cpu_idle    | 			  stack[1023]
	 * |-----------------|            stack[1024] !!NO   direcciones altas (0xFFFFFF)
	 *
	 */
	idle_union->stack[KERNEL_STACK_SIZE - 1] = (unsigned long)cpu_idle; 
	idle_union->stack[KERNEL_STACK_SIZE - 2] = 0;
	idle_union->task.kernel_esp = (unsigned long)&(idle_union->stack[1022]);

	// PID = 0
	idle_union->task.PID = 0;

	// Round Robin 
	idle_union->task.quantum = 15;
	idle_union->task.state = ST_RUN;

	// Fork
	idle_union->task.parent_struct = &(idle_union->task);
	INIT_LIST_HEAD(&(idle_union->task.child));
	INIT_LIST_HEAD(&(idle_union->task.parent));
	
	// variable global para acceder a IDLE
	idle_task = &(idle_union->task);
} 



void init_task1(void) {
	// TP del Directorio
	int Dir = alloc_frame(); // pag.fís del directorio
	page_table_entry* DirAddress = (page_table_entry*)(Dir << 12); // @ de pág.fís.
	clear_page_table(DirAddress); // limpiamos los valores que tenia 

	// TP del kernel
	int Kernel = alloc_frame();
    page_table_entry* KernelAddress = (page_table_entry*)(Kernel << 12);
    clear_page_table(KernelAddress);
    set_kernel_pages(KernelAddress);

	// TP del usuario
	int User = alloc_frame();
	page_table_entry* UserAddress = (page_table_entry*)(User << 12);
	clear_page_table(UserAddress);
	set_user_pages(UserAddress); 

	// Mapeamos para las tres TP's las mismas @lógicas = @físicas
	// ? -> Estamos en real mode, un tiempo de boot en que el SO trabaja con @físicas. 
	// 		Cuando active la paginacíon al pasar al protected mode, al tener @físicas = @lógicas
	// 		no tendrá problemas en localizarlas.
	set_ss_pag(KernelAddress,Dir,Dir,0);
	set_ss_pag(KernelAddress,Kernel,Kernel,0);
	set_ss_pag(KernelAddress,User,User,0);

	// Las dos primeras entradas de Dir apuntan a la TP del sistema y usuario, respectivamente.
	DirAddress[0].entry = 0;
	DirAddress[0].bits.pbase_addr = Kernel;
	DirAddress[0].bits.user = 0;
	DirAddress[0].bits.rw = 1;
	DirAddress[0].bits.present = 1;

	DirAddress[1].entry = 0;
	DirAddress[1].bits.pbase_addr = User;
	DirAddress[1].bits.user = 1;
	DirAddress[1].bits.rw = 1;
	DirAddress[1].bits.present = 1;

	// Task union de INIT
	int union_frame = alloc_frame();
	set_ss_pag(KernelAddress, union_frame, union_frame, 0);
	union task_union* task1_union = (union task_union*)(union_frame << 12);

	// Asignamos su direccion del Dir
	task1_union->task.dir_pages_baseAddr = DirAddress;

	// Modificamos la TSS para que ahora apunte a la pila de INIT.
	// Recordatorio : TSS contiene SS + esp0 , que siempre apuntan al inicio de la 
	// 			      pila del proceso en ready
	tss.esp0 = (DWord)&(task1_union->stack[KERNEL_STACK_SIZE]); // NOTA: Probar a cambiar a unsigned long

	// Modificamos el MSR 0X175
	// Recordatorio: En las fast syscalls, no se accede a la TSS para encontrar la @system_stack,
	// 				 sino que se accede al MSR(Model Specific Registers) para encontrarla. Se 
	// 				 encuentra concretamente en el 0x175 = Operating system stack (esp)
	writeMSR(0x175,(int)&(task1_union->stack[KERNEL_STACK_SIZE]));

	// El registro rc3 apunta al Dir de INIT, al ser el proceso en ejecución
	set_cr3(task1_union->task.dir_pages_baseAddr);

	// Asignamos PID
	task1_union->task.PID = 1;

	// Round Robin
	task1_union->task.quantum = 15; 
	task1_union->task.state = ST_RUN;

	// Primer proceso a ejecutar -> current quantum
	current_quantum = task1_union->task.quantum;

	// Fork
	task1_union->task.parent_struct = &(task1_union->task); 
	INIT_LIST_HEAD(&task1_union->task.child);  
	INIT_LIST_HEAD(&task1_union->task.parent); 

	// Le asignamos la variable global para poder acceder a ella
	init_task = &(task1_union->task);
}



void inner_task_switch(union task_union *new) {
	set_cr3(new->task.dir_pages_baseAddr);

	writeMSR(0x175,(unsigned long)&(new->stack[KERNEL_STACK_SIZE]));
	tss.esp0 = (unsigned long)&(new->stack[KERNEL_STACK_SIZE]);

	switch_stack(&current()->kernel_esp, new->task.kernel_esp); 
}

void init_sched() {
	INIT_LIST_HEAD(&readyqueue);
	INIT_LIST_HEAD(&blocked);
}


page_table_entry * get_DIR (struct task_struct *t)
{
       return t->dir_pages_baseAddr;
}

page_table_entry * get_PT (struct task_struct *t)
{
       return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}

struct task_struct *list_head_to_task_struct(struct list_head *l) {
   return (struct task_struct*)((char*)l - offsetof(struct task_struct, list)); 
}






// Update number of ticks
void update_sched_data_rr(void) {
	--current_quantum;
}

// Devuelve cierto si necesitamos cambiar de proceso, falso en caso contrario
int needs_sched_rr (void) {
	// se ha acabado quantum + hay procesos en ready
    if (current_quantum <= 0 && (!list_empty(&readyqueue))) return 1;
	// idle se esta ejecutando + hay procesos en ready
	if (current()->PID == 0 && !list_empty(&readyqueue)) return 1;
	// todo ok, no cal 
	return 0;
}


void update_process_state_rr (struct task_struct *t,struct list_head *dst_queue) {
	if (t->state != ST_RUN) list_del(&(t->list));

	if (dst_queue != NULL) {
		list_add_tail(&(t->list), dst_queue);
        if (dst_queue == &readyqueue) {
            t->state = ST_READY;
        }
		else {
            t->state = ST_BLOCKED; 
        }
	}
	// el proceso va a RUN
	else {
		t->state = ST_RUN;
	}
} 

void sched_next_rr (void) {
    // por defecto sera idle
    struct task_struct* next_process = idle_task;

    // si hay procesos en la readyqueue cambiamos de proceso
    if (!list_empty(&readyqueue)) {
        // obtenemos su list
        struct list_head* next_list = list_first(&readyqueue);
        // obtenemos el proceso
        next_process = list_head_to_task_struct(next_list);
        
        // actualizamos estado (esto ya hace el list_del internamente)
        update_process_state_rr(next_process, NULL); 
    } 
    else {
        idle_task->state = ST_RUN;
    }

    // actualizamos quantum
    current_quantum = get_quantum(next_process);

    // task_switch
    task_switch((union task_union*)next_process);
}


// Devuelve el quantum del proceso current
int get_quantum(struct task_struct *t) {
	return t->quantum;
}

// Asigna quantum al proceso t
void set_quantum(struct task_struct *t, int new_quantum) {
	t->quantum = new_quantum;
}


void schedule() {
	update_sched_data_rr();

	if (needs_sched_rr()) {
		if (current()->PID != 0) update_process_state_rr(current(),&readyqueue);

		sched_next_rr();
	}
}

