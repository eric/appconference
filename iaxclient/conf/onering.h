#define ONERING_SIZE 8192
#define ONERING_SAMPLES ONERING_SIZE / 2
#define ONERING_MASK 0x1fff

struct ast_onering {
    unsigned short z1,z2; // [0...(ONERING_SAMPLES)-1]
    struct ast_frame f;
    short data[ONERING_SAMPLES];
    char framedata[ONERING_SIZE + AST_FRIENDLY_OFFSET];
};

struct ast_onering *ast_onering_new();

int ast_onering_write(struct ast_onering *s, struct ast_frame *f);

struct ast_frame *ast_onering_read(struct ast_onering *s, int max);

void ast_onering_free(struct ast_onering *s);



