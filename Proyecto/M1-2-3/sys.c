/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>
#include <list.h>

#include <mm_address.h>

#include <sched.h>
#include <errno.h>

char sys_buffer[256];

extern int zeos_ticks;

extern struct list_head readyqueue;

int global_PID = 1;

#define LECTURA 0
#define ESCRIPTURA 1

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; 
  if (permissions!=ESCRIPTURA) return -13; 
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

// write 
int sys_write(int fd, void* buffer, int size) {
  // 1. Check fd
  int id = check_fd(fd, ESCRIPTURA);
  if (id != 0) return id;
  // 2. Check buffer
  if (buffer == NULL) return -EFAULT; // bad address
  if (!access_ok(VERIFY_READ,buffer,size)) return -EFAULT; // bad address
  // 3. Check size
  if (size < 0) return -EINVAL; // invalid argument

  int bytes_left = size;
  int bytes_written = 0;

  while (bytes_left > 0) {
    int chunk_size = (bytes_left > 256 ? 256 : bytes_left);

    int error = copy_from_user(buffer + bytes_written, sys_buffer, chunk_size);
    if (error < 0) return -EFAULT; // bad address

    sys_write_console(sys_buffer, chunk_size);

    bytes_left -= chunk_size;
    bytes_written += chunk_size;
  }
  
  return bytes_written;
}

// gettime
int sys_gettime(void){
	return zeos_ticks;
}

// getpid
int sys_getpid() {
  return current()->PID;
}

// - fork - para el return del hijo
int ret_from_fork() {
  return 0;
}

