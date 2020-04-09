#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * If the following sizes are to be changed, please ensure to update the
 * numbers in m0t1fs_io_config_params.sh as well.
 */
unsigned long long NUM_ALPHABETS             = 26;
unsigned long long NUM_ALPHABET_REPEATITIONS = 4096;
unsigned long long NUM_ITERATIONS            = 200;

int main(int argc, char *argv[])
{
	FILE               *fd = NULL;
	char                ch;
	int                 i, j, k;
	unsigned long long  total_size;
	unsigned long long  bytes = 0;
	float               total_size_MB;

	if (argc != 2) {
		printf("Usage: %s output-file-name\n", argv[0]);
		exit(1);
	}
	fd = fopen(argv[1], "w");
	if (NULL == fd) {
		printf("fopen() Error !!!\n");
		return 1;
	}
	printf("File \"%s\" opened for writing\n", argv[1]);

	for (i = 0; i < NUM_ITERATIONS; ++i) {
		ch = 'a';
		for (j = 0; j < NUM_ALPHABETS; ++j, ++ch) {
			for (k = 0; k < NUM_ALPHABET_REPEATITIONS; ++k) {
				bytes = fwrite(&ch, sizeof(char), 1, fd);
				/*
				 * bytes is collected here just to keep the
				 * compiler happy, during the RPM build. It is
				 * intentionally ignored here and then checked
				 * only once below, again to keep the compiler
				 * happy!
				 */
			}
		}
	}
	assert(bytes == sizeof(char));
	total_size = NUM_ALPHABETS * NUM_ALPHABET_REPEATITIONS *
		     NUM_ITERATIONS;
	assert(ftell(fd) == total_size);
	fclose(fd);
	total_size_MB = (float)total_size / 1024 / 1024;
	printf("File \"%s\" closed after writing %llu bytes (%f MB)\n",
	       argv[1], total_size, total_size_MB);
	exit(0);
}
