#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <memory.h>

typedef struct {
	void **data;
	int capa, len;
} Vector;

int BUFSZ = 32;

Vector *new_vector(void)
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

typedef struct {
	Vector *keys;
	Vector *vals;
} Map;

Map *new_map(void)
{
	Map *m = malloc(sizeof(Map));

	m->keys = new_vector();
	m->vals = new_vector();

	return m;
}

void map_set(Map *m, char *key, void *val)
{
	for (int i = 0; i < m->keys->len; i++) {
		if (strcmp(m->keys->data[i], key))
			continue;
		m->vals[i].data[i] = val;
		return;
	}
	vec_push(m->keys, key);
	vec_push(m->vals, val);
}

void *map_get(Map *m, char *key)
{
	for (int i = 0; i < m->keys->len; i++) {
		if (strcmp(m->keys->data[i], key))
			continue;
		return m->vals->data[i];
	}
	return NULL;
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
	char *ident;
} Token;

Vector *tokens;

void push_token(int type, int val, char *ident)
{
	Token *t = malloc(sizeof(Token));

	t->type = type;
	t->val = val;
	t->ident = ident;

	vec_push(tokens, t);
}

void push_char(int chr)
{
	push_token(chr, 0, NULL);
}

void push_num(int val)
{
	push_token(TK_NUM, val, NULL);
}

void push_ident(char *p, char *e)
{
	int len = e - p;
	char *ident = malloc(len + 1);

	memcpy(ident, p, len);
	ident[len] = 0;

	push_token(TK_IDENT, 0, ident);
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

int save_token(void)
{
	return pos;
}

void restore_token(int save)
{
	pos = save;
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
			push_char(TK_EQ);
			p += 2;
			continue;
		}
		if (*p == '!' && *(p + 1) == '=') {
			push_char(TK_NE);
			p += 2;
			continue;
		}
		if (*p == '+' || *p == '-' ||
		    *p == '&' || *p == '^' || *p == '|' ||
		    *p == '*' || *p == '/' ||
		    *p == '(' || *p == ')' ||
		    *p == '{' || *p == '}' ||
		    *p == '=' || *p == ';') {
			push_char(*p);
			p++;
			continue;
		}
		if (*p >= 'a' && *p <='z') {
			char *ident = p;

			while (isalnum(*p))
				p++;
			push_ident(ident, p);
			continue;
		}
		if (isdigit(*p)) {
			int val = strtol(p, &p, 0);

			push_num(val);
			continue;
		}
		fprintf(stderr, "syntax error: %s\n", p);
		exit(1);
	}
	push_char(TK_EOF);
}

enum {
	ND_NUM = 256,
	ND_IDENT,
	ND_EQ,
	ND_NE,
	ND_STATEMENT,
	ND_DECFUNC,
	ND_CALL,
};

