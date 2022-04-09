#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

int main(int argc, char** argv) {
	for (int i = 3; i < FD_SETSIZE; i++) {
		close(i);
	}

	execv(argv[1], argv + 1);

	return errno;
}
