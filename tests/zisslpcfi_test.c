// SPDX-FileCopyrightText: 2023 Rivos Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>


int curr_pid = 0;

#define PROT_SHADOWSTACK 0x40
#define CSR_SSP 0x020

#ifdef __ASSEMBLY__
#define __ASM_STR(x)	x
#else
#define __ASM_STR(x)	#x
#endif

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("csrr %0, " __ASM_STR(csr)	\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrw " __ASM_STR(csr) ", %0"	\
			      : : "rK" (__v)			\
			      : "memory");			\
})

int foo()
{
	printf("pid %d, Inside foo\n", curr_pid);
	return 0;
}


int bar()
{
	printf("pid %d, Inside bar\n", curr_pid);
	return 0;
}

int indirect_foo(int i, char c)
{
	if (i == 0)
		printf("pid %d, Integer arg is 0\n", curr_pid);

	if (c == 'a')
		printf("pid %d, char arg is a\n", curr_pid);

	return 0;
}

int (*func_ptr)(int i, char c);

int fork_task = 0;
int mmap_shdw_stk = 0;
int wait_on_keybd_input = 0;
int signal_test = 0;

int get_options(int argc, char *argv[])
{
	int c;
	while ((c = getopt (argc, argv, "fmsw")) != -1) {
		switch (c) {
		case 'f':
        		fork_task = 1;
        		break;
      		case 'm':
			mmap_shdw_stk = 1;
			break;
		case 's':
			signal_test = 1;
		case 'w':
			wait_on_keybd_input = 1;
			break;
		case '?':
			fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
			return 1;
		default:
			abort ();
		}	
	}
	return 0;
}

void signal_call()
{
	unsigned long ssp_csr = 0;

	printf("\n inside signal call");

	ssp_csr = csr_read(CSR_SSP);
	printf("Current SSP CSR value is %lx\n", ssp_csr);
}

void  __attribute__((aligned(16))) sig_usr_handler(int signo)
{
	unsigned long ssp_csr = 0;

	if (signo == SIGINT)
		printf("\nrecieved SIGUSR1");

	ssp_csr = csr_read(CSR_SSP);
	printf("Current SSP CSR value is %lx\n", ssp_csr);
	signal_call();
}

int shadow_stack_signal_test()
{
	if (signal(SIGINT, sig_usr_handler) == SIG_ERR) {
		printf("\n registering SIGUSR1 failed");
		return -1;
	}

	printf("\n Signal registered for SIGINT");

	return 0;
}

int main(int argc, char *argv[])
{
	int *shadow_stack = NULL;
	int *read_write = NULL;
	int pid = 0;
	unsigned long ssp_csr = 0;
	func_ptr = indirect_foo;

	curr_pid = getpid();

	ssp_csr = csr_read(CSR_SSP);
	printf("Current SSP CSR value is %lx\n", ssp_csr);

	if (get_options(argc, argv)) {
		printf("parsing command line arguments failed\n");
		return -1;
	}

	if (fork_task) {
		pid = fork();
		if (pid == 0) {
			printf("Inside child\n");
			ssp_csr = csr_read(CSR_SSP);
			printf("Current SSP CSR value is %lx\n", ssp_csr);

		} else {
			printf("Inside parent\n");
		}
		curr_pid = getpid();
	}

	if (mmap_shdw_stk) {
		printf("Allocating shadowstack and regular read-write memory\n");
		shadow_stack = (int *)mmap(NULL, 4096, PROT_SHADOWSTACK, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
		read_write = (int *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		printf("Shadow stack allocated at %p, read_write mem allocated at %p \n", shadow_stack, read_write);
		printf("Going to read shadow stack memory\n");
		printf("Performing read on shadow stack %d\n", *shadow_stack);
	}

	if (wait_on_keybd_input) {
		printf("Press any key to exit the test\n");
		getchar();
	}

	if (signal_test) {
		shadow_stack_signal_test();

		while (1)
			sleep(1);
	}

	foo();
	bar();
	func_ptr(0,'a');

	//while (1);
	return 0;
}
