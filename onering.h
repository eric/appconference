#define ONERING_SIZE 8000
#define ONERING_SAMPLES ONERING_SIZE / 2

struct ast_onering {
	char data[ONERING_SIZE];
	int len;
};

struct ast_onering *ast_onering_new(void);

int ast_onering_write(struct ast_onering *s, struct ast_frame *f);

struct ast_frame *ast_onering_read(struct ast_onering *s, int max);

void ast_onering_free(struct ast_onering *s);



