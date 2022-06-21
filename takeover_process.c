// #define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <elf.h>

// #pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

int getTextStackStart(pid_t pid, size_t *textStart, size_t *stackStart) {
	char mapsFilePath[1000];
	char *line = NULL;
	size_t len, buflen = 0;

	sprintf(mapsFilePath, "/proc/%d/maps", pid);
	FILE *mapsFile = fopen(mapsFilePath, "r");
	if (mapsFile == NULL) {
		return errno;
	}

	while ((len = getline(&line, &buflen, mapsFile)) != -1) {
		char addr[40];
		char perm[10];
		sscanf(line, "%s %s", addr, perm);
		if (perm[2] == 'x') {
			for (int i = 0; i < len; i++) {
				if (line[i] == '-') {
					line[i] = 0;
					break;
				}
			}
			sscanf(line, "%lx", textStart);
			break;
		}
	}

	while ((len = getline(&line, &buflen, mapsFile)) != -1) {
		if (!strcmp(line + (len - 9), " [stack]\n")) {
			for (int i = 0; i < len; i++) {
				if (line[i] == '-') {
					line[i] = 0;
					break;
				}
			}
			sscanf(line, "%lx", stackStart);
			break;
		}
	}
	free(line);
	return 0;
}

__attribute__((optimize("O0")))
__attribute__ ((optnone))
_Noreturn void crash() {
	while (true) {
		__builtin_trap();
		syscall(SYS_kill, syscall(SYS_getpid), SIGKILL);
		syscall(SYS_kill, getpid(), SIGKILL);
		raise(SIGKILL);
		kill(getpid(), SIGKILL);
		for (long i = 0; i < 1000 * 1000 * 30; i++) {
			*((volatile int *) i) = rand();
			*((volatile int *) (rand() * i)) = rand();
		}
		syscall(SYS_exit);
		exit(-1);
	}
}

__attribute__((no_stack_protector))
__attribute__((optimize("O0")))
__attribute__ ((optnone))
_Noreturn void garbageInstructions() {
	volatile long long a = 123;
	volatile long long b = 234 * a;
	volatile long long c = 345 * a / b;
	volatile long long d = 456 + a % b * c;
	volatile long long e = 567 * a + b + c % d;
	a = 123 + b ^ c & d + 456;
	b = a % c | (d + e) * b;
	b = d - e * c ^ b;
	c = a + c & d;
	d = c - d / (e * e * e);
	if (d == 0) {
		syscall(123, a, b, c, d, e);
	}
	__builtin_trap();
}

int ptraceGetReg(pid_t pid, struct user_regs_struct* regs) {
	struct iovec iov = {
		.iov_base = regs,
		.iov_len = sizeof (struct user_regs_struct),
	};
	return ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov);
}

int ptraceSetReg(pid_t pid, struct user_regs_struct* regs) {
	struct iovec iov = {
		.iov_base = regs,
		.iov_len = sizeof (struct user_regs_struct),
	};
	return ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov);
}

__attribute__((optimize("O0")))
__attribute__ ((optnone))
_Noreturn void jump(size_t target) {
	((void (*)(void))(target))();
	crash();
}

