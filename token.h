#ifndef TOKEN_H
#define TOKEN_H

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

extern Token *get_token(void);
extern void unget_token(void);
extern int save_token(void);
extern void restore_token(int save);

extern void tokenize(char *p);

#endif // TOKEN_H