// fork
int sys_fork(void) {
  // datos del padre
  struct task_struct* parent_struct = current();
  union task_union* parent_union = (union task_union*)parent_struct;
  page_table_entry *parent_SPT = get_PT(parent_struct);
  page_table_entry *parent_UPT = (page_table_entry *)(current()->dir_pages_baseAddr[2].bits.pbase_addr << 12);
  
  // 0. Task union del hijo
  int child_frame = alloc_frame();
  if (child_frame < 0) return -1;
  set_ss_pag(parent_SPT, child_frame, child_frame, 0);

  union task_union* child_union = (union task_union*)(child_frame << 12);
  struct task_struct* child_struct = (struct task_struct*)child_union;

  // 1. COPIAMOS TASK_UNION DEL PADRE
  copy_data(parent_union, child_union, PAGE_SIZE);

  
  // 2. RESERVAMOS DIRECTORIO PARA EL HIJO
  // reservamos marco físico
  int dir_frame = alloc_frame();
  if (dir_frame < 0) {
    free_frame(child_frame);
    return -1; 
  }
  // Dentro de la SPT del padre(current), mapeamos las @ del hijo
  set_ss_pag(parent_SPT, dir_frame, dir_frame, 0);
  // obtenemos @
  page_table_entry* child_dir = (page_table_entry*)(dir_frame << 12);
  // limpiamos lo q habia en el marco
  clear_page_table(child_dir);

  child_struct->dir_pages_baseAddr = child_dir;
  
  // Comparten TP del kernel
  child_dir[0] = parent_struct->dir_pages_baseAddr[0];
  child_dir[1] = parent_struct->dir_pages_baseAddr[1]; //*MD mapeamos ambas tp 
  

  // 3. UPT DEL HIJO
  int user_frame = alloc_frame();
  if (user_frame < 0){
    free_frame(dir_frame);
    free_frame(child_frame);
    return -1;
  }
  set_ss_pag(parent_SPT,user_frame,user_frame,0);
  page_table_entry* child_UPT = (page_table_entry*)(user_frame << 12);
  clear_page_table(child_UPT);
  set_cr3(get_DIR(current()));

  child_dir[2].entry = 0;
  child_dir[2].bits.pbase_addr = user_frame;
  child_dir[2].bits.user = 1;
  child_dir[2].bits.rw = 1;
  child_dir[2].bits.present = 1;

  // 4. ASIGNAMOS FRAMES PARA DATA + STACK
  int new_frames[NUM_PAG_DATA];
  for (int i = 0; i < NUM_PAG_DATA; ++i) {
    new_frames[i] = alloc_frame();

    if (new_frames[i] < 0) {
      for (int j = 0; j < i; j++) {
        free_frame(new_frames[j]);
      }
      free_frame(user_frame);
      free_frame(dir_frame);
      free_frame(child_frame);
      return -1;
    }
  }

  // 5. COMPARTIR CODE
	for (int i = 0; i < NUM_PAG_CODE; i++) {
   set_ss_pag(child_UPT, NUM_PAG_DATA+i, parent_UPT[NUM_PAG_DATA+i].bits.pbase_addr, 1);
	}
  set_cr3(get_DIR(parent_struct));

  // 6. COPIAR DATA + STACK
  // Teoria:
  // Al ser una copia del padre, ambos tienen la misma @lógica. Para poder hacer el copy
  // data hacemos un mapeo temportal en la TP del padre, de manera que esas @lógicas temporales apuntan
  // a las @físicas del hijo. Dhe esta manera podemos hacer el copy data.
  // Al acabar, hacer flush del TLB para eliminar mapeos temporales!!

  int pag_libre_parent = NUM_PAG_DATA + NUM_PAG_CODE;
  /*TP PADRE:
    data/stack: UPT[0..19]
    code:       UPT[20..27]
    temporal:   UPT[28..47]
  */
  //NUM_PAG DATA = 20 frames fisicos tenemos q copiar del padre al hijo
  for (int i = 0; i < NUM_PAG_DATA; ++i) {
    // // TP USER Hijo Mapea en el hijo sus páginas lógica de data+stack para poder llegar
    set_ss_pag(child_UPT, i, new_frames[i], 1); 
    // Padre: Crea los mapeos temporales que hacen referencia a las @físicas del hijo para el copy data
    set_ss_pag(parent_UPT, (pag_libre_parent + i), new_frames[i], 1); 
  }

  set_cr3(get_DIR(parent_struct)); // Flush para actualizar

  // !!!!!!
  // Ahora ya podemos hacer el copy data, ya que pag_libre_parent hace referencia a los marcos físicos del hijo.
  copy_data((void *)(PAG_LOG_INIT_DATA << 12),
            (void *)((PAG_LOG_INIT_DATA + pag_libre_parent) << 12),
            NUM_PAG_DATA * PAGE_SIZE);


  // 6.1 Eliminamos los mapeos temporales del padre
  for (int i = 0; i < NUM_PAG_DATA; i++) {
    del_ss_pag(parent_UPT, (pag_libre_parent + i));
  }
  set_cr3(get_DIR(parent_struct));

  // 7. PREPARACIÓN SYSTEM STACK DE CHILD_PROCESS
  // PROBLEMA 1 :  Los procesos de fork son procesos nuevos, nunca han ejecutado código de usuario.
  //               Por lo tanto,  nunca han ejecutado task_switch.
  //               Eso implica que entraran en medio de la función task_switch, sin haber hecho el dynamic link.
  //               Al deshacer el dynamic link en el task_switch, se haria un pop de la @retorno !!
  // PROBLEMA 2:   El padre tiene que devolver el PID del hijo, y el hijo PID == 0.

  // SOLUCIÓN:     Hacemos un fake dynamic link, por lo que cuando el proceso hijo ejecute el task_switch 
  //               borre "basura".
  // SOLUCIÓN 2:   Para el proceso hijo, haremos que su dirección de retorno sea la función ret_from_fork,
  //               que básicamente hace un return 0 -> %eax = 0 :)

  /** PILA DE CHILD_PROCESS
   * ---------------------
   * |     0 (ebp)       |
   * ---------------------  KERNEL_STACK_SIZE - 19
   * |  @ret_from_fork   |
   * ---------------------  KERNEL_STACK_SIZE - 18
   * | @sysenter_hand    |
   * ---------------------  KERNEL_STACK_SIZE - 17 
   * |    CTX.SW (11)    |
   * ---------------------  KERNEL_STACK_SIZE - 6
   * |  CTX.HW (5 regs)  |
   * ---------------------  KERNEL_STACK_SIZE - 1
  */
  child_union->stack[KERNEL_STACK_SIZE - 18] = (unsigned long)ret_from_fork;  // @return
  child_union->stack[KERNEL_STACK_SIZE - 19] = 0; // fake ebp
  // kernel_esp apunta a ebp
  child_struct->kernel_esp = (unsigned long) &(child_union->stack[KERNEL_STACK_SIZE - 19]);

  //Asignamos PID 
  child_union->task.PID = ++global_PID;

  // Round Robin
	child_union->task.quantum = 15; 

	// Fork
	child_union->task.parent_struct = parent_struct;
	INIT_LIST_HEAD(&child_union->task.child); 
  INIT_LIST_HEAD(&child_union->task.parent);
	list_add_tail(&child_struct->parent, &parent_struct->child);
  // Estado
	child_union->task.state = ST_READY;
  child_union->task.pending_unblocks = 0;
  child_union->task.charsParaLeer = 0;

  // Push a la readyqueue
  list_add_tail(&child_union->task.list, &readyqueue);

  // Padre devuelve PID del hijo
  return child_union->task.PID;
  
}

