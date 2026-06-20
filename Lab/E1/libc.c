/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

#include "errno.h"

int errno;

void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}



void perror() {
  char *output;
  
  switch (errno) {
    case EBADF:
      output = "bad file descriptor\n";
      break;
    case EFAULT:
      output = "bad address\n";
      break;
    case EINVAL:
      output = "invalid argument\n";
      break;
    case ENOSYS:
      output = "function not implemented\n";
      break;

    default:
      output = "type of error not implemented\n";
      break;
  }

  write(1,output,strlen(output));
}

