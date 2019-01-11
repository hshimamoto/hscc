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
	TK_EQ,		// ==
	TK_NE,		// !=
	TK_EOF,
};

typedef struct {
	int type;
	int val;
} Token;

Vector *tokens;

void push_token(int type, int val)
{
	Token *t = malloc(sizeof(Token));

	t->type = type;
	t->val = val;

	vec_push(tokens, t);
}

int pos;

Token *get_token(void)
{
	return tokens->data[pos++];
}

void unget_token(void)
{
	pos--;
}

void tokenize(char *p)
{
	tokens = new_vector();

	while (*p) {
		if (isspace(*p)) {
			p++;
			continue;
		}
		if (*p == '=' && *(p + 1) == '=') {
			push_token(TK_EQ, 0);
			p += 2;
			continue;
		}
		if (*p == '!' && *(p + 1) == '=') {
			push_token(TK_NE, 0);
			p += 2;
			continue;
		}
		if (*p == '+' || *p == '-' ||
		    *p == '&' || *p == '^' || *p == '|' ||
		    *p == '*' || *p == '/' ||
		    *p == '(' || *p == ')' ||
		    *p == '=' || *p == ';') {
			push_token(*p, 0);
			p++;
			continue;
		}
		if (*p >= 'a' && *p <='z') {
			push_token(TK_IDENT, *p - 'a');
			p++;
			continue;
		}
		if (isdigit(*p)) {
			int val = strtol(p, &p, 0);

			push_token(TK_NUM, val);
			continue;
		}
		fprintf(stderr, "syntax error: %s\n", p);
		exit(1);
	}
	push_token(TK_EOF, 0);
}

enum {
	ND_NUM = 256,
	ND_IDENT,
	ND_EQ,
	ND_NE,
};

typedef struct node {
	int type;
	struct node *lhs, *rhs;
	int val;	// for ND_NUM;
} Node;

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
 * prog      : assign prog2
 * prog2     : e | prog
 * assign    : expr assign2 ";"
 * assing2   : e | "=" expr assign2
 * expr      : expr_xor | expr_xor "|" expr
 * expr_xor  : expr_and | expr_and "^" expr_xor
 * expr_and  : expr_cmp | expr_cmp "&" expr_and
 * expr_cmp  : expr_plus | expr_plus "==" expr_cmp | expr_plus "!=" expr_cmp
 * expr_plus : mul | mul "+" expr_plus | mul "-" expr_plus
 * mul       : term | term "*" mul | term "/" mul
 * term      : num | ident | "(" expr ")"
 */
Node *expr();

Node *num_or_ident()
{
	Token *t = get_token();

	if (t->type == TK_NUM) {
		int val = t->val;
		return new_node(ND_NUM, NULL, NULL, val);
	}
	if (t->type == TK_IDENT) {
		int val = t->val;
		return new_node(ND_IDENT, NULL, NULL, val);
	}
	// syntax error
	fprintf(stderr, "syntax error\n");
	exit(1);
}

Node *term()
{
	Token *t = get_token();

	if (t->type == '(') {
		Node *e = expr();

		t = get_token();
		if (t->type != ')') {
			fprintf(stderr, "syntax error\n");
			exit(1);
		}

		return e;
	}

	unget_token();

	return num_or_ident();
}

Node *mul()
{
	Node *lhs = term();
	Token *t = get_token();

	if (t->type == '*') {
		return new_node('*', lhs, mul(), 0);
	}
	if (t->type == '/') {
		return new_node('/', lhs, mul(), 0);
	}
	unget_token();
	return lhs;
}

Node *expr_plus()
{
	Node *lhs = mul();
	Token *t = get_token();

	if (t->type == '+') {
		return new_node('+', lhs, expr_plus(), 0);
	}
	if (t->type == '-') {
		return new_node('-', lhs, expr_plus(), 0);
	}
	unget_token();
	return lhs;
}

Node *expr_cmp()
{
	Node *lhs = expr_plus();
	Token *t = get_token();

	if (t->type == TK_EQ) {
		return new_node(ND_EQ, lhs, expr_cmp(), 0);
	}
	if (t->type == TK_NE) {
		return new_node(ND_NE, lhs, expr_cmp(), 0);
	}
	unget_token();
	return lhs;
}

Node *expr_and()
{
	Node *lhs = expr_cmp();
	Token *t = get_token();

	if (t->type == '&') {
		return new_node('&', lhs, expr_and(), 0);
	}
	unget_token();
	return lhs;
}

Node *expr_xor()
{
	Node *lhs = expr_and();
	Token *t = get_token();

	if (t->type == '^') {
		return new_node('^', lhs, expr_xor(), 0);
	}
	unget_token();
	return lhs;
}

Node *expr()
{
	Node *lhs = expr_xor();
	Token *t = get_token();

	if (t->type == '|') {
		return new_node('|', lhs, expr(), 0);
	}
	unget_token();
	return lhs;
}

Node *assign2()
{
	Token *t = get_token();

	if (t->type == ';') {
		return NULL;
	}

	if (t->type == '=') {
		Node *lhs = expr();
		Node *rhs = assign2();

		if (rhs == NULL)
			return lhs;

		return new_node('=', lhs, rhs, 0);
	}

	// bad
	fprintf(stderr, "no semicolumn (t->type %u)\n", t->type);
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
	Token *t = get_token();

	if (t->type == TK_EOF)
		return;

	unget_token();
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
	} else if (node->type == ND_EQ) {
		puts("  cmp rax, rdi");
		puts("  sete al");
		puts("  movzb rax, al");
	} else if (node->type == ND_NE) {
		puts("  cmp rax, rdi");
		puts("  setne al");
		puts("  movzb rax, al");
	} else if (node->type == '&') {
		puts("  and rax, rdi");
	} else if (node->type == '^') {
		puts("  xor rax, rdi");
	} else if (node->type == '|') {
		puts("  or rax, rdi");
	} else {
		fprintf(stderr, "error node->type = %d\n", node->type);
		exit(1);
	}
	puts("  push rax");
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: hscc <file>\n");
		return 1;
	}

	FILE *fp = fopen(argv[1], "r");

	if (!fp) {
		fprintf(stderr, "fopen error: %s\n", argv[1]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);

	char *p = malloc(sz + 1);

	fseek(fp, 0, SEEK_SET);
	fread(p, sz, 1, fp);
	p[sz] = 0;

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
