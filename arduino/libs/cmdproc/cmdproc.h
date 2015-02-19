#define CMD_MAX_ARGS 5

typedef int (cmd_fn)(int argc, char *argv[]);

typedef struct {
    const char *name;
    cmd_fn *cmd;
    const char *help;
} cmd_t;

int cmd_process(const cmd_t *commands, char *line);

