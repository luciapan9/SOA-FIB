#include <libc.h>

/* Syscalls */
int write(int fd, char *buffer, int size);
int read(char *buffer, int maxchars);
int fork(void);
int getpid(void);
int gettime(void);
void exit(void);
int gotoxy(int x, int y);
int set_color(int fg, int bg);
void *shmat(int id, void *addr);

/* Pantalla VGA modo texto. */
#define SCREEN_W 80
#define SCREEN_H 25

/*
 * Pagina compartida:
 *   id 0, mapeada de forma fija en 0x83C000.
 *   0x83C000 - 0x800000 = 0x3C000 => entrada UPT 60.
 * Queda fuera de datos/codigo y fuera de las entradas temporales del fork.
 */
#define SHM_ID 0
#define SHM_ADDR ((void *)0x83C000)
#define SHM_MAGIC 0x51ACE101

/* Layout del juego. */
#define GAME_TOP 3
#define GAME_BOTTOM 23
#define BORDER_LEFT 2
#define BORDER_RIGHT 77
#define PLAYER_Y 22
#define PLAYER_MIN_X (BORDER_LEFT + 1)
#define PLAYER_MAX_X (BORDER_RIGHT - 3)

#define ALIEN_ROWS 2
#define ALIEN_COLS 10
#define ALIEN_X_STEP 5
#define ALIEN_Y_STEP 2
#define ALIEN_W 3
#define ALIEN_START_X 14
#define ALIEN_START_Y 5

#define ENGINE_TICK_DELAY 1
#define PHYSICS_TICKS 2
#define ALIEN_MOVE_BASE 12

/* Colores VGA. */
#define COLOR_TEXT 7
#define COLOR_DIM 8
#define COLOR_TITLE 11
#define COLOR_PLAYER 10
#define COLOR_ALIEN 13
#define COLOR_BULLET 14
#define COLOR_ALERT 12
#define COLOR_BORDER 9

char frame_chars[SCREEN_H][SCREEN_W];
char frame_colors[SCREEN_H][SCREEN_W];
char prev_chars[SCREEN_H][SCREEN_W];
char prev_colors[SCREEN_H][SCREEN_W];
int render_started = 0;


typedef struct {
	int magic;
	int quit;
	int engine_ready;
	int control_ready;

	int frame;
	int game_over;
	int player_x;

	int bullet_active;
	int bullet_x;
	int bullet_y;

	int aliens_x;
	int aliens_y;
	int alien_dir;
	int alien_alive[ALIEN_ROWS][ALIEN_COLS];
	int aliens_left;

	int score;
	int high_score;
	int wave;
	int speed_level;

	int fire_count;
	int restart_count;
	int last_key;

	int engine_pid;
	int control_pid;
} InvadersShared;

/* Escribe una cadena por stdout usando la syscall write.
   Se usa para mensajes simples de error o de salida. */
void print_str(char *s)
{
	write(1, s, strlen(s));
}

/* Coloca el cursor en una posicion, cambia el color y escribe una cadena.*/
void put_str_at(int x, int y, int color, char *s)
{
	set_color(color, 0);
	gotoxy(x, y);
	write(1, s, strlen(s));
}

/* Escribe un caracter concreto en la pantalla real.
   Comprueba limites para no dibujar fuera de la VGA de 80x25. */
void put_char_at(int x, int y, int color, char c)
{
	char b[1];
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
	b[0] = c;
	set_color(color, 0);
	gotoxy(x, y);
	write(1, b, 1);
}

/* Convierte un entero a texto y lo escribe en pantalla.
   Los valores negativos se fuerzan a 0 para evitar formatos raros. */
void put_num_at(int x, int y, int color, int n)
{
	char b[16];
	if (n < 0) n = 0;
	itoa(n, b);
	put_str_at(x, y, color, b);
}

/* Borra una fila completa escribiendo 80 espacios.
   Se usa al limpiar la pantalla fuera del render incremental. */
void clear_row(int y)
{
	char spaces[SCREEN_W];
	int i;

	for (i = 0; i < SCREEN_W; i++) spaces[i] = ' ';
	set_color(COLOR_TEXT, 0);
	gotoxy(0, y);
	write(1, spaces, SCREEN_W);
}

/* Borra toda la pantalla fila a fila.
   Despues deja el cursor en la esquina superior izquierda. */
