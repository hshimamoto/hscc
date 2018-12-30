#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

enum {
	TK_NUM = 256,
	TK_EOF,
};

typedef struct {
	int type;
	int val;
} Token;

Token tokens[100];

void tokenize(char *p)
{
	int i = 0; // index of tokens

	while (*p) {
		if (isspace(*p)) {
			p++;
			continue;
		}
		if (*p == '+' || *p == '-' ||
		    *p == '*' || *p == '/' ||
		    *p == '(' || *p == ')') {
			tokens[i].type = *p;
			i++;
			p++;
			continue;
		}
		if (isdigit(*p)) {
			tokens[i].type = TK_NUM;
			tokens[i].val = strtol(p, &p, 0);
			i++;
			continue;
		}
		fprintf(stderr, "syntax error: %s\n", p);
		exit(1);
	}
	tokens[i].type = TK_EOF;
}

enum {
	ND_NUM = 256,
};

typedef struct node {
	int type;
	struct node *lhs, *rhs;
	int val;	// for ND_NUM;
} Node;

int pos; // position in tokens

Node *new_node(int type, Node *lhs, Node *rhs, int val)
{
	Node *node = malloc(sizeof(Node));

	node->type = type;
	node->lhs = lhs;
	node->rhs = rhs;
	node->val = val;

	return node;
}

/*
 * expr: mul | mul "+" expr | mul "-" expr
 * mul : term | term "*" mul | term "/" mul
 * term: num | "(" expr ")"
 */
Node *expr();

Node *num()
{
	if (tokens[pos].type == TK_NUM) {
		int val = tokens[pos].val;
		pos++;
		return new_node(ND_NUM, NULL, NULL, val);
	}
	// syntax error
	fprintf(stderr, "syntax error\n");
	exit(1);
}

Node *term()
{
	if (tokens[pos].type == '(') {
		pos++;

		Node *e = expr();

		if (tokens[pos].type != ')') {
			fprintf(stderr, "syntax error\n");
			exit(1);
		}
		pos++;

		return e;
	}

	return num();
}

Node *mul()
{
	Node *lhs = term();

	if (tokens[pos].type == '*') {
		pos++;
		return new_node('*', lhs, mul(), 0);
	}
	if (tokens[pos].type == '/') {
		pos++;
		return new_node('/', lhs, mul(), 0);
	}
	return lhs;
}

Node *expr()
{
	Node *lhs = mul();

	if (tokens[pos].type == '+') {
		pos++;
		return new_node('+', lhs, expr(), 0);
	}
	if (tokens[pos].type == '-') {
		pos++;
		return new_node('-', lhs, expr(), 0);
	}
	return lhs;
}

void gen(Node *node)
{
	if (node->type == ND_NUM) {
		printf("  push %d\n", node->val);
		return;
	}

	gen(node->lhs);
	gen(node->rhs);
	puts("  pop rdi");
	puts("  pop rax");
	if (node->type == '+') {
		puts("  add rax, rdi");
	} else if (node->type == '-') {
		puts("  sub rax, rdi");
	} else if (node->type == '*') {
		puts("  mul rdi");
	} else if (node->type == '/') {
		puts("  xor edx, edx");
		puts("  div rdi");
	} else {
		fprintf(stderr, "error node->type = %d\n", node->type);
		exit(1);
	}
	puts("  push rax");
}

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

	tokenize(p);

	Node *node = expr();
	// generate stack machine
	gen(node);
	puts("  pop rax");
	puts("  ret");

	return 0;
}
