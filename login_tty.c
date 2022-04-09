#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utmp.h>

int main(int argc, char** argv) {
	if (getpid() != 1) {
		int pid;

		if ((pid = fork()) != 0) {
			int wstat = 0;

			do {
				waitpid(pid, &wstat, 0);
			} while (!WIFEXITED(wstat) && !WIFSIGNALED(wstat));

			if (WIFSIGNALED(wstat)) {
				raise(WTERMSIG(wstat));
				raise(SIGKILL);
			}

			return WEXITSTATUS(wstat);
		}
	}

	int fd;

	setsid();
	fd = open(argv[1], O_RDWR | O_NOCTTY);

	if (ioctl(fd, TIOCSCTTY, 1) == -1 && getpid() != 1) {
		return errno;
	}

	int temp;
	for (int i = 0; i < 3; i++) {
		if (fcntl(i, F_GETFL) == -1 && errno == EBADF) {

			if ((temp = open("/dev/null", O_RDWR | O_NOCTTY)) >= 3) {
				close(temp);
			}
		}
	}
	while (dup2(fd, 0) == -1 && errno == EBUSY);
	while (dup2(fd, 1) == -1 && errno == EBUSY);
	while (dup2(fd, 2) == -1 && errno == EBUSY);

	close(fd);

	execv(argv[2], argv + 2);

	return errno;
}