void clear_screen(void)
{
	int y;

	for (y = 0; y < SCREEN_H; y++) clear_row(y);
	gotoxy(0, 0);
}

/* Marca que no hay frame anterior valido.
   Obliga al siguiente render_flush a pintar toda la pantalla. */
void render_reset(void)
{
	render_started = 0;
}

/* Prepara un frame nuevo en memoria llenandolo de espacios.
   A partir de aqui las funciones draw_* escriben en el buffer, no en pantalla. */
void render_begin(void)
{
	int x;
	int y;

	for (y = 0; y < SCREEN_H; y++) {
		for (x = 0; x < SCREEN_W; x++) {
			frame_chars[y][x] = ' ';
			frame_colors[y][x] = COLOR_TEXT;
		}
	}
}

/* Escribe un caracter dentro del buffer del frame actual.*/
void render_char_at(int x, int y, int color, char c)
{
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
	frame_chars[y][x] = c;
	frame_colors[y][x] = color;
}

/* Escribe una cadena dentro del buffer del frame actual.
   Reutiliza render_char_at para respetar los limites de pantalla. */
void render_str_at(int x, int y, int color, char *s)
{
	int i;

	for (i = 0; s[i] != 0; i++) {
		render_char_at(x + i, y, color, s[i]);
	}
}

/* Convierte un numero y lo dibuja dentro del buffer del frame.
   Lo usa el HUD para score, wave, frame y pids. */
void render_num_at(int x, int y, int color, int n)
{
	char b[16];

	if (n < 0) n = 0;
	itoa(n, b);
	render_str_at(x, y, color, b);
}

/* Compara el frame nuevo con el frame anterior y solo escribe lo cambiado.
   Agrupa caracteres contiguos del mismo color para reducir syscalls y parpadeo. */
void render_flush(void)
{
	int x;
	int y;
	int start;
	int color;

	for (y = 0; y < SCREEN_H; y++) {
		x = 0;
		while (x < SCREEN_W) {
			if (render_started &&
			    frame_chars[y][x] == prev_chars[y][x] &&
			    frame_colors[y][x] == prev_colors[y][x]) {
				x++;
				continue;
			}

			start = x;
			color = frame_colors[y][x];
			while (x < SCREEN_W &&
			       (!render_started ||
			        frame_chars[y][x] != prev_chars[y][x] ||
			        frame_colors[y][x] != prev_colors[y][x]) &&
			       frame_colors[y][x] == color) {
				prev_chars[y][x] = frame_chars[y][x];
				prev_colors[y][x] = frame_colors[y][x];
				x++;
			}

			set_color(color, 0);
			gotoxy(start, y);
			write(1, &frame_chars[y][start], x - start);
		}
	}

	render_started = 1;
}

/* Espera activa durante n ticks del reloj de ZeOS.
   Sirve para marcar el ritmo del motor del juego. */
void wait_ticks(int n)
{
	int start;

	start = gettime();
	while (gettime() - start < n) {
	}
}

/* Espera hasta que la pagina compartida este inicializada.
   El magic evita que un proceso lea el struct a medio preparar. */
void wait_for_shared(volatile InvadersShared *shm)
{
	while (shm->magic != SHM_MAGIC) {
		wait_ticks(1);
	}
}

/* Limita la posicion horizontal de la nave.
   Asi nunca atraviesa los bordes del area de juego. */
int clamp_player_x(int x)
{
	if (x < PLAYER_MIN_X) return PLAYER_MIN_X;
	if (x > PLAYER_MAX_X) return PLAYER_MAX_X;
	return x;
}

/* Inicializa una oleada nueva de marcianos.
   Coloca todos vivos, reinicia la bala y ajusta la velocidad por oleada. */
void start_wave(volatile InvadersShared *shm, int wave)
{
	int r;
	int c;

	shm->wave = wave;
	shm->speed_level = wave - 1;
	if (shm->speed_level > 6) shm->speed_level = 6;

	shm->bullet_active = 0;
	shm->bullet_x = 0;
	shm->bullet_y = 0;
	shm->aliens_x = ALIEN_START_X;
	shm->aliens_y = ALIEN_START_Y;
	shm->alien_dir = 1;
	shm->aliens_left = ALIEN_ROWS * ALIEN_COLS;

	for (r = 0; r < ALIEN_ROWS; r++) {
		for (c = 0; c < ALIEN_COLS; c++) {
			shm->alien_alive[r][c] = 1;
		}
	}
}

