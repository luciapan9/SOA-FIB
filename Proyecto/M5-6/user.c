#include <libc.h>
#include <errno.h>

int write(int fd, char *buffer, int size);
int fork(void);
void exit(void);
void *shmat(int id, void *addr);
int shmdt(void *addr);
int shmrm(int id);

extern int errno;

#define PAGE_SIZE       0x1000
#define USER_START      0x800000
#define SHM_FIRST_ADDR  (USER_START + (28 * PAGE_SIZE))
#define USER_END        (USER_START + (1024 * PAGE_SIZE))

#define SHM0 ((void *)SHM_FIRST_ADDR)
#define SHM1 ((void *)(SHM_FIRST_ADDR + PAGE_SIZE))
#define BAD_UNALIGNED ((void *)(SHM_FIRST_ADDR + 1))
#define BAD_DATA ((void *)USER_START)
#define BAD_HIGH ((void *)USER_END)
#define ERR_PTR ((void *)-1)

static int total = 0;
static int failed = 0;
static int group_failed = 0;
static char *group_first_fail = 0;

void print(char *s)
{
	write(1, s, strlen(s));
}

void print_num(int n)
{
	char buf[16];
	itoa(n, buf);
	print(buf);
}

void clear_screen(void)
{
	char spaces[80];
	int x, y;

	for (x = 0; x < 80; x++) spaces[x] = ' ';

	set_color(7, 0);
	for (y = 0; y < 25; y++) {
		gotoxy(0, y);
		write(1, spaces, 80);
	}
	gotoxy(0, 0);
}

void begin_group(void)
{
	group_failed = failed;
	group_first_fail = 0;
}

void check(char *name, int ok)
{
	total++;
	if (!ok) {
		failed++;
		if (group_first_fail == 0) group_first_fail = name;
	}
}

void check_ok(char *name, int value)
{
	check(name, value == 0);
}

void check_error(char *name, int value, int error)
{
	check(name, value == -1 && errno == error);
}

void check_ptr_error(char *name, void *value, int error)
{
	check(name, value == ERR_PTR && errno == error);
}

void end_group(char *name)
{
	print(name);
	print(": ");

	if (failed == group_failed) {
		set_color(2, 0);
		print("OK\n");
	}
	else {
		set_color(4, 0);
		print("FALLO");
		if (group_first_fail != 0) {
			print(" (");
			print(group_first_fail);
			print(")");
		}
		print("\n");
	}

	set_color(7, 0);
}

int is_error_ptr(void *ptr)
{
	return ptr == ERR_PTR;
}

void test_errores_basicos(void)
{
	begin_group();

	errno = 0; check_ptr_error("shmat id negativo", shmat(-1, 0), EINVAL);
	errno = 0; check_ptr_error("shmat id grande", shmat(10, 0), EINVAL);
	errno = 0; check_ptr_error("shmat sin alinear", shmat(0, BAD_UNALIGNED), EINVAL);
	errno = 0; check_ptr_error("shmat en datos", shmat(0, BAD_DATA), EINVAL);
	errno = 0; check_ptr_error("shmat fuera", shmat(0, BAD_HIGH), EINVAL);

	errno = 0; check_error("shmdt sin alinear", shmdt(BAD_UNALIGNED), EINVAL);
	errno = 0; check_error("shmdt en datos", shmdt(BAD_DATA), EINVAL);
	errno = 0; check_error("shmdt fuera", shmdt(BAD_HIGH), EINVAL);
	errno = 0; check_error("shmdt no mapeada", shmdt(SHM0), EINVAL);

	errno = 0; check_error("shmrm id negativo", shmrm(-1), EINVAL);
	errno = 0; check_error("shmrm id grande", shmrm(10), EINVAL);
	errno = 0; check_error("shmrm vacia", shmrm(9), EINVAL);

	end_group("1 errores de entrada");
}

