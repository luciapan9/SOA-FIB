#include <asm.h>

# Creamos un nuevo fichero .S en vez de usar wrapper.S, 
# ya que wrapper.S esta mas orientada a llamadas al sistema


/** Pila actual  **/
# ----------------
# |     ebp      | <- ebp del inner_task_switch 
# | @ret_wrapper | <- @return para inner_task_switch
# |  param. new  | <- parámtro new pasado en inner_task_switch(new)
# |     ebx      | <- registro a salvar para asegurar que no se modifica
# |     ebp      | <- dynamic link del task_switch 
# |  @ret_caller | <- al acabar task_switch, volver al caller
# |  param. new  | <- el parámetro que nos ha pasado schedule
# |---------------

/** Handler del task_switch, primera parte**/
ENTRY (task_switch)
    #dynamic link
    pushl %ebp 
    movl %esp,%ebp 

    # salvamos los registros que vayamos a modificar
    pushl %ebx
    pushl %edx

    pushl 8(%ebp) # parámetro new
    call inner_task_switch 

    addl $4,%esp # nos saltamos el parám new 

    # restauramos registros
    popl %edx
    popl %ebx

    # undo dynamic link and return 
    popl %ebp 
    ret



/** Pila del old_proceso (A) **/
#  ---------------
# |     ebp      |
# |    @inner    |  +4
# |   old_esp    |  +8
# |   new_esp    |  +12
# |     ...      |
#  ---------------

/** Pila de new proceso (B) **/
/** ?                                                          **/
/** Sabemos que el proceso B ejecutó task_switch, por lo que   **/
/** se quedo "congelado" con ebp e @inner en el top de la pila **/
#  ---------------
# |     ebp      |
# |    @inner    |
# |              |
# |   .......    |
# |              |
# ---------------- 

/** Segundo handler para task_switch, después de ejecutar inner_task_switch (en C)**/
ENTRY(switch_stack)
	ENTRY(switch_stack)
    # 1. Dynamic link
    push %ebp 
    movl %esp, %ebp

    # 2. Guardamos registros
    push %ebx
    push %edx

    movl %ebp, %ebx       
    # edx = dirección de memoria donde está 'current()->kernel_esp'
    movl 8(%ebp), %edx    
    # Guardamos nuestro ebp dentro de 'current()->kernel_esp'
    movl %ebx, (%edx)     
  

    # 3. Restauramos registros (en orden inverso al push)
    pop %edx
    pop %ebx

    # 4. Cambio de contexto, a partir de aquí -> RUN = Proceso B
    movl 12(%ebp), %esp
    
    # 5. Undo dynamic link del proceso B 
    popl %ebp
    ret