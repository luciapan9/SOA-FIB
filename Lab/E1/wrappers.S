#include <asm.h>


ENTRY(write)
    # 1. dynamic link
    pushl %ebp
    movl %esp,%ebp

    # 2. ponemos el id del sys_write en %eax
    movl $4,%eax

    # 3. Guardamos los registros necesarios 
    pushl %ebx 

    # 4. Paso de parámetros: int write(int fd,char* buffer,int size)
    movl 8(%ebp),%edx  # %edx <- fd  (1r parám.)
    movl 12(%ebp),%ecx # %ecx <- buffer (2o parám.)
    movl 16(%ebp),%ebx # %ebx <- size  (3r parám.)

    # 5. Fake dynamic link 
    /** una vez termina se termina el tratamiento de la **/
    /** syscall, con el sysexit el kernel no sabe como  **/
    /** volver a la @ de vuelta del wrapper. Para ello, **/
    /** creamos un fake dynamic link -> guardamos la    **/
    /** dirección de retorno al wrapper.                **/

    pushl $write_return # store de la @ de retorno al wrapper
    pushl %ebp # fake dynamic link
    movl %esp,%ebp 

    /** Pila actual **/
    # |   ebp   | 
    # |  @ret   | <- $write_return, return de la siguiente inst. del wrapper.
    # |   ebx   |                           
    # | old_ebp |
    # |  @ret   | <- return a la siguiente instrucción de usuario

    # 6. Fast system call
    sysenter 

write_return:
    # 7.1 Limpiamos pila 
    popl %ebp
    addl $4,%esp
    # 7.2 Restauramos registros
    popl %ebx

    # 8. Procesamos el resultado
    cmpl $0,%eax  # is ([%eax] < 0) ?
    jl write_error  # if yes then go to write_error
    jmp end   # else jump end 

write_error:
    negl %eax # en positivo el valor del errno
    movl %eax,errno # errno tiene el error code 
    movl $-1,%eax # al usuario le devolvemos -1

end:
    # 9. Return
    popl %ebp 
    ret 


ENTRY(gettime)
    # link dinamico
    push %ebp
    movl %esp, %ebp

    # ponemos el número de la syscall en eax
    movl $10, %eax

    # metemos la dir de retorno y creamos el fake dynamic link
    push $return_gettime
    pushl %ebp # fake dynamic link
    movl %esp,%ebp

    sysenter

return_gettime:
    # restauramos ebp y quitamos el @return_gettime de la pila
    popl %ebp
    addl $4, %esp

    # miramos si gettime nos ha devuelto error
    cmpl $0, %eax  
    jl gettime_error 

    # si no nos ha dado error vamos directamente al fin
    jmp fin 

gettime_error:
    negl %eax # en positivo el valor del errno
    movl %eax,errno # errno tiene el error code 
    movl $-1,%eax # al usuario le devolvemos -1

fin:
    # deshacemos dynamic link
    popl %ebp 
    ret 