// exit
void sys_exit(void) {
  // datos 
  struct task_struct *current_process = current();
  page_table_entry *UPT = (page_table_entry *)(current_process->dir_pages_baseAddr[2].bits.pbase_addr << 12);

  //iberamos frames de DATA + STACK
  for (int i = 0; i < NUM_PAG_DATA; ++i) {
    free_frame(UPT[i].bits.pbase_addr);
    del_ss_pag(UPT, i);
  }

  //iberamos UPT + Directorio 
  int upt_frame = current_process->dir_pages_baseAddr[2].bits.pbase_addr;
  free_frame(upt_frame);

  int dir_frame = (int)(current_process->dir_pages_baseAddr) >> 12;
  free_frame(dir_frame);

  //o sacamos de la lista de hijos del padre
  list_del(&current_process->parent);

  struct list_head* aux;
  struct list_head* pos;

  list_for_each_safe(pos,aux, &(current_process->child)) {
    struct task_struct *child = list_entry(pos, struct task_struct, parent);
    // asignamos el nuevo padre
    child->parent_struct = idle_task;
    // lo borramos de la lista de hijos del padre muerto
    list_del(pos);
    // lo añadimos a la nueva lista
    list_add_tail(&child->parent, &idle_task->child); 
  }

  //liberar la union
  int pcb_frame = (int)(current_process) >> 12;
  free_frame(pcb_frame);

  // 6. Ponemos otro proceso a run 
  sched_next_rr();
}

// block
void sys_block(void) {
  // si tenia bloqueos pendientes -> --pending blocks y no bloqueamos
  if (current()->pending_unblocks > 0) {
    --current()->pending_unblocks;
    return;
  }
  // else bloqueamos 
  update_process_state_rr(current(),&blocked);
  sched_next_rr();
}

// unblock
int sys_unblock(int pid) {
  // Buscar el hijo con ese pid
  struct list_head *pos;
  struct task_struct *target = NULL;

  list_for_each(pos, &current()->child) {
    //struct task_struct *child = list_head_to_task_struct(pos);
    struct task_struct *child = list_entry(pos, struct task_struct, parent);
    if (child->PID == pid) {
      target = child;
      break;
    }
  }
  if (target == NULL) return -1;

  // esta blocked -> lo pasamos a ready
  if (target->state == ST_BLOCKED) {
    update_process_state_rr(target, &readyqueue);
  }
  else {
    target->pending_unblocks++;
  }

  return 0;
}


int sys_gotoxy(int x, int y) {
  if (x < 0 || x > 79) return -EINVAL;
  if (y < 0 || y > 24) return -EINVAL;
  set_cursor_pos((Byte)x, (Byte)y);
  return 0;
}

int sys_set_color(int fg, int bg) {
  if (fg < 0 || fg > 15) return -EINVAL;
  if (bg < 0 || bg > 7)  return -EINVAL;
  set_color_attr((Byte)fg, (Byte)bg);
  return 0;
}

int sys_read(char* buffer, int maxchars) {
  int bytes_read = 0; //char leidos

  if (maxchars < 0) return -EINVAL;
  if (maxchars == 0) return 0;
  if (buffer == NULL) return -EFAULT;
  if (!access_ok(VERIFY_WRITE, buffer, maxchars)) return -EFAULT;

  current()->charsParaLeer = maxchars;
  while (!keyboard_can_current_read()) {
    keyboard_block_current();
  }

  while (bytes_read < maxchars) {
    int chunk_size = 0;

    while (keyboard_buffer_empty()) { //bloquea hasta que haya al menos una tecla
      keyboard_block_current();
    }

    //Lee caracteres del buffer circular y los mete en sys_buffer, no leemos mas de lo q pide el user
    while (chunk_size < 256 && bytes_read + chunk_size < maxchars && keyboard_read_char(&sys_buffer[chunk_size])) {
      chunk_size++;
    }

    if (copy_to_user(sys_buffer, buffer + bytes_read, chunk_size) < 0) {
      keyboard_finish_current_read();
      return -EFAULT;
    }//copiamos desde memoria de kernel a usuario, buffer -> dir de usuario

    bytes_read += chunk_size;
    current()->charsParaLeer = maxchars - bytes_read;
  }

  keyboard_finish_current_read();
  return bytes_read;//devolvemos cuanto hemos leido
}