int getIPRegister_main(
	pid_t childPID,
	size_t textStart,
	unsigned long long lastvalue64[1000],
	int cont64[1000],
	int cont64max[1000],
	unsigned int lastvalue32[1000],
	int cont32[1000],
	int cont32max[1000]
) {
	int childStatus;

	if (ptrace(PTRACE_ATTACH, childPID, NULL, NULL)) {
		kill(childPID, SIGKILL);
		waitpid(childPID, &childStatus, 0);
		return errno;
	}
	ptrace(PTRACE_SETOPTIONS, childPID, NULL, PTRACE_O_EXITKILL);
	waitpid(childPID, &childStatus, 0);
	ptrace(PTRACE_SETOPTIONS, childPID, NULL, PTRACE_O_EXITKILL);
	
	for (int i = 0; i < 300; i++) {
		if (ptrace(PTRACE_POKEDATA, childPID, textStart + i, ((char *) garbageInstructions)[i])) {
			kill(childPID, SIGKILL);
			waitpid(childPID, &childStatus, 0);
			return errno;
		}
	}

	memset(lastvalue64, 0, 8000);
	memset(lastvalue32, 0, 4000);

	struct user_regs_struct regs;
	void *pregs = &regs;

	for (int s = 0; s < 5000; s++) {
		if (ptrace(PTRACE_SINGLESTEP, childPID, NULL, 0)) {
			kill(childPID, SIGKILL);
			waitpid(childPID, &childStatus, 0);
			return errno;
		}
		waitpid(childPID, &childStatus, 0);

		if (WIFEXITED(childStatus) || WIFSIGNALED(childStatus) || (WSTOPSIG(childStatus) != SIGSTOP && WSTOPSIG(childStatus) != SIGTRAP)) {
			break;
		}

		if (ptraceGetReg(childPID, &regs)) {
			kill(childPID, SIGKILL);
			waitpid(childPID, &childStatus, 0);
			return errno;
		}

		for (int alg = 0; alg <= 4; alg += 4) {
			for (int i = 0; alg + i * 8 + 8 <= sizeof (regs); i++) {
				if (
					llabs((long long) (*((unsigned long long *) (pregs + alg + i * 8)) - textStart)) < 300
				&&	llabs((long long) (lastvalue64[alg / 4 + i * 2] - textStart)) < 300
				&&	lastvalue64[alg / 4 + i * 2] < *((unsigned long long *) (pregs + alg + i * 8))
				) {

					cont64[alg / 4 + i * 2]++;
					if (cont64[alg / 4 + i * 2] > cont64max[alg / 4 + i * 2]) {
						cont64max[alg / 4 + i * 2] = cont64[alg / 4 + i * 2];
					}

				} else {
					// cont64[alg / 4 + i * 2] = 0;
				}

				lastvalue64[alg / 4 + i * 2] = *((unsigned long long *) (pregs + alg + i * 8));
			}
		}

		for (int i = 0; i * 4 + 4 <= sizeof (regs); i++) {
			if (
				abs((int) (*((unsigned int *) (pregs + i * 4)) - textStart)) < 300
			&&	abs((int) (lastvalue32[i] - textStart)) < 300
			&&	lastvalue32[i] < *((unsigned int *) (pregs + i * 4))
			) {

				cont32[i]++;
				if (cont32[i] > cont32max[i]) {
					cont32max[i] = cont32[i];
				}

			} else {
				// cont32[i] = 0;
			}

			lastvalue32[i] = *((unsigned int *) (pregs + i * 4));
		}
	}

	kill(childPID, SIGKILL);
	waitpid(childPID, &childStatus, 0);

	return 0;
}

int getIPRegister_final(int cont64max[1000], int cont32max[1000], bool *is64, int *offset, int *certaincy) {
	int contmax = 0;
	int maxoffset;
	bool maxtype;
	int contscnd = 0;
	
	for (int i = 0; i < 1000; i++) {
		if (cont64max[i] > contmax) {
			contscnd = contmax;
			contmax = cont64max[i];
			maxoffset = i * 4;
			maxtype = 1;
		}
	}
	
	for (int i = 0; i < 1000; i++) {
		if (cont32max[i] > contmax) {
			contscnd = contmax;
			contmax = cont32max[i];
			maxoffset = i * 4;
			maxtype = 0;
		}
	}

	if (contmax > 15 && contmax > contscnd * 2) {
		*is64 = maxtype;
		*offset = maxoffset;

		if (certaincy) {
			if (contmax > 150) {
				*certaincy = 100;
			} else {
				*certaincy = (int) ((float) contmax / 3.00 * 2.00);
			}
		}

		return 0;

	} else {
		errno = ENOTSUP;
		return ENOTSUP;
	}
}

__attribute__((optimize("O0")))
__attribute__ ((optnone))
int getIPRegister(bool *is64, int *offset, int *certaincy) {

	size_t textStart, stackStart;
	if (getTextStackStart(getpid(), &textStart, &stackStart)) {
		return errno;
	}

	unsigned long long lastvalue64[1000];
	int cont64[1000];
	int cont64max[1000];
	unsigned int lastvalue32[1000];
	int cont32[1000];
	int cont32max[1000];
	memset(cont64, 0, sizeof (cont64));
	memset(cont64max, 0, sizeof (cont64max));
	memset(cont32, 0, sizeof (cont32));
	memset(cont32max, 0, sizeof (cont32max));

	for (int startOffset = 0; startOffset < 20; startOffset++) {

		pid_t childPID = fork();
		if (!childPID) {
			syscall(SYS_ptrace, PTRACE_TRACEME, 0, NULL, NULL);
			jump(textStart + startOffset);
			return 0;
		}

		if (getIPRegister_main(childPID, textStart, lastvalue64, cont64, cont64max, lastvalue32, cont32, cont32max)) {
			kill(childPID, SIGKILL);
			return errno;
		}
	}

	return getIPRegister_final(cont64max, cont32max, is64, offset, certaincy);
}

