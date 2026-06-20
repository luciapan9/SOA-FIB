#include <libc.h>

int write(int fd, char *buffer, int size);
int read(char *buffer, int maxchars);
int fork(void);
int getpid(void);
void exit(void);
int gotoxy(int x, int y);
int set_color(int fg, int bg);

void print_str(char *s) {
	write(1, s, strlen(s));
}

void print_num(char *texto, int n) {
	char buff[32];
	print_str(texto);
	if (n < 0) {
		print_str("-");
		n = -n;
	}
	itoa(n, buff);
	print_str(buff);
	print_str("\n");
}

void print_buffer(char *texto, char *buffer, int n) {
	print_str(texto);
	print_str("[");
	write(1, buffer, n);
	print_str("]\n");
}




int __attribute__ ((__section__(".text.main")))
  main(void)
{
	int pid;
	int n;
	char buffer[8];
	volatile int delay;

	print_str("\nTEST: dos hijos esperando read\n");

	pid = fork();
	if (pid < 0) {
		print_str("fork HIJO1 ERROR\n");
		perror();
		exit();
	}
	if (pid == 0) {
		print_str("HIJO1: esperando 6 chars\n");
		n = read(buffer, 6);
		print_num("HIJO1: read devuelve ", n);
		print_buffer("HIJO1: buffer = ", buffer, n);
		exit();
	}

	pid = fork();
	if (pid < 0) {
		print_str("fork HIJO2 ERROR\n");
		perror();
		exit();
	}

	if (pid == 0) {
		print_str("HIJO2: esperando 6 chars\n");
		n = read(buffer, 6);
		print_num("HIJO2: read devuelve ", n);
		print_buffer("HIJO2: buffer = ", buffer, n);
		exit();
	}

	for (delay = 0; delay < 20000000; ++delay) {}

	print_str("PADRE escribe: \n");
	exit();

  while (1){

  };
}