void test_mapear_y_soltar(void)
{
	int *a;
	int *b;
	void *busy;

	begin_group();

	a = (int *)shmat(0, 0);
	check("shmat auto id0", !is_error_ptr(a));
	if (is_error_ptr(a)) {
		end_group("2 attach/detach normal");
		return;
	}

	a[0] = 1234;
	a[1] = 0;

	b = (int *)shmat(0, SHM1);
	check("shmat fijo id0", b == (int *)SHM1);
	if (!is_error_ptr(b)) {
		check("misma pagina lee", b[0] == 1234);
		b[1] = 5678;
		check("misma pagina escribe", a[1] == 5678);
	}

	errno = 0;
	busy = shmat(3, SHM1);
	check_ptr_error("shmat encima", busy, EINVAL);
	if (!is_error_ptr(busy)) {
		shmdt(busy);
		shmrm(3);
	}

	if (!is_error_ptr(b)) {
		check_ok("shmdt fijo", shmdt(b));
		errno = 0; check_error("shmdt repetido", shmdt(b), EINVAL);
	}

	check_ok("shmdt auto", shmdt(a));
	check_ok("shmrm sin mapas", shmrm(0));
	errno = 0; check_error("shmrm repetido", shmrm(0), EINVAL);

	end_group("2 attach/detach normal");
}

void test_borrado_diferido(void)
{
	int *p;
	int *again;

	begin_group();

	p = (int *)shmat(1, 0);
	check("shmat id1", !is_error_ptr(p));
	if (is_error_ptr(p)) {
		end_group("3 shmrm diferido");
		return;
	}

	p[0] = 700;
	check_ok("shmrm activa", shmrm(1));

	p[0]++;
	check("pagina viva tras shmrm", p[0] == 701);

	check_ok("shmdt ultimo", shmdt(p));
	errno = 0; check_error("id1 ya borrada", shmrm(1), EINVAL);

	again = (int *)shmat(1, 0);
	check("shmat id1 otra vez", !is_error_ptr(again));
	if (!is_error_ptr(again)) {
		again[0] = 900;
		check("id1 reutilizable", again[0] == 900);
		check_ok("shmdt id1 nuevo", shmdt(again));
		check_ok("shmrm id1 nuevo", shmrm(1));
	}

	end_group("3 shmrm diferido");
}

void test_fork_comparte(void)
{
	int *p;
	int pid;
	int guard;

	begin_group();

	p = (int *)shmat(2, 0);
	check("shmat id2", !is_error_ptr(p));
	if (is_error_ptr(p)) {
		end_group("4 fork con shm");
		return;
	}

	p[0] = 10;
	p[1] = 0;
	p[2] = 0;

	pid = fork();
	if (pid == 0) {
		if (p[0] == 10) {
			p[0] = 44;
			p[2] = 1;
		}
		else {
			p[2] = -1;
		}
		p[1] = 1;
		shmdt(p);
		exit();
	}

	check("fork padre", pid > 0);

	guard = 0;
	while (p[1] == 0 && guard < 60000000) guard++;

	check("hijo responde", p[1] == 1);
	check("hijo leyo padre", p[2] == 1);
	check("padre ve hijo", p[0] == 44);
	check_ok("shmdt padre id2", shmdt(p));
	check_ok("shmrm id2", shmrm(2));

	end_group("4 fork con shm");
}

int __attribute__ ((__section__(".text.main")))
main(void)
{
	clear_screen();

	set_color(11, 0);
	print("Pruebas:\n\n");
	set_color(7, 0);

	test_errores_basicos();
	test_mapear_y_soltar();
	test_borrado_diferido();
	test_fork_comparte();

	print("\nTotal: ");
	set_color(failed == 0 ? 2 : 4, 0);
	print_num(total - failed);
	print("/");
	print_num(total);
	if (failed == 0) print(" OK\n");
	else print(" con fallos\n");

	set_color(7, 0);
	exit();

	while (1) {}
}
