#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <memory.h>

typedef struct {
	void **data;
	int capa, len;
} Vector;

int BUFSZ = 32;

Vector *new_vector()
{
	Vector *v = malloc(sizeof(Vector));
	v->data = malloc(sizeof(void *) * BUFSZ);
	v->capa = BUFSZ;
	v->len = 0;
	return v;
}

void vec_push(Vector *v, void *e)
{
	if (v->capa >= v->len) {
		v->capa += BUFSZ;
		v->data = realloc(v->data, sizeof(void *) * v->capa);
	}
	v->data[v->len++] = e;
}

enum {
	TK_NUM = 256,
	TK_IDENT,
	TK_EOF,
};

typedef struct {
	int type;
	int val;
} Token;

Vector *tokens;

void tokenize(char *p)
{
	tokens = new_vector();

	while (*p) {
		if (isspace(*p)) {
			p++;
			continue;
		}
		if (*p == '+' || *p == '-' ||
		    *p == '*' || *p == '/' ||
		    *p == '(' || *p == ')' ||
		    *p == '=' || *p == ';') {
			Token *t = malloc(sizeof(Token));
			t->type = *p;
			vec_push(tokens, t);
			p++;
			continue;
		}
		if (*p >= 'a' && *p <='z') {
			Token *t = malloc(sizeof(Token));
			t->type = TK_IDENT;
			t->val = *p - 'a';
			vec_push(tokens, t);
			p++;
			continue;
		}
		if (isdigit(*p)) {
			Token *t = malloc(sizeof(Token));
			t->type = TK_NUM;
			t->val = strtol(p, &p, 0);
			vec_push(tokens, t);
			continue;
		}
		fprintf(stderr, "syntax error: %s\n", p);
		exit(1);
	}
	Token *t = malloc(sizeof(Token));
	t->type = TK_EOF;
	vec_push(tokens, t);
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
	Token *t = tokens->data[pos];

	if (t->type == TK_NUM) {
		int val = t->val;
		pos++;
		return new_node(ND_NUM, NULL, NULL, val);
	}
	if (t->type == TK_IDENT) {
		int val = t->val;
		pos++;
		return new_node(ND_IDENT, NULL, NULL, val);
	}
	// syntax error
	fprintf(stderr, "syntax error\n");
	exit(1);
}

Node *term()
{
	Token *t = tokens->data[pos];

	if (t->type == '(') {
		pos++;

		Node *e = expr();

		t = tokens->data[pos];
		if (t->type != ')') {
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
	Token *t = tokens->data[pos];

	if (t->type == '*') {
		pos++;
		return new_node('*', lhs, mul(), 0);
	}
	if (t->type == '/') {
		pos++;
		return new_node('/', lhs, mul(), 0);
	}
	return lhs;
}

Node *expr()
{
	Node *lhs = mul();
	Token *t = tokens->data[pos];

	if (t->type == '+') {
		pos++;
		return new_node('+', lhs, expr(), 0);
	}
	if (t->type == '-') {
		pos++;
		return new_node('-', lhs, expr(), 0);
	}
	return lhs;
}

Node *assign2()
{
	Token *t = tokens->data[pos];

	if (t->type == ';') {
		pos++;
		return NULL;
	}

	if (t->type == '=') {
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
Vector *code;

void prog()
{
	vec_push(code, assign());
	Token *t = tokens->data[pos];

	if (t->type == TK_EOF)
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
	puts("  push rbp");
	puts("  mov rbp, rsp");
	puts("  sub rsp, 240"); // 30 * 8

	tokenize(p);

	code = new_vector();
	prog();
	// generate stack machine
	int i;
	for (i = 0; i < code->len; i++) {
		printf("# code[%d]\n", i);
		gen(code->data[i]);
		puts("  pop rax");
	}
	puts("  mov rsp, rbp");
	puts("  pop rbp");
	puts("  ret");

	return 0;
}
