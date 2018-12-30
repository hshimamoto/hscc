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
	if (*endp != '\0') {
		p = endp;
		if (*p != '+' && *p != '-') {
			fprintf(stderr, "syntax error\n");
			return 1;
		}
		n = strtol(p + 1, &endp, 0);
		if (p + 1 == endp) {
			fprintf(stderr, "syntax error\n");
			return 1;
		}
		if (*p == '+')
			printf("  add rax, %d\n", n);
		else
			printf("  sub rax, %d\n", n);
	}
	puts("  ret");

	return 0;
}