/* Reinicia la partida actual desde cero.
   Mantiene fuera los datos globales como high_score y pids. */
void reset_game(volatile InvadersShared *shm)
{
	shm->frame = 0;
	shm->game_over = 0;
	shm->player_x = (SCREEN_W / 2) - 1;
	shm->score = 0;
	start_wave(shm, 1);
}

/* Inicializa toda la estructura compartida antes del fork.
   Escribe SHM_MAGIC al final para avisar de que ya se puede leer. */
void init_shared(volatile InvadersShared *shm)
{
	shm->magic = 0;
	shm->quit = 0;
	shm->engine_ready = 0;
	shm->control_ready = 0;
	shm->high_score = 0;
	shm->engine_pid = 0;
	shm->control_pid = 0;
	shm->fire_count = 0;
	shm->restart_count = 0;
	shm->last_key = 0;
	reset_game(shm);
	shm->magic = SHM_MAGIC;
}

int last_fps_ticks = 0;

/* Calcula los FPS aproximados con el contador de ticks del sistema.
   Se llama desde el motor cada vez que se dibuja un frame. */
int get_fps(void)
{
	int now;
	int elapsed;

	now = gettime();
	elapsed = now - last_fps_ticks;
	last_fps_ticks = now;
	if (elapsed <= 0) return 0;
	return 18 / elapsed;
}

/* Dibuja el marcador superior dentro del buffer de render.
   Muestra titulo, puntos, record, oleada, FPS y controles centrados. */
void draw_header(volatile InvadersShared *shm, int fps)
{
	render_str_at(31, 0, COLOR_TITLE, "SPACE INVADERS SHM");

	render_str_at(18, 1, COLOR_BULLET, "score:");
	render_num_at(25, 1, COLOR_BULLET, shm->score);
	render_str_at(35, 1, COLOR_BULLET, "best:");
	render_num_at(41, 1, COLOR_BULLET, shm->high_score);
	render_str_at(50, 1, COLOR_PLAYER, "wave:");
	render_num_at(56, 1, COLOR_PLAYER, shm->wave);
	render_str_at(63, 1, COLOR_PLAYER, "fps:");
	render_num_at(68, 1, COLOR_PLAYER, fps);

	render_str_at(13, 2, COLOR_DIM, "a/d mover | w/espacio disparar | r reset | q salir");
}

/* Dibuja el marco del area de juego.
   Usa lineas horizontales y verticales para delimitar la zona jugable. */
void draw_border(void)
{
	int x;
	int y;

	for (x = BORDER_LEFT; x <= BORDER_RIGHT; x++) {
		render_char_at(x, GAME_TOP, COLOR_BORDER, '-');
		render_char_at(x, GAME_BOTTOM, COLOR_BORDER, '-');
	}

	for (y = GAME_TOP + 1; y < GAME_BOTTOM; y++) {
		render_char_at(BORDER_LEFT, y, COLOR_BORDER, '|');
		render_char_at(BORDER_RIGHT, y, COLOR_BORDER, '|');
	}
}

/* Dibuja la nave del jugador en la fila fija PLAYER_Y.
   La posicion recibida se limita antes de pintar para no salir del borde. */
void draw_player(int x)
{
	x = clamp_player_x(x);
	render_char_at(x, PLAYER_Y, COLOR_PLAYER, '/');
	render_char_at(x + 1, PLAYER_Y, COLOR_PLAYER, 'A');
	render_char_at(x + 2, PLAYER_Y, COLOR_PLAYER, '\\');
}

/* Dibuja un marciano individual de tres caracteres.
   El grupo entero se compone repitiendo esta funcion en filas y columnas. */
void draw_alien(int x, int y)
{
	render_char_at(x, y, COLOR_ALIEN, '/');
	render_char_at(x + 1, y, COLOR_ALIEN, 'M');
	render_char_at(x + 2, y, COLOR_ALIEN, '\\');
}

/* Recorre la matriz de marcianos vivos y los pinta.
   Solo dibuja las posiciones cuyo alien_alive sigue valiendo 1. */
void draw_aliens(volatile InvadersShared *shm)
{
	int r;
	int c;

	for (r = 0; r < ALIEN_ROWS; r++) {
		for (c = 0; c < ALIEN_COLS; c++) {
			if (shm->alien_alive[r][c]) {
				draw_alien(shm->aliens_x + c * ALIEN_X_STEP,
				           shm->aliens_y + r * ALIEN_Y_STEP);
			}
		}
	}
}

