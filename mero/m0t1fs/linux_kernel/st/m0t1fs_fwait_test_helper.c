#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "m0t1fs/m0t1fs_ioctl.h"

#define PASS "pass"
#define FAIL "fail"

/**
 * Creates an object in mero, writes some data and tries to fwait on it.
 */
int test_fwait_write(int fd)
{
	int     rv;
	char   *str = "Hello World";

	/* write some data */
	rv = write(fd, str, strlen(str));

	/* Try and fwait on the file */
	rv = ioctl(fd, M0_M0T1FS_FWAIT);
	fprintf(stderr, "ioctl returned:%d\n", rv);

	return rv == 0;
}

/**
 * Generates an ioctl with an unknown command associated.
 */
int test_wrong_cmd(int fd)
{
	int rv;

	rv = ioctl(fd, M0_M0T1FS_FWAIT + 1);
	return rv < 0 && ENOTTY == errno;
}

/**
 * Generates an ioctl targeted to an invalid file descriptor.
 */
int test_wrong_fd(void)
{
	int rv;

	rv = ioctl(666, M0_M0T1FS_FWAIT);
	return rv < 0 && EBADF == errno;
}

int main(int argc, char **argv)
{
	int     rv;
	int     fd;
	char    object_name[PATH_MAX];

	/* check we were told the mount point */
	if (argc != 2){
		fprintf(stderr, "Usage: %s /path/to/mount/point\n", argv[0]);
		exit(1);
	}

	/* Build a path for our to-create object */
	rv = snprintf(object_name, sizeof(object_name), "%s/0:11234151",
		      argv[1]);
	if (rv >= sizeof(object_name)) {
		fprintf(stderr, "Path overflow\n");
		exit(1);
	}

	/* Creat the object*/
	fd = creat(object_name, 0600);
	if (fd >= 0) {
		do {
			/* Run the tests */
			rv = test_fwait_write(fd);
			fprintf(stderr, "test_fwait_write: %s\n",
				rv ? PASS:FAIL);
			if (!rv)
				break;

			rv = test_wrong_cmd(fd);
			fprintf(stderr, "test_wrong_cmd: %s\n",
				rv ? PASS:FAIL);
			if (!rv)
				break;

			rv = test_wrong_fd();
			fprintf(stderr, "test_wrong_fd: %s\n",
				rv ? PASS:FAIL);
			/* fall through */
		} while(0);
	} else {
		fprintf(stderr, "Failed to creat %s: %s\n",
				object_name, strerror(errno));
		exit(1);
	}

	if (rv)
		return 0;
	else
		exit(1);
}
