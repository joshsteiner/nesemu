#define DEBUG 1

#include "nesemu.h"
#include "minunit.h"


int tests_run = 0;
FILE *log_file;


static char *test_cart_loads(void)
{
	FILE *mario = fopen("mario.nes", "r");
	mu_assert(mario != NULL, "couldn't open file");

	struct Cartridge cart;
	int result = from_iNES(mario, &cart);
	mu_assert(result == 0, "error reading file (invalid format)");

	load_cart(&cart);

	dump_mem("mario_init"); 

	fclose(mario);
	return 0;
}

static char *test_test_roms(void)
{
	FILE *test_rom = fopen("nestest.nes", "r");
	mu_assert(test_rom, "couldn't open file");

	struct Cartridge cart;
	int result = from_iNES(test_rom, &cart);
	mu_assert(result == 0, "invalid format");

	load_cart(&cart);
	run_nestest();

	fclose(test_rom);
	return 0;
}

static char *all_tests(void)
{
	// mu_run_test(test_cart_loads);
	mu_run_test(test_test_roms);
	return 0;
}


int main()
{
	log_file = stdout;

	/* run tests */
	char *result = all_tests();
	if (result != 0) {
		fprintf(stderr, "%s\n", result);
	} else {
		printf("all tests passed!\n");
	}
	printf("tests run: %d\n", tests_run);

	return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