/* Dibuja el mensaje de fin de partida.
   Se pinta encima del frame cuando los marcianos alcanzan la nave. */
void draw_game_over(void)
{
	render_str_at(33, 11, COLOR_ALERT, "GAME OVER");
	render_str_at(22, 13, COLOR_TEXT, "pulsa r para reiniciar o q para salir");
}

/* Construye un frame completo del juego y lo vuelca a pantalla.
   Primero llena el buffer y al final render_flush solo escribe diferencias. */
void draw_world(volatile InvadersShared *shm, int fps)
{
	render_begin();
	draw_header(shm, fps);
	draw_border();
	draw_aliens(shm);

	if (shm->bullet_active) {
		render_char_at(shm->bullet_x, shm->bullet_y, COLOR_BULLET, '|');
	}

	draw_player(shm->player_x);
	if (shm->game_over) draw_game_over();
	render_flush();
}

/* Calcula los limites del bloque de marcianos vivos.
   Devuelve 0 si no queda ninguno, o 1 si encontro al menos uno. */
int alive_alien_bounds(volatile InvadersShared *shm, int *left, int *right, int *bottom)
{
	int r;
	int c;
	int found;
	int ax;
	int ay;

	found = 0;
	*left = BORDER_RIGHT;
	*right = BORDER_LEFT;
	*bottom = GAME_TOP;

	for (r = 0; r < ALIEN_ROWS; r++) {
		for (c = 0; c < ALIEN_COLS; c++) {
			if (shm->alien_alive[r][c]) {
				ax = shm->aliens_x + c * ALIEN_X_STEP;
				ay = shm->aliens_y + r * ALIEN_Y_STEP;
				if (ax < *left) *left = ax;
				if (ax + ALIEN_W - 1 > *right) *right = ax + ALIEN_W - 1;
				if (ay > *bottom) *bottom = ay;
				found = 1;
			}
		}
	}

	return found;
}

/* Mueve el bloque de marcianos horizontalmente o lo baja una fila.
   Si toca un borde cambia la direccion y desciende. */
void move_aliens(volatile InvadersShared *shm)
{
	int left;
	int right;
	int bottom;
	int hit_edge;

	if (!alive_alien_bounds(shm, &left, &right, &bottom)) return;

	hit_edge = 0;
	if (shm->alien_dir > 0 && right + 1 >= BORDER_RIGHT) hit_edge = 1;
	if (shm->alien_dir < 0 && left - 1 <= BORDER_LEFT) hit_edge = 1;

	if (hit_edge) {
		shm->alien_dir = -shm->alien_dir;
		shm->aliens_y++;
	} else {
		shm->aliens_x += shm->alien_dir;
	}

	alive_alien_bounds(shm, &left, &right, &bottom);
	if (bottom >= PLAYER_Y) {
		shm->game_over = 1;
	}
}

/* Intenta crear una bala desde la nave.
   Solo dispara si no hay otra bala activa y la partida sigue viva. */
void try_fire(volatile InvadersShared *shm)
{
	if (shm->game_over) return;
	if (shm->bullet_active) return;

	shm->bullet_active = 1;
	shm->bullet_x = clamp_player_x(shm->player_x) + 1;
	shm->bullet_y = PLAYER_Y - 1;
}

/* Avanza la bala hacia arriba y comprueba impactos con marcianos.
   Si acierta, mata el marciano, suma puntos y desactiva la bala. */
void update_bullet(volatile InvadersShared *shm)
{
	int r;
	int c;
	int ax;
	int ay;

	if (!shm->bullet_active) return;

	shm->bullet_y--;
	if (shm->bullet_y <= GAME_TOP) {
		shm->bullet_active = 0;
		return;
	}

	for (r = 0; r < ALIEN_ROWS; r++) {
		for (c = 0; c < ALIEN_COLS; c++) {
			if (shm->alien_alive[r][c]) {
				ax = shm->aliens_x + c * ALIEN_X_STEP;
				ay = shm->aliens_y + r * ALIEN_Y_STEP;
				if (shm->bullet_y == ay &&
				    shm->bullet_x >= ax &&
				    shm->bullet_x < ax + ALIEN_W) {
					shm->alien_alive[r][c] = 0;
					shm->aliens_left--;
					shm->bullet_active = 0;
					shm->score += 10;
					if (shm->score > shm->high_score) {
						shm->high_score = shm->score;
					}
					return;
				}
			}
		}
	}
}

