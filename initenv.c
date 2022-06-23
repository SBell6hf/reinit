#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/vt.h>
#include <sys/kd.h>

#pragma GCC diagnostic ignored "-Wattributes"

void safeSync() {
	sync();
	syscall(SYS_sync);
	usleep(300);
	sync();
	syscall(SYS_sync);
}

bool isTTY(int fd) {
	if (fd < 0) {
		return false;
	}
	char kt;
	if (ioctl(fd, KDGKBTYPE, &kt)) {
		return false;
	}
	return kt == KB_101 || kt == KB_84;
}

int chvt(int ttyNum) {
	int fd = -1;
	char ttyFilename[20];
	char *ttyDevices[] = {
		ttyFilename     ,
		"/dev/tty"      ,
		"/dev/tty0"     ,
		"/dev/tty1"     ,
		"/dev/tty2"     ,
		"/dev/vc/0"     ,
		"/dev/vc/1"     ,
		"/dev/vc/2"     ,
		"/dev/console"  ,
	};
	sprintf(ttyFilename, "/dev/tty%d%s", ttyNum, "\0");
	for (int i = 0; i < sizeof (ttyDevices) / sizeof (char *); i++) {
		if (isTTY(fd = open(ttyDevices[i], O_WRONLY))) {
			break;
		}
	}
	if (!isTTY(fd)) {
		return ENOTSUP;
	}
	if (ioctl(fd, VT_ACTIVATE, ttyNum)) {
		return errno;
	}
	if (ioctl(fd, VT_WAITACTIVE, ttyNum)) {
		return errno;
	}
	return 0;
}

__attribute__((optimize("O0")))
__attribute__ ((optnone))
_Noreturn void crash() {
	while (true) {
		syscall(SYS_kill, syscall(SYS_getpid), SIGKILL);
		syscall(SYS_kill, getpid(), SIGKILL);
		raise(SIGKILL);
		kill(getpid(), SIGKILL);
		__builtin_trap();
		for (long i = 0; i < 1000 * 1000 * 30; i++) {
			*((volatile int *) i) = rand();
			*((volatile int *) (rand() * i)) = rand();
		}
		syscall(SYS_exit, -1);
		exit(-1);
	}
}

void safeChvt(int vtNum) {
	safeSync();
	int pid, wstat;
	if (!(pid = fork())) {
		safeSync();
		chvt(vtNum);
		safeSync();
		crash();
		return;
	}
	safeSync();
	waitpid(pid, &wstat, 0);
	safeSync();
}

int main(int argc, char **argv, char **environ) {
	for (int i = 0; i < 100; i++) {
		signal(i, SIG_IGN);
	}

	FILE *optionsFile = fopen("/._tmp_reinit/options", "r");
	int nostop, nokill, keepfd, nochvt, vtNum;
	fscanf(optionsFile, "%d %d %d %d %d", &nostop, &nokill, &keepfd, &nochvt, &vtNum);
	fclose(optionsFile);

	if (!keepfd) {

		safeSync();
		
		setsid();
		for (int i = 0; i < FD_SETSIZE; i++) {
			ioctl(i, TIOCNOTTY);
			close(i);
		}

		int temp;
		for (int i = 0; i < 3; i++) {
		if (fcntl(i, F_GETFL) == -1 && errno == EBADF) {
			if ((temp = open("/dev/null", O_RDWR | O_NOCTTY)) >= 3) {
					close(temp);
				}	
			}
		}
		
		setsid();
	}

	if (!nochvt) {
		safeChvt(vtNum);
	}

	if (!nostop) {
		safeSync();
		for (int i = 0; i < 20; i++) {
			kill(-1, SIGSTOP);
			usleep(20);
		}
	}

	if (!nochvt) {
		safeChvt(vtNum);
	}

	if (!nokill) {
		safeSync();
		for (int i = 0; i < 20; i++) {
			kill(-1, SIGKILL);
			usleep(20);
		}
	}

	if (!nochvt) {
		safeChvt(vtNum);
	}

	safeSync();

	execve("/._tmp_reinit/prepare", argv, environ);
	execve("/sbin/init", argv, environ);
	execve("/usr/lib/systemd/systemd", argv, environ);

	int wstatus;
	while (1) {
		wait(&wstatus);
	}

	return 0;
}
