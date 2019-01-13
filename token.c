#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "token.h"

Vector *tokens;
int col, line;

void push_token(int type, int val, char *ident)
{
	Token *t = malloc(sizeof(Token));

	t->type = type;
	t->val = val;
	t->ident = ident;
	t->col = col;
	t->line = line;

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
	char *head = p;

	while (*p) {
		if (*p == '\n') {
			p++;
			line++;
			head = p;
			continue;
		}
		if (isspace(*p)) {
			p++;
			continue;
		}
		col = p - head;
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
		fprintf(stderr, "syntax error: %s line:%d col:%d\n", p, line, col);
		exit(1);
	}
	push_char(TK_EOF);
}