typedef struct node {
	int type;
	struct node *lhs, *rhs;
	int val;	// for ND_NUM;
	char *name;	// for ND_DECFUNC
	int offset;	// local variable offset for ND_INDENT
	Map *vars;	// variables for DECFUNC
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

Node *new_ident(char *name)
{
	Node *node = malloc(sizeof(Node));

	node->type = ND_IDENT;
	node->lhs = NULL;
	node->rhs = NULL;
	node->val = 0;
	node->name = name;

	return node;
}

Node *new_decfunc(Node *child, char *name)
{
	Node *node = malloc(sizeof(Node));

	node->type = ND_DECFUNC;
	node->lhs = child;
	node->rhs = NULL;
	node->val = 0;
	node->name = name;

	return node;
}

Node *new_call(char *name)
{
	Node *node = malloc(sizeof(Node));

	node->type = ND_CALL;
	node->lhs = NULL;
	node->rhs = NULL;
	node->val = 0;
	node->name = name;

	return node;
}

/*
 * prog      : declare prog2
 * prog2     : e | prog
 * declare   : decfunc | assign
 * decfunc   : ident "(" ")" "{" prog "}"
 * assign    : expr assign2 ";"
 * assing2   : e | "=" expr assign2
 * expr      : expr_xor | expr "|" expr_xor
 * expr_xor  : expr_and | expr_xor "^" expr_and
 * expr_and  : expr_cmp | expr_and "&" expr_cmp
 * expr_cmp  : expr_plus | expr_cmp "==" expr_plus | expr_cmp "!=" expr_plus
 * expr_plus : mul | expr_plus "+" mul | expr_plus "-" mul
 * mul       : term | mul "*" term | mul "/" term
 * term      : num | ident | ident "(" ")" | "(" expr ")"
 */
Node *prog();
Node *expr();

Node *num_or_ident()
{
	Token *t = get_token();

	if (t->type == TK_NUM) {
		return new_node(ND_NUM, NULL, NULL, t->val);
	}
	if (t->type == TK_IDENT) {
		char *name = t->ident;

		t = get_token();
		if (t->type == '(') {
			t = get_token();
			if (t->type == ')')
				return new_call(name);
			unget_token();
		}
		unget_token();
		return new_ident(name);
	}
	// syntax error
	fprintf(stderr, "syntax error, expected num_or_ident but %d\n", t->type);
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

	for (;;) {
		Token *t = get_token();

		if (t->type == '*') {
			lhs = new_node('*', lhs, term(), 0);
		} else if (t->type == '/') {
			lhs = new_node('/', lhs, term(), 0);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_plus()
{
	Node *lhs = mul();

	for (;;) {
		Token *t = get_token();

		if (t->type == '+') {
			lhs = new_node('+', lhs, mul(), 0);
		} else if (t->type == '-') {
			lhs = new_node('-', lhs, mul(), 0);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_cmp()
{
	Node *lhs = expr_plus();

	for (;;) {
		Token *t = get_token();

		if (t->type == TK_EQ) {
			lhs = new_node(ND_EQ, lhs, expr_plus(), 0);
		} else if (t->type == TK_NE) {
			lhs = new_node(ND_NE, lhs, expr_plus(), 0);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_and()
{
	Node *lhs = expr_cmp();

	for (;;) {
		Token *t = get_token();

		if (t->type == '&') {
			lhs = new_node('&', lhs, expr_cmp(), 0);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_xor()
{
	Node *lhs = expr_and();

	for (;;) {
		Token *t = get_token();

		if (t->type == '^') {
			lhs = new_node('^', lhs, expr_and(), 0);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr()
{
	Node *lhs = expr_xor();

	for (;;) {
		Token *t = get_token();

		if (t->type == '|') {
			lhs = new_node('|', lhs, expr_xor(), 0);
		} else {
			unget_token();
			return lhs;
		}
	}
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

Node *declare()
{
	int save = save_token();
	Token *ident = get_token();

	if (ident->type != TK_IDENT)
		goto err;

	Token *lp = get_token();
	if (lp->type != '(')
		goto err;
	Token *rp = get_token();
	if (rp->type != ')')
		goto err;

	lp = get_token();
	if (lp->type != '{')
		goto err;

	// declare function
	Node *node = prog();

	rp = get_token();
	if (rp->type != '}')
		goto err;

	return new_decfunc(node, ident->ident);
err:
	restore_token(save);
	return assign();
}

Node *prog()
{
	Node *lhs = declare();

	for (;;) {
		Token *t = get_token();

		if (t->type == TK_EOF)
			return lhs;
		if (t->type == '}') {
			unget_token();
			return lhs;
		}

		unget_token();
		lhs = new_node(ND_STATEMENT, lhs, declare(), 0);
	}
}

typedef struct {
	char *name;
	int offset;
} Variable;

Map *variables;

void analyze(Node *node, int depth)
{
	if (node->type == ND_DECFUNC) {
		// setup
		Map *prev_vars = variables;
		node->vars = new_map();
		variables = node->vars;

		analyze(node->lhs, depth + 1);

		// restore
		variables = prev_vars;
		return;
	}
	if (node->type == ND_NUM) {
		// nothing to do
		return;
	}
	if (node->type == ND_IDENT) {
		// local variables
		Variable *var = map_get(variables, node->name);

		if (!var) {
			// new variable
			var = malloc(sizeof(Variable));
			var->name = node->name;
			var->offset = (variables->keys->len) * 8;
			map_set(variables, node->name, var);
		}
		node->offset = var->offset;
		return;
	}
	if (node->type == ND_CALL) {
		// nothing to do
		return;
	}
	if (node->lhs)
		analyze(node->lhs, depth + 1);
	if (node->rhs)
		analyze(node->rhs, depth + 1);
}

void gen_lval(Node *node)
{
	if (node->type == ND_IDENT) {
		// rbp - N * 8
		printf("#[%s]\n", node->name);
		printf("  mov rax, rbp\n");
		printf("  sub rax, %d\n", node->offset + 8);
		printf("  push rax\n");
		return;
	}

	fprintf(stderr, "not lval\n");
	exit(1);
}

void gen(Node *node)
{
	if (node->type == ND_DECFUNC) {
		printf(".global %s\n", node->name);
		printf("%s:\n", node->name);
		puts("  push rbp");
		puts("  mov rbp, rsp");
		if (node->vars->keys->len > 0)
			printf("  sub rsp, %d\n", (8 * node->vars->keys->len) + 8);
		gen(node->lhs);
		puts("  pop rax");
		puts("  mov rsp, rbp");
		puts("  pop rbp");
		puts("  ret");
		return;
	}
	if (node->type == ND_STATEMENT) {
		gen(node->lhs);
		puts("  pop rax");
		gen(node->rhs);
		// omit the below
		//puts("  pop rax");
		//puts("  push rax");
		return;
	}
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
	if (node->type == ND_CALL) {
		printf("  call %s\n", node->name);
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

	tokenize(p);

	Node *code = prog();
	analyze(code, 0);
	// generate stack machine
	puts(".intel_syntax noprefix");
	gen(code);

	return 0;
}
