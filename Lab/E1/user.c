#include <libc.h>

int pid;

int __attribute__ ((__section__(".text.main")))
  main(void)
{   

  /* =================== */
  /*   TESTING WRITE     */
  /* =================== */
  //char buff[15] = "\nTesting write\n";
  /**  1. Miramos si escribe */
  //write(1,buff,strlen(buff));
  /** 2. Comprovamos el perror() */
  //if (write(-1,buff,strlen(buff)) < 0) perror(); // bad file descriptor
  //if (write(1,0,strlen(buff)) < 0) perror();  // bad address
  //if (write(1,buff,-1) < 0) perror();  // invalid argument
  /** 3. Probamos a escribir muchos carácteres = 279 carácteres*/
  //char *muchas_As = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
  //                  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
  //write(1,muchas_As,strlen(muchas_As));

  /* ====================== */
  /*    TESTING GETTIME     */
  /* ====================== */
  //char buff_time[32];
  //int t;
  //t = gettime();

  //itoa(t, buff_time);
  //write(1, "\n", 1);
  //write(1, "Clock Ticks: ", 13);

  //write(1, buff_time, strlen(buff_time));
  //write(1, "\n", 1); 
  

  /* ========================= */
  /*    TESTING PAGE FAULT     */
  /* ========================= */
  //char *p = 0;
  //*p = 'x';



    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */
  while(1) { }
}
