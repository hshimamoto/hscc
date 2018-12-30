#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

enum {
	TK_NUM = 256,
	TK_IDENT,
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
		    *p == '(' || *p == ')' ||
		    *p == '=' || *p == ';') {
			tokens[i].type = *p;
			i++;
			p++;
			continue;
		}
		if (*p >= 'a' && *p <='z') {
			tokens[i].type = TK_IDENT;
			tokens[i].val = *p - 'a';
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
	ND_IDENT,
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
 * prog   : assign prog2
 * prog2  : e | prog
 * assign : expr assign2 ";"
 * assing2: e | "=" expr assign2
 * expr   : mul | mul "+" expr | mul "-" expr
 * mul    : term | term "*" mul | term "/" mul
 * term   : num | ident | "(" expr ")"
 */
Node *expr();

Node *num_or_ident()
{
	if (tokens[pos].type == TK_NUM) {
		int val = tokens[pos].val;
		pos++;
		return new_node(ND_NUM, NULL, NULL, val);
	}
	if (tokens[pos].type == TK_IDENT) {
		int val = tokens[pos].val;
		pos++;
		return new_node(ND_IDENT, NULL, NULL, val);
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

	return num_or_ident();
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

Node *assign2()
{
	if (tokens[pos].type == ';') {
		pos++;
		return NULL;
	}

	if (tokens[pos].type == '=') {
		pos++;
		Node *lhs = expr();
		Node *rhs = assign2();

		if (rhs == NULL)
			return lhs;

		return new_node('=', lhs, rhs, 0);
	}

	// bad
	fprintf(stderr, "no semicolumn\n");
	exit(1);
}

Node *assign()
{
	Node *lhs = expr();
	Node *rhs = assign2();

	if (rhs == NULL)
		return lhs;

	return new_node('=', lhs, rhs, 0);
}

// code store
Node *code[100];
int cpos;

void prog()
{
	code[cpos] = assign();
	cpos++;
	if (tokens[pos].type == TK_EOF)
		return;
	// next statement
	return prog();
}

void gen_lval(Node *node)
{
	if (node->type == ND_IDENT) {
		// rbp - N * 8
		printf("  mov rax, rbp\n");
		printf("  sub rax, %d\n", (node->val + 1) * 8);
		printf("  push rax\n");
		return;
	}

	fprintf(stderr, "not lval\n");
	exit(1);
}

void gen(Node *node)
{
	if (node->type == ND_NUM) {
		printf("  push %d\n", node->val);
		return;
	}
	if (node->type == ND_IDENT) {
		gen_lval(node);
		puts("  pop rax");
		puts("  mov rax, [rax]");
		puts("  push rax");
		return;
	}
	if (node->type == '=') {
		gen_lval(node->lhs);
		gen(node->rhs);
		puts("  pop rdi");
		puts("  pop rax");
		puts("  mov [rax], rdi");
		puts("  push rdi");
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
	puts("  mov rbp, rsp");
	puts("  sub rsp, 240"); // 30 * 8

	tokenize(p);

	prog();
	// generate stack machine
	int i;
	for (i = 0; i < 100 && code[i]; i++) {
		printf("# code[%d]\n", i);
		gen(code[i]);
		puts("  pop rax");
	}
	puts("  mov rsp, rbp");
	puts("  ret");

	return 0;
}
