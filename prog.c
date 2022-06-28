/*
 * A program to test "stored-strings" GCC plugin.
 *
 * Build:
 *   gcc -Wall -O2 -fplugin="${PWD}/stored-strings-plugin.so" -o prog prog.c
 *
 * Example output:
 *      prog.c: In function ‘main’:
 *      prog.c:51:19: note: got an assignment of a const char * to a global variable.
 *      51 |         p->my.str = "Abracadabra";
 *      |         ~~~~~~~~~~^~~~~~~~~~~~~~~
 *      prog.c:54:27: note: got an assignment of a const char * to a global variable.
 *      54 |                 p->my.str = "";
 *      |                 ~~~~~~~~~~^~~~
 *      prog.c:56:27: note: got an assignment of a const char * to a global variable.
 *      56 |                 other_str = "Nope";
 *      |                 ~~~~~~~~~~^~~~~~~~
 *      prog.c:58:14: note: got an assignment of a const char * to a global variable.
 *      58 |         *var = "Brokodobo";
 *      |         ~~~~~^~~~~~~~~~~~~
 *      prog.c:61:19: note: got an assignment of a const char * to a global variable.
 *      61 |         p->my.str = "Hrumhrum";
 *      |         ~~~~~~~~~~^~~~~~~~~~~~ 
 */

#include <stdio.h>

#define __section(section)	__attribute__((__section__(section)))
#define __init			__section(".init.text")
#define   noinline		__attribute__((__noinline__))

const char *other_str;

volatile int cond = 1;

struct mystruct {
	const char *str;
	int other;
};

struct outer {
	struct mystruct my;
	int val;
} data;

struct outer more_data[8];

void __init noinline somefunc(void)
{
	other_str = "FooBarBaz";
}

int main(int argc, char *argv[])
{
	struct outer *p = &data;
	const char **var = &other_str;
	const char *local;
	const char *arr[2];
	
	p->my.str = "Abracadabra";
	local = "Locloc";
	if (!cond) {
		p->my.str = "";
		local = "Laclac";
		other_str = "Nope";
	}
	*var = "Brokodobo";
	arr[1] = "Frobfrob";
	p = &more_data[3];
	p->my.str = "Hrumhrum";

	printf("strings: %s %s %s %s\n", data.my.str, other_str, arr[1], local);
	return 0;
}