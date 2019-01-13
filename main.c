#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>

#include "utils.h"
#include "token.h"

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
	int reg;	// allocated register
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
int regs; // rdi, rsi, rcx, r8, r9, r10, r11
char *regname[7] = {
	"rdi", "rsi", "rcx", "r8", "r9", "r10", "r11"
};

int reg_alloc(void)
{
	for (int i = 0; i < 7; i++) {
		int b = 1 << i;
		if (regs & b)
			continue;
		regs |= b;
		return i;
	}
	fprintf(stderr, "no more regs\n");
	exit(1);
}

void reg_free(int r)
{
	int b = 1 << r;
	regs &= ~b;
}

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
	if (node->type == ND_STATEMENT) {
		int prev_regs = regs;

		// left
		regs = 0;
		analyze(node->lhs, depth + 1);
		// restore
		regs = prev_regs;
		// right
		regs = 0;
		analyze(node->rhs, depth + 1);
		// restore
		regs = prev_regs;

		return;
	}
	if (node->type == ND_NUM) {
		// allocate reg
		node->reg = reg_alloc();
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
		// allocate reg
		node->reg = reg_alloc();
		return;
	}
	if (node->type == ND_CALL) {
		// allocate reg
		node->reg = reg_alloc();
		return;
	}
	if (node->type == '=') {
		// lhs must be IDENT
		analyze(node->lhs, depth + 1);
		// don't care about regs
		reg_free(node->lhs->reg);
		analyze(node->rhs, depth + 1);
		// reuse rhs register
		node->reg = node->rhs->reg;

		return;
	}
	if (node->type == '+' || node->type == '-' ||
	    node->type == '&' || node->type == '^' || node->type == '|' ||
	    node->type == '*' || node->type == '/' ||
	    node->type == ND_EQ || node->type == ND_NE) {
		analyze(node->lhs, depth + 1);
		analyze(node->rhs, depth + 1);
		// reuse lhs register
		node->reg = node->lhs->reg;
		reg_free(node->rhs->reg);

		return;
	}
	if (node->lhs)
		analyze(node->lhs, depth + 1);
	if (node->rhs)
		analyze(node->rhs, depth + 1);
}

void emit(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("\t");
	vprintf(fmt, ap);
	printf("\n");
}

void emit_global(char *label)
{
	printf(".global %s\n", label);
	printf("%s:\n", label);
}

void gen_lval(Node *node)
{
	if (node->type == ND_IDENT) {
		// rbp - N * 8
		emit("#[%s]", node->name);
		emit("mov rax, rbp");
		emit("sub rax, %d", node->offset + 8);
		emit("push rax");
		return;
	}

	fprintf(stderr, "not lval\n");
	exit(1);
}

void gen(Node *node)
{
	if (node->type == ND_DECFUNC) {
		emit_global(node->name);
		emit("push rbp");
		emit("mov rbp, rsp");
		if (node->vars->keys->len > 0)
			emit("sub rsp, %d", (8 * node->vars->keys->len) + 8);
		gen(node->lhs);
		emit("pop rax");
		emit("mov rsp, rbp");
		emit("pop rbp");
		emit("ret");
		return;
	}
	if (node->type == ND_STATEMENT) {
		gen(node->lhs);
		emit("pop rax");
		gen(node->rhs);
		// omit the below
		//emit("pop rax");
		//emit("push rax");
		return;
	}
	if (node->type == ND_NUM) {
		emit("mov %s, %d", regname[node->reg], node->val);
		emit("push %s", regname[node->reg]);
		return;
	}
	if (node->type == ND_IDENT) {
		gen_lval(node);
		emit("pop rax");
		emit("mov %s, [rax]", regname[node->reg]);
		emit("push %s", regname[node->reg]);
		return;
	}
	if (node->type == ND_CALL) {
		emit("call %s", node->name);
		emit("mov %s, rax", regname[node->reg]);
		emit("push %s", regname[node->reg]);
		return;
	}
	if (node->type == '=') {
		gen_lval(node->lhs);
		gen(node->rhs);
		emit("pop rdi");
		emit("pop rax");
		emit("mov [rax], rdi");
		emit("push rdi");
		return;
	}

	gen(node->lhs);
	gen(node->rhs);

	int rl = node->lhs->reg, rr = node->rhs->reg;

	emit("pop %s", regname[rr]);
	emit("pop %s", regname[rl]);
	if (node->type == '+') {
		emit("add %s, %s", regname[rl], regname[rr]);
	} else if (node->type == '-') {
		emit("sub %s, %s", regname[rl], regname[rr]);
	} else if (node->type == '*') {
		emit("mov rax, %s", regname[rl]);
		emit("mul %s", regname[rr]);
		emit("mov %s, rax", regname[rl]);
	} else if (node->type == '/') {
		emit("xor edx, edx");
		emit("mov rax, %s", regname[rl]);
		emit("div %s", regname[rr]);
		emit("mov %s, rax", regname[rl]);
	} else if (node->type == ND_EQ) {
		emit("cmp %s, %s", regname[rl], regname[rr]);
		emit("sete al");
		emit("movzb rax, al");
		emit("mov %s, rax", regname[rl]);
	} else if (node->type == ND_NE) {
		emit("cmp %s, %s", regname[rl], regname[rr]);
		emit("setne al");
		emit("movzb rax, al");
		emit("mov %s, rax", regname[rl]);
	} else if (node->type == '&') {
		emit("and %s, %s", regname[rl], regname[rr]);
	} else if (node->type == '^') {
		emit("xor %s, %s", regname[rl], regname[rr]);
	} else if (node->type == '|') {
		emit("or %s, %s", regname[rl], regname[rr]);
	} else {
		fprintf(stderr, "error node->type = %d\n", node->type);
		exit(1);
	}
	emit("push %s", regname[node->reg]);
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