__attribute__((no_stack_protector))
__attribute__((optimize("O0")))
__attribute__ ((optnone))
_Noreturn void injectedExec() {
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
	((int (*)(int, ...))(1))(SYS_execve, 0x6580128407840c47, 0x3ad0a4c068f89c94, 0x32c32f524e9ecb2b);
#elif UINTPTR_MAX == 0xFFFFFFFF
	((int (*)(int, ...))(1))(SYS_execve, 0x4308cd39, 0x10f8532f, 0x30012e71);
#else
	((int (*)(int, ...))(1))(
		SYS_execve,
		((((sizeof (long) - 4) / 4) * 0x4308cd39) ^ 0x4308cd39) + 0x6580128407840c47 * ((sizeof (long) - 4) / 4),
		((((sizeof (long) - 4) / 4) * 0x10f8532f) ^ 0x10f8532f) + 0x3ad0a4c068f89c94 * ((sizeof (long) - 4) / 4),
		((((sizeof (long) - 4) / 4) * 0x30012e71) ^ 0x30012e71) + 0x32c32f524e9ecb2b * ((sizeof (long) - 4) / 4)
	);
#endif

	__builtin_trap();
}

int ptraceJumpTo(pid_t pid, size_t target, bool IPIs64, long IPOffset) {
	struct user_regs_struct regs;
	void *pregs = &regs;

	if (ptraceGetReg(pid, &regs)) {
		return errno;
	}
	if (IPIs64) {
		*(unsigned long long *) (pregs + IPOffset) = target;
	} else {
		*(unsigned int *) (pregs + IPOffset) = target;
	}
	if (ptraceSetReg(pid, &regs)) {
		return errno;
	}

	return 0;
}

int ptraceGetIP(pid_t pid, size_t *out, bool IPIs64, long IPOffset) {
	struct user_regs_struct regs;
	void *pregs = &regs;

	if (ptraceGetReg(pid, &regs)) {
		return errno;
	}
	if (IPIs64) {
		*out = *(unsigned long long *) (pregs + IPOffset);
	} else {
		*out = *(unsigned int *) (pregs + IPOffset);
	}
	if (ptraceSetReg(pid, &regs)) {
		return errno;
	}

	return 0;
}

int ptraceWriteData(pid_t pid, size_t target, const char *data, size_t len) {
	for (int i = 0; i < len; i++) {
		if (ptrace(PTRACE_POKEDATA, pid, target + i, data[i])) {
			return errno;
		}
	}
	return 0;
}

size_t serializeExecVEArgs(
	size_t offset,
	const char *path, const char **argv, const char **envp,
	size_t *outlen, char *out,
	char **pathout, char ***argvout, char *** envpout
) {
	char* ptr = out;

	*pathout = (char *) offset;
	strcpy(out, path);
	ptr += strlen(path) + 1;

	int argc;
	for (argc = 0; argv[argc] != NULL; argc++);
	char **nargv = malloc((argc + 1) * sizeof (char *));
	nargv[argc] = NULL;

	for (int i = 0; argv[i] != NULL; i++) {
		strcpy(ptr, argv[i]);
		nargv[i] = (char *) (ptr - out + offset);
		ptr += strlen(argv[i]) + 1;
	}
	*argvout = (char **) (ptr - out + offset);
	memcpy(ptr, nargv, (argc + 1) * sizeof (char *));
	ptr += (argc + 1) * sizeof (char *);

	int envpc;
	for (envpc = 0; envp[envpc] != NULL; envpc++);
	char **nenvp = malloc((envpc + 1) * sizeof (char *));
	nenvp[envpc] = NULL;

	for (int i = 0; envp[i] != NULL; i++) {
		strcpy(ptr, envp[i]);
		nenvp[i] = (char *) (ptr - out + offset);
		ptr += strlen(envp[i]) + 1;
	}
	*envpout = (char **) (ptr - out + offset);
	memcpy(ptr, nenvp, (envpc + 1) * sizeof (char *));
	ptr += (envpc + 1) * sizeof (char *);

	*outlen = ptr - out;
	return *outlen;
}

