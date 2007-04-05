//
// Written by Linus Torvalds, http://lkml.org/lkml/2002/12/30/154
//   Slight compilation modifications by Scott Bronson
//
//   cc -Wall -Werror -O3 syscall.c -m32 -o syscall
//


#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <asm/unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#define rdtsc() ({ unsigned long a, d; asm volatile("rdtsc":"=a" (a), "=d" (d)); a; })
// for testing _just_ system call overhead.
//#define __NR_syscall __NR_stat64
#define __NR_syscall __NR_getpid

#define NR (100000)
int main()
{
	int i, ret;
	unsigned long fast = ~0UL, slow = ~0UL, overhead = ~0UL;
	char *filename = "test";
	struct stat st;
	int j;
	for (i = 0; i < NR; i++) {
		unsigned long cycles = rdtsc();
		asm volatile("");
		cycles = rdtsc() - cycles;
		if (cycles < overhead)
			overhead = cycles;
	}
	printf("overhead: %6lu\n", overhead);

	for (j = 0; j < 10; j++)
	for (i = 0; i < NR; i++) {
		unsigned long cycles = rdtsc();
		asm volatile("call 0xffffe000"
			:"=a" (ret)
			:"0" (__NR_syscall),
			 "b" (filename),
			 "c" (&st));
		cycles = rdtsc() - cycles;
		if (cycles < fast)
			fast = cycles;
	}
	fast -= overhead;
	printf("sysenter: %6lu cycles\n", fast);

	for (i = 0; i < NR; i++) {
		unsigned long cycles = rdtsc();
		asm volatile("int $0x80"
			:"=a" (ret)
			:"0" (__NR_syscall),
			 "b" (filename),
			 "c" (&st));
		cycles = rdtsc() - cycles;
		if (cycles < slow)
			slow = cycles;
	}
	slow -= overhead;
	printf("int0x80:  %6lu cycles\n", slow);
	printf("          %6lu cycles difference\n", slow-fast);
	printf("factor %f\n", (double) slow / fast);

	return 0;
}
