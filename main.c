#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: hscc <code>\n");
		return 1;
	}

	char *p = argv[1];

	puts(".intel_syntax noprefix");
	puts(".global main");
	puts("main:");

	char *endp;
	long n;

	n = strtol(p, &endp, 0);
	if (p == endp) {
		fprintf(stderr, "syntax error\n");
		return 1;
	}

	printf("  mov rax, %d\n", n);
	p = endp;
	while (*p) {
		if (*p == '+')
			printf("  add");
		else if (*p == '-')
			printf("  sub");
		else {
			fprintf(stderr, "syntax error\n");
			return 1;
		}
		p++;
		n = strtol(p, &endp, 0);
		if (p == endp) {
			fprintf(stderr, "syntax error\n");
			return 1;
		}
		printf(" rax, %d\n", n);
		p = endp;
	}
	puts("  ret");

	return 0;
}
