#include <io.h>
#include <utils.h>
#include <list.h>
#include <sched.h>

// Queue for blocked processes in I/O 
struct list_head keyboard_blocked;


#define KEYBOARD_BUFFER_SIZE 128  //tamaño del buffer
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int keyboard_head = 0;
static int keyboard_tail = 0;
static int keyboard_count = 0;
static struct task_struct *keyboard_reader = NULL;



int sys_write_console(char *buffer,int size)
{
  int i;
  
  for (i=0; i<size; i++)
    printc(buffer[i]);
  
  return size;
}


/* Inicializ el buffer circular de teclado y la cola de procesos bloqueados
 * esperando teclas. LO llamo durante init_sched antes de activar usuario */
void init_keyboard_buffer(void)
{
  keyboard_head = 0;
  keyboard_tail = 0;
  keyboard_count = 0;
  keyboard_reader = NULL;
  INIT_LIST_HEAD(&keyboard_blocked);
}

static void keyboard_wake_reader(struct task_struct *task)
{
  update_process_state_rr(task, &readyqueue);
}

static void keyboard_wake_next_reader(void)
{
  if (!list_empty(&keyboard_blocked)) {
    struct list_head *first = list_first(&keyboard_blocked);
    struct task_struct *task = list_head_to_task_struct(first);

    keyboard_reader = task;
    keyboard_wake_reader(task);
  }
}

/* Inserta una tecla traducida por la interrupcion en el buffer circular
 * */
void keyboard_store_char(char c)
{
  if (keyboard_count < KEYBOARD_BUFFER_SIZE) {
    keyboard_buffer[keyboard_tail] = c;
    keyboard_tail = (keyboard_tail + 1) % KEYBOARD_BUFFER_SIZE;
    keyboard_count++;
  }

  if (keyboard_reader != NULL) {
    if (keyboard_reader->state == ST_BLOCKED) {
      keyboard_wake_reader(keyboard_reader);
    }
  }
  else {
    keyboard_wake_next_reader();
  }
}

/*Indica si no hay teclas pendientes. Lo usa sys_read para decidir si debe
 * bloquear el proceso actual hasta la siguiente interrupcion de teclado */
int keyboard_buffer_empty(void)
{
  return keyboard_count == 0;
}

/*Extrae una tecla del buffer circular. Devuelve 1 si ha leido una tecla y
 * 0 si el buffer estaba vacio. */
int keyboard_read_char(char *c)
{
  if (keyboard_count == 0) return 0;

  *c = keyboard_buffer[keyboard_head];
  keyboard_head = (keyboard_head + 1) % KEYBOARD_BUFFER_SIZE;
  keyboard_count--;

  return 1;
}

int keyboard_can_current_read(void)
{
  if (keyboard_reader == NULL) keyboard_reader = current();

  return keyboard_reader == current();
}

void keyboard_finish_current_read(void)
{
  if (keyboard_reader == current()) {
    keyboard_reader = NULL;
  }

  current()->charsParaLeer = 0;
  keyboard_wake_next_reader();
}

/*Bloquea el proceso actual en la cola especfica de teclado y cede CPU.
 * Hace que read sea bloqueante cuando no hay teclas*/
void keyboard_block_current(void)
{
  update_process_state_rr(current(), &keyboard_blocked);
  sched_next_rr();
}