int injectExec(pid_t pid, const char *path, const char **argv, const char **envp) {

	int wstatus;
	size_t textStart, stackStart;
	bool IPIs64;
	size_t IPReg;
	int IPOffset;

	if (getIPRegister(&IPIs64, &IPOffset, NULL)) {
		return errno;
	}

	if (ptrace(PTRACE_ATTACH, pid, NULL, NULL)) {
		return errno;
	}
	ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACEEXEC | PTRACE_O_TRACESYSGOOD);
	waitpid(pid, &wstatus, 0);
	ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACEEXEC | PTRACE_O_TRACESYSGOOD);

	if (getTextStackStart(pid, &textStart, &stackStart)) {
		return errno;
	}

	for (int startOffset = 0; startOffset <= 18; startOffset++) {
		ptraceWriteData(pid, textStart, (char *) injectedExec, 350);
		ptraceJumpTo(pid, textStart + startOffset, IPIs64, IPOffset);

		do {
			ptrace(PTRACE_CONT, pid, NULL, NULL);
			waitpid(pid, &wstatus, 0);
			if (WIFSIGNALED(wstatus) || WIFEXITED(wstatus)) {
				errno = ESRCH;
				return 1;
			}
			if (WIFSTOPPED(wstatus) && WSTOPSIG(wstatus) == SIGILL) {
				goto fail1;
			}
		} while (!WIFSTOPPED(wstatus) || (WSTOPSIG(wstatus) != SIGSEGV && WSTOPSIG(wstatus) != SIGBUS));

		if (ptraceGetIP(pid, &IPReg, IPIs64, IPOffset)) {
			return errno;
		}
		if ((long)(IPReg - textStart) > 35 || IPReg == 1) {
			break;
		}

		fail1: continue;
	}

	size_t execVEArgLen;
	char* execVEArgs = malloc(16384);
	char *rpath;
	char **rargv;
	char **renvp;
	serializeExecVEArgs(stackStart,  path, argv, envp,  &execVEArgLen, execVEArgs,  &rpath, &rargv, &renvp);

	struct user_regs_struct regs;
	void *pregs = &regs;

	for (int startOffset = 0; startOffset <= 10; startOffset++) {
		ptraceWriteData(pid, textStart, (char *) syscall, 350);
		ptraceJumpTo(pid, textStart, IPIs64, IPOffset);

		do {
			ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
			waitpid(pid, &wstatus, 0);
			if (WIFSIGNALED(wstatus) || WIFEXITED(wstatus)) {
				errno = ESRCH;
				return 1;
			}
			if (WIFSTOPPED(wstatus) && WSTOPSIG(wstatus) == SIGILL) {
				goto fail2;
			}
		} while (!WIFSTOPPED(wstatus) || !(WSTOPSIG(wstatus) & 0x80));

		if (ptraceGetIP(pid, &IPReg, IPIs64, IPOffset)) {
			return errno;
		}
		if ((long)(IPReg - textStart) < 18) {
			goto fail2;
		}
	
		if (ptraceGetReg(pid, &regs)) {
			return errno;
		}
		char argFound = 0b000000;
	
		if (sizeof (long) == 8) {
			for (int alg = 0; alg <= 4; alg += 4) {
				for (int i = 0; alg + i * 8 + 8 <= sizeof (regs); i++) {
					if (*(unsigned long long *) (pregs + alg + i * 8) == 0x6580128407840c47) {
						*(unsigned long long *) (pregs + alg + i * 8) = (unsigned long long) rpath;
						argFound |= 0b001;
					}
					if (*(unsigned long long *) (pregs + alg + i * 8) == 0x3ad0a4c068f89c94) {
						*(unsigned long long *) (pregs + alg + i * 8) = (unsigned long long) rargv;
						argFound |= 0b010;
					}
					if (*(unsigned long long *) (pregs + alg + i * 8) == 0x32c32f524e9ecb2b) {
						*(unsigned long long *) (pregs + alg + i * 8) = (unsigned long long) renvp;
						argFound |= 0b100;
					}
				}
			}
		} else {
			for (int alg = 0; alg <= 4; alg += 4) {
				for (int i = 0; alg + i * 8 + 8 <= sizeof (regs); i++) {
					if (*(unsigned int *) (pregs + alg + i * 8) == 0x4308cd39) {
						*(unsigned int *) (pregs + alg + i * 8) = (unsigned int) rpath;
						argFound |= 0b001000;
					}
					if (*(unsigned int *) (pregs + alg + i * 8) == 0x10f8532f) {
						*(unsigned int *) (pregs + alg + i * 8) = (unsigned int) rargv;
						argFound |= 0b010000;
					}
					if (*(unsigned int *) (pregs + alg + i * 8) == 0x30012e71) {
						*(unsigned int *) (pregs + alg + i * 8) = (unsigned int) renvp;
						argFound |= 0b100000;
					}
				}
			}
		}

		if (argFound == 0b000111 || argFound == 0b111000) {
			break;
		}

		fail2: continue;
	}

	ptraceSetReg(pid, &regs);
	ptraceWriteData(pid, stackStart, execVEArgs, execVEArgLen);
	free(execVEArgs);

	ptrace(PTRACE_DETACH, pid);
	return 0;
}

int main(int argc, char** argv, char** environ) {
	pid_t pid;
	sscanf(argv[1], "%d", &pid);
	return injectExec(pid, (const char *) argv[2], (const char **) argv + 3, (const char **) environ);
}
