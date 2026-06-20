/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>
#include <errno.h>

char sys_buffer[256];

extern int zeos_ticks;


#define LECTURA 0
#define ESCRIPTURA 1

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}


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


int sys_gettime(void){
	return zeos_ticks;
}