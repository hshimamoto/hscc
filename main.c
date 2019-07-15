#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>

#include "utils.h"
#include "token.h"

enum {
	ND_NUM = 256,
	ND_STRING,
	ND_IDENT,
	ND_BINOP,	// binary operation
	ND_EQ,
	ND_NE,
	ND_STATEMENT,
	ND_DECFUNC,
	ND_DECVAR,
	ND_CALL,
	ND_PARAMS,	// parameters
};

typedef struct node {
	int type;
	struct node *lhs, *rhs;
	Token *token;
	int optype;	// for ND_BINOP
	int val;	// for ND_NUM
	char *str;	// for ND_STRING
	char *name;	// for ND_DECFUNC
	int offset;	// local variable offset for ND_INDENT
	int reg;	// allocated register
	int regs;	// save registers on call for ND_CALL
	Vector *params;	// params for ND_DECFUNC and ND_CALL
	// local variables for ND_DECFUNC
	Map *vars;
} Node;

Node *new_node(int type, Node *lhs, Node *rhs, Token *token)
{
	Node *node = malloc(sizeof(Node));

	node->type = type;
	node->lhs = lhs;
	node->rhs = rhs;
	node->token = token;

	return node;
}

Node *new_num(int num, Token *token)
{
	Node *node = new_node(ND_NUM, NULL, NULL, token);

	node->val = num;

	return node;
}

Node *new_ident(char *name, Token *token)
{
	Node *node = new_node(ND_IDENT, NULL, NULL, token);

	node->name = name;

	return node;
}

Node *new_string(char *str, Token *token)
{
	Node *node = new_node(ND_STRING, NULL, NULL, token);

	node->str = str;

	return node;
}

Node *new_binop(int optype, Node *lhs, Node *rhs, Token *token)
{
	Node *node = new_node(ND_BINOP, lhs, rhs, token);

	node->optype = optype;

	return node;
}

Node *new_decfunc(Node *child, char *name, Vector *params, Token *token)
{
	Node *node = new_node(ND_DECFUNC, child, NULL, token);

	node->name = name;
	node->params = params;

	return node;
}

Node *new_decvar(char *name, Token *token)
{
	Node *node = new_node(ND_DECVAR, NULL, NULL, token);

	node->name = name;

	return node;
}

Node *new_call(char *name, Vector *params, Token *token)
{
	Node *node = new_node(ND_CALL, NULL, NULL, token);

	node->name = name;
	node->params = params;

	return node;
}

/*
 * prog      : declare prog2
 * prog2     : e | prog
 * declare   : decfunc | decvar | assign
 * decfunc   : "int" ident "(" ")" "{" prog "}"
 * decvar    : "int" ident ";"
 * assign    : expr assign2 ";"
 * assing2   : e | "=" expr assign2
 * expr      : expr_xor | expr "|" expr_xor
 * expr_xor  : expr_and | expr_xor "^" expr_and
 * expr_and  : expr_cmp | expr_and "&" expr_cmp
 * expr_cmp  : expr_plus | expr_cmp "==" expr_plus | expr_cmp "!=" expr_plus
 * expr_plus : mul | expr_plus "+" mul | expr_plus "-" mul
 * mul       : term | mul "*" term | mul "/" term
 * term      : num | ident | call | "(" expr ")"
 * call      : indent "(" params ")"
 * params    : e | ident "," params
 */
Node *prog(void);
Node *expr(void);

Vector *params(void)
{
	Vector *p = new_vector();
	Token *t = get_token();

	for (;;) {
		if (t->type == TK_IDENT) {
			// push variable names
			vec_push(p, t->str);
			t = get_token();
		}
		if (t->type == ')')
			return p;
		if (t->type != ',') {
			fprintf(stderr, "invalid params %d (%d:%d)\n", t->type, t->line, t->col);
			exit(1);
		}
		t = get_token();
	}
}

