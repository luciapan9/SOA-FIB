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

void clear_screen() {
	char spaces[80];
	int i, j;
	for (i = 0; i < 80; i++) spaces[i] = ' ';
	set_color(7, 0);
	for (j = 0; j < 25; j++) {
		gotoxy(0, j);
		write(1, spaces, 80);
	}
	gotoxy(0, 0);
}


// Los dos procesos escriben en diferentes posiciones de la pantalla en diferentes colores.
void check_milestone4_HIJO() {
	int i;
	char num[4];
	int colors[5] = {11, 10, 14, 13, 9};
	for (i = 0; i < 5; i++) {
		set_color(colors[i], 0);
		gotoxy(i * 6, 18 + i);        // baja por la derecha
		write(1, "H=", 2);
		itoa(i, num);
		write(1, num, strlen(num));
		for (int delay = 0; delay < 8000000; delay++) {}
	}
	set_color(7, 0);
}

void check_milestone4_PADRE() {
	int i;
	char num[4];
	int colors[5] = {12, 4, 6, 14, 7};
	for (i = 0; i < 5; i++) {
		set_color(colors[i], 0);
		gotoxy(72 - i * 6, 18 + i);   // baja por la izquierda
		write(1, "P=", 2);
		itoa(i, num);
		write(1, num, strlen(num));
		for (int delay = 0; delay < 8000000; delay++) {}
	}
	set_color(7, 0);
}

/* ------------------------------------------------------------------ */
int __attribute__ ((__section__(".text.main")))
  main(void)
{
	int pid;

	clear_screen();
	set_color(13,0);
	print_str("=== MILESTONE 4 TESTS ===\n");
	
	// prueba del errno para gotoxy
	if (gotoxy(-1, 0) < 0) perror();       
	if (gotoxy(80, 0) < 0) perror();         
	if (gotoxy(0, 25) < 0) perror();  
	print_str("errno del gotoxy OK\n\n");    
	
	// prueba del errno para set_color
	if (set_color(-1, 0) < 0) perror();
	if (set_color(16, 0) < 0) perror();
	if (set_color(0, -1) < 0) perror();
	if (set_color(0,8) < 0) perror();
	print_str("errno del set color OK\n");


	pid = fork();
	if (pid < 0) {
		print_str("fork ERROR\n");
		exit();
	}
	if (pid == 0) {
		check_milestone4_HIJO();
		exit();
	}
	else {
		check_milestone4_PADRE();
		exit();
	}

	while (1) {};
}
