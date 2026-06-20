#include <libc.h>


void print_str(char *s) {
	write(1, s, strlen(s));
}

void print_num(char *texto, int n) {
	char buff[32];
	print_str(texto);
	itoa(n, buff);
	print_str(buff);
	print_str("\n");
}



int __attribute__ ((__section__(".text.main")))
  main(void)
{   
  int r, i;
	int mypid = getpid();

	print_num("PID inicial = ", mypid);

	r = fork();

	if (r < 0) {
		print_str("fork ERROR\n");
	}

	if (r == 0) {
		// hijo
		print_num("HIJO: mi PID = ", getpid());

		for (i = 0; i < 5; ++i) {
			print_num("HIJO iter = ", i);
		}

		print_str("HIJO: exit\n");
	
	}
	else {
		// padre
		print_num("PADRE: fork devuelve PID hijo = ", r);
		print_num("PADRE: mi PID = ", getpid());

		for (i = 0; i < 5; ++i) {
			print_num("PADRE iter = ", i);
		}

		print_str("PADRE: exit\n");
		exit();
	}

  while (1){

  };
}