/* Actualiza la logica de una iteracion de fisica.
   Mueve bala, cambia de oleada y decide cuando toca mover marcianos. */
void advance_game(volatile InvadersShared *shm, int *alien_counter)
{
	int delay;

	if (shm->game_over) return;

	shm->player_x = clamp_player_x(shm->player_x);
	update_bullet(shm);

	if (shm->aliens_left <= 0) {
		shm->score += 50;
		if (shm->score > shm->high_score) shm->high_score = shm->score;
		start_wave(shm, shm->wave + 1);
		*alien_counter = 0;
		return;
	}

	delay = ALIEN_MOVE_BASE - shm->speed_level;
	if (delay < 2) delay = 2;

	(*alien_counter)++;
	if (*alien_counter >= delay) {
		*alien_counter = 0;
		move_aliens(shm);
	}
}

/* Proceso padre: motor del juego y renderizador.
   Lee eventos escritos por el hijo en la pagina compartida y dibuja frames. */
void run_engine_renderer(volatile InvadersShared *shm)
{
	int fire_seen;
	int restart_seen;
	int physics_counter;
	int alien_counter;
	int fps;

	wait_for_shared(shm);
	shm->engine_pid = getpid();
	shm->engine_ready = 1;

	fire_seen = shm->fire_count;
	restart_seen = shm->restart_count;
	physics_counter = 0;
	alien_counter = 0;
	fps = 0;
	last_fps_ticks = gettime();

	clear_screen();
	render_reset();

	while (!shm->quit) {
		wait_ticks(ENGINE_TICK_DELAY);

		if (restart_seen != shm->restart_count) {
			reset_game(shm);
			restart_seen = shm->restart_count;
			fire_seen = shm->fire_count;
			physics_counter = 0;
			alien_counter = 0;
		}

		if (fire_seen != shm->fire_count) {
			fire_seen = shm->fire_count;
			try_fire(shm);
		}

		physics_counter++;
		if (physics_counter >= PHYSICS_TICKS) {
			physics_counter = 0;
			advance_game(shm, &alien_counter);
			shm->frame++;
			fps = get_fps();
			draw_world(shm, fps);
		}
	}

	clear_screen();
	put_str_at(26, 11, COLOR_TITLE, "Space Invaders terminado");
	exit();
}

/* Proceso hijo: lector de teclado y controlador de la nave.
   Escribe player_x y contadores de eventos en la pagina compartida. */
void run_controller(volatile InvadersShared *shm)
{
	char c;
	int x;

	wait_for_shared(shm);
	shm->control_pid = getpid();
	shm->control_ready = 1;

	while (!shm->quit) {
		if (read(&c, 1) == 1) {
			shm->last_key = c;

			x = shm->player_x;
			if (c == 'a' || c == 'A') {
				x--;
				shm->player_x = clamp_player_x(x);
			} else if (c == 'd' || c == 'D') {
				x++;
				shm->player_x = clamp_player_x(x);
			} else if (c == 'w' || c == 'W' || c == ' ') {
				shm->fire_count++;
			} else if (c == 'r' || c == 'R') {
				shm->restart_count++;
			} else if (c == 'q' || c == 'Q') {
				shm->quit = 1;
			}
		}
	}

	exit();
}

/* Punto de entrada del programa de usuario.
   Mapea la pagina compartida, inicializa estado, hace fork y separa roles. */
int __attribute__((__section__(".text.main")))
main(void)
{
	volatile InvadersShared *shm;
	int child_pid;

	shm = (volatile InvadersShared *)shmat(SHM_ID, SHM_ADDR);
	if (shm == (void *)-1) {
		clear_screen();
		print_str("ERROR: no se pudo mapear la pagina compartida\n");
		exit();
	}

	init_shared(shm);

	child_pid = fork();
	if (child_pid < 0) {
		clear_screen();
		print_str("ERROR: fork del proceso de control\n");
		exit();
	}

	if (child_pid == 0) {
		shm = (volatile InvadersShared *)shmat(SHM_ID, SHM_ADDR);
		if (shm == (void *)-1) exit();
		run_controller(shm);
	}

	run_engine_renderer(shm);
	return 0;
}