Node *num_or_ident(void)
{
	Token *t = get_token();

	if (t->type == TK_NUM) {
		return new_num(t->val, t);
	}
	if (t->type == TK_STRING) {
		return new_string(t->str, t);
	}
	if (t->type == TK_IDENT) {
		Token *curr = t;
		char *name = t->str;

		t = get_token();
		if (t->type == '(')
			return new_call(name, params(), curr);
		unget_token();
		return new_ident(name, curr);
	}
	// syntax error
	fprintf(stderr, "syntax error, expected num_or_ident but %d\n", t->type);
	exit(1);
}

Node *term(void)
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

Node *mul(void)
{
	Node *lhs = term();

	for (;;) {
		Token *t = get_token();

		if (t->type == '*' || t->type == '/') {
			lhs = new_binop(t->type, lhs, term(), t);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_plus(void)
{
	Node *lhs = mul();

	for (;;) {
		Token *t = get_token();

		if (t->type == '+' || t->type == '-') {
			lhs = new_binop(t->type, lhs, mul(), t);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_cmp(void)
{
	Node *lhs = expr_plus();

	for (;;) {
		Token *t = get_token();

		if (t->type == TK_EQ) {
			lhs = new_node(ND_EQ, lhs, expr_plus(), t);
		} else if (t->type == TK_NE) {
			lhs = new_node(ND_NE, lhs, expr_plus(), t);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_and(void)
{
	Node *lhs = expr_cmp();

	for (;;) {
		Token *t = get_token();

		if (t->type == '&') {
			lhs = new_binop('&', lhs, expr_cmp(), t);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr_xor(void)
{
	Node *lhs = expr_and();

	for (;;) {
		Token *t = get_token();

		if (t->type == '^') {
			lhs = new_binop('^', lhs, expr_and(), t);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *expr(void)
{
	Node *lhs = expr_xor();

	for (;;) {
		Token *t = get_token();

		if (t->type == '|') {
			lhs = new_binop('|', lhs, expr_xor(), t);
		} else {
			unget_token();
			return lhs;
		}
	}
}

Node *assign2(void)
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

		return new_node('=', lhs, rhs, t);
	}

	// bad
	fprintf(stderr, "no semicolumn (t->type %u %d:%d)\n", t->type, t->line, t->col);
	exit(1);
}

Node *assign(void)
{
	Node *lhs = expr();
	Node *rhs = assign2();

	if (rhs == NULL)
		return lhs;

	return new_node('=', lhs, rhs, NULL);
}

Node *declare_var(void)
{
	int save = save_token();
	Token *ident = get_token();

	if (ident->type != TK_IDENT)
		goto err;

	// check type "int"
	if (strcmp(ident->str, "int"))
		goto err;

	// get variable name
	ident = get_token();
	if (ident->type != TK_IDENT)
		goto err;

	return new_decvar(ident->str, ident);
err:
	restore_token(save);
	return NULL;
}

Vector *declare_params(void)
{
	Vector *p = new_vector();

	for (;;) {
		Node *param = declare_var();

		if (param == NULL)
			return p;

		vec_push(p, param);

		Token *t = get_token();
		if (t->type == ',')
			continue;

		unget_token();

		return p;
	}
}

Node *declare(void)
{
	int save = save_token();
	Token *ident = get_token();

	if (ident->type != TK_IDENT)
		goto err;

	// check type "int"
	if (strcmp(ident->str, "int"))
		goto err;

	// get variable name
	ident = get_token();
	if (ident->type != TK_IDENT)
		goto err;

	Token *t = get_token();
	if (t->type == ';') {
		// declare variable
		return new_decvar(ident->str, ident);
	} else if (t->type != '(')
		goto err;

	Vector *p = declare_params();

	t = get_token();
	if (t->type != ')')
		goto err;

	t = get_token();
	if (t->type != '{')
		goto err;

	// declare function
	Node *node = prog();

	t = get_token();
	if (t->type != '}')
		goto err;

	return new_decfunc(node, ident->str, p, ident);
err:
	restore_token(save);
	return assign();
}

Node *prog(void)
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
		lhs = new_node(ND_STATEMENT, lhs, declare(), NULL);
	}
}

enum {
	TYPE_PTR = 0,
	TYPE_INT,
};

typedef struct Type {
	int type;
	struct Type *ptrof;
} Type;

typedef struct {
	Type type;
	char *name;
	int offset;
} Variable;

Node *curr_block;
Map *variables;
int regs; // rdi, rsi, rcx, r8, r9, r10, r11
char *regname[7] = {
	"rdi", "rsi", "rcx", "r8", "r9", "r10", "r11"
};
char *regargs[6] = {
	"rdi", "rsi", "rdx", "rcx", "r8", "r9"
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

void analyze_add_localvar(Node *node, char *name)
{
	Variable *var = map_get(variables, name);

	if (var) {
		fprintf(stderr, "duplicate variable %s (%d:%d)\n", name,
				node->token->line, node->token->col);
		exit(1);
	}
	var = malloc(sizeof(Variable));
	var->type.type = TYPE_INT;
	var->type.ptrof = NULL;
	var->name = name;
	var->offset = (variables->keys->len) * 8;
	map_set(variables, name, var);
}

void analyze(Node *node, int depth)
{
	if (node->type == ND_DECFUNC) {
		// setup
		Node *prev_block = curr_block;
		Map *prev_vars = variables;
		node->vars = new_map();
		variables = node->vars;

		// map params to local variables
		Vector *p = node->params;
		for (int i = 0; i < p->len; i++) {
			Node *node = p->data[i]; // must be ND_DECVAR
			analyze_add_localvar(node, node->name);
		}
		analyze(node->lhs, depth + 1);

		// restore
		variables = prev_vars;
		curr_block = prev_block;
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
	if (node->type== ND_DECVAR) {
		analyze_add_localvar(node, node->name);
		return;
	}
	if (node->type == ND_NUM) {
		// allocate reg
		node->reg = reg_alloc();
		return;
	}
	if (node->type == ND_STRING) {
		// allocate reg
		node->reg = reg_alloc();
		return;
	}
	if (node->type == ND_IDENT) {
		// local variables
		Variable *var = map_get(variables, node->name);

		if (!var) {
			fprintf(stderr, "unkown variable %s (%d:%d)\n",
					node->name, node->token->line, node->token->col);
			exit(1);
		}
		node->offset = var->offset;
		// allocate reg
		node->reg = reg_alloc();
		return;
	}
	if (node->type == ND_CALL) {
		node->regs = regs; // we are using regs
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
	if (node->type == ND_BINOP ||
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

void emit_p(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
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

void gen_saveregs(Node *node)
{
	emit("# save regs %d", node->regs);
	for (int i = 0; i < 7; i++) {
		int b = 1 << i;
		if (node->regs & b)
			emit("push %s", regname[i]);
	}
}

void gen_restoreregs(Node *node)
{
	emit("# restore regs %d", node->regs);
	for (int i = 6; i >= 0; i--) {
		int b = 1 << i;
		if (node->regs & b)
			emit("pop %s", regname[i]);
	}
}

void gen(Node *node)
{
	if (node->type == ND_DECFUNC) {
		emit_global(node->name);
		emit("push rbp");
		emit("mov rbp, rsp");
		if (node->vars->keys->len > 0)
			emit("sub rsp, %d", (8 * node->vars->keys->len) + 8);
		// parameters
		Vector *p = node->params;
		emit("# params %d", p->len);
		for (int i = 0; i < p->len; i++) {
			Node *decvar = p->data[i];
			Variable *var = map_get(node->vars, decvar->name);
			emit("# %s [%s] offset %d", regargs[i], var->name, var->offset);
			emit("mov [rbp-%d], %s", var->offset + 8, regargs[i]);
		}
		// setup
		Node *prev_block = curr_block;
		Map *prev_vars = variables;
		curr_block = node;
		variables = node->vars;
		gen(node->lhs);
		emit("mov rax, %s", regname[node->lhs->reg]);
		emit("mov rsp, rbp");
		emit("pop rbp");
		emit("ret");
		// restore
		curr_block = prev_block;
		variables = prev_vars;
		return;
	}
	if (node->type == ND_DECVAR) {
		emit("#decvar %s", node->name);
		return;
	}
	if (node->type == ND_STATEMENT) {
		gen(node->lhs);
		gen(node->rhs);
		return;
	}
	if (node->type == ND_NUM) {
		emit("mov %s, %d", regname[node->reg], node->val);
		return;
	}
	if (node->type == ND_STRING) {
		static int strcnt = 0;
		emit_p(".data");
		emit_p("Lstr%d:", strcnt);
		emit(".asciz \"%s\"", node->str);
		emit_p(".text");
		emit("lea %s, [rip + Lstr%d]", regname[node->reg], strcnt);
		strcnt++;
		return;
	}
	if (node->type == ND_IDENT) {
		emit("mov %s, [rbp-%d]", regname[node->reg], node->offset + 8);
		return;
	}
	if (node->type == ND_CALL) {
		gen_saveregs(node);
		Vector *p = node->params;
		emit("# params %d", p->len);
		for (int i = 0; i < p->len; i++) {
			Variable *var = map_get(variables, p->data[i]);
			emit("# %s [%s] offset %d", regargs[i], var->name, var->offset);
			emit("mov %s, [rbp-%d]", regargs[i], var->offset + 8);
		}
		emit("call %s", node->name);
		gen_restoreregs(node);
		emit("mov %s, rax", regname[node->reg]);
		return;
	}
	if (node->type == '=') {
		gen(node->rhs);
		if (node->lhs->type != ND_IDENT) {
			fprintf(stderr, "not lval\n");
			exit(1);
		}
		emit("mov [rbp-%d], %s", node->lhs->offset + 8, regname[node->reg]);
		return;
	}

	gen(node->lhs);
	gen(node->rhs);

	int rl = node->lhs->reg, rr = node->rhs->reg;

	if (node->type == ND_BINOP) {
		char *op = NULL;

		if (node->optype == '*') {
			emit("mov rax, %s", regname[rl]);
			emit("mul %s", regname[rr]);
			emit("mov %s, rax", regname[rl]);
			return;
		}
		if (node->optype == '/') {
			emit("xor edx, edx");
			emit("mov rax, %s", regname[rl]);
			emit("div %s", regname[rr]);
			emit("mov %s, rax", regname[rl]);
			return;
		}

		if (node->optype == '+')
			op = "add";
		if (node->optype == '-')
			op = "sub";
		if (node->optype == '&')
			op = "and";
		if (node->optype == '^')
			op = "xor";
		if (node->optype == '|')
			op = "or";

		if (!op) {
			fprintf(stderr, "unknown BINOP %d\n", node->optype);
			exit(1);
		}

		emit("%s %s, %s", op, regname[rl], regname[rr]);
		return;
	}
	if (node->type == ND_EQ) {
		emit("cmp %s, %s", regname[rl], regname[rr]);
		emit("sete al");
		emit("movzb rax, al");
		emit("mov %s, rax", regname[rl]);
		return;
	}
	if (node->type == ND_NE) {
		emit("cmp %s, %s", regname[rl], regname[rr]);
		emit("setne al");
		emit("movzb rax, al");
		emit("mov %s, rax", regname[rl]);
		return;
	}
	fprintf(stderr, "error node->type = %d\n", node->type);
	exit(1);
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
	emit_p(".intel_syntax noprefix");
	emit_p(".text");
	gen(code);

	return 0;
}
