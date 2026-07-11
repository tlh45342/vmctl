/*
 * cli.c - Foundry vmctl command interpreter
 *
 * This command layer is intentionally modeled after the earlier ARM VM CLI:
 *
 *     command table -> tokenizer -> dispatcher -> shared session
 *
 * The same evaluator is used for:
 *
 *     vmctl vm list
 *     vmctl                 (interactive mode)
 *     vmctl import file
 *     vmctl < file
 *
 * This implementation also supports:
 *
 *     - quoted arguments
 *     - #, ;, and // comments
 *     - local variables using $name or ${name}
 *     - command-result assignment, for example:
 *
 *           id = vm create
 *           use $id
 *
 * The temporary "shim" commands have intentionally been removed.
 */

#include "cli.h"
#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define MAX_LINE       2048
#define MAX_ARGS       64
#define MAX_RESPONSE   65536
#define MAX_TOKEN      1024
#define VMCTL_VERSION  "0.0.8"

typedef int (*command_fn)(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

typedef struct {
    const char *name;
    command_fn function;
    const char *help;
} command_t;

/* Command handlers. */
static int command_help(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_version(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_ping(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_config(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_vm(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_console(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_use(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_current(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_set(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_unset(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_vars(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_script(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static int command_quit(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
);

static const command_t COMMANDS[] = {
    {"help",    command_help,    "show command help"},
    {"version", command_version, "show vmctl version"},
    {"ping",    command_ping,    "ping Foundry"},
    {"config",  command_config,  "config show | config set <key> <value>"},
    {"vm",      command_vm,      "vm create | vm list"},
    {"console", command_console, "console list"},
    {"use",     command_use,     "use <vm-id-or-name>"},
    {"current", command_current, "show the current VM"},
    {"set",     command_set,     "set <name> <value>"},
    {"unset",   command_unset,   "unset <name>"},
    {"vars",    command_vars,    "show local variables"},
    {"do",      command_script,  "do <scriptfile>"},
    {"import",  command_script,  "import <scriptfile>"},
    {"quit",    command_quit,    "exit interactive mode"},
    {"exit",    command_quit,    "exit interactive mode"}
};

static void trim(char *text)
{
    char *start;
    size_t length;

    if (!text)
        return;

    start = text;

    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != text)
        memmove(text, start, strlen(start) + 1);

    length = strlen(text);

    while (length > 0 && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
}

static void strip_comments(char *text)
{
    int quoted = 0;
    char quote_character = '\0';
    char *cursor;

    if (!text)
        return;

    for (cursor = text; *cursor; ++cursor) {
        if (quoted) {
            if (*cursor == '\\' && cursor[1]) {
                ++cursor;
                continue;
            }

            if (*cursor == quote_character) {
                quoted = 0;
                quote_character = '\0';
            }

            continue;
        }

        if (*cursor == '"' || *cursor == '\'') {
            quoted = 1;
            quote_character = *cursor;
            continue;
        }

        if (*cursor == '#' || *cursor == ';') {
            *cursor = '\0';
            return;
        }

        /*
         * Treat // as a comment only when it begins a token.  This preserves
         * URLs such as http://192.168.150.83:5606 while still allowing:
         *
         *     // full-line comment
         *     vm list // trailing comment
         */
        if (*cursor == '/' && cursor[1] == '/' &&
            (cursor == text || isspace((unsigned char)cursor[-1]))) {
            *cursor = '\0';
            return;
        }
    }
}

/*
 * Tokenize a command line in place.
 *
 * Quotes are removed, quoted whitespace is preserved, and simple backslash
 * escaping is accepted.  argv entries point into the original line buffer.
 */
static int tokenize(char *line, char **argv, int max_args)
{
    char *read_cursor = line;
    char *write_cursor = line;
    int argc = 0;

    while (*read_cursor) {
        char quote_character = '\0';

        while (*read_cursor && isspace((unsigned char)*read_cursor))
            ++read_cursor;

        if (!*read_cursor)
            break;

        if (argc >= max_args)
            return -1;

        argv[argc++] = write_cursor;

        if (*read_cursor == '"' || *read_cursor == '\'')
            quote_character = *read_cursor++;

        while (*read_cursor) {
            if (quote_character) {
                if (*read_cursor == quote_character) {
                    ++read_cursor;
                    break;
                }

                if (*read_cursor == '\\' && read_cursor[1]) {
                    ++read_cursor;

                    if (*read_cursor == 'n')
                        *write_cursor++ = '\n';
                    else if (*read_cursor == 't')
                        *write_cursor++ = '\t';
                    else
                        *write_cursor++ = *read_cursor;

                    ++read_cursor;
                    continue;
                }

                *write_cursor++ = *read_cursor++;
                continue;
            }

            if (isspace((unsigned char)*read_cursor))
                break;

            if (*read_cursor == '"' || *read_cursor == '\'') {
                quote_character = *read_cursor++;
                continue;
            }

            if (*read_cursor == '\\' && read_cursor[1])
                ++read_cursor;

            *write_cursor++ = *read_cursor++;
        }

        /*
         * Consume the delimiter before writing the token terminator.  Since
         * tokenization is in-place, writing '\0' first would overwrite the
         * whitespace currently under read_cursor and prematurely end parsing
         * after the first token.
         */
        while (*read_cursor && isspace((unsigned char)*read_cursor))
            ++read_cursor;

        *write_cursor++ = '\0';
    }

    return argc;
}

static int expand_token(
    vmctl_session_t *session,
    const char *input,
    char *output,
    size_t output_size
)
{
    const char *cursor = input;
    size_t used = 0;

    while (*cursor) {
        if (*cursor != '$') {
            if (used + 1 >= output_size)
                return 0;

            output[used++] = *cursor++;
            continue;
        }

        ++cursor;

        if (*cursor == '$') {
            if (used + 1 >= output_size)
                return 0;

            output[used++] = *cursor++;
            continue;
        }

        {
            char variable_name[128];
            size_t name_length = 0;
            const char *value;

            if (*cursor == '{') {
                ++cursor;

                while (*cursor &&
                       *cursor != '}' &&
                       name_length + 1 < sizeof(variable_name)) {
                    variable_name[name_length++] = *cursor++;
                }

                if (*cursor != '}') {
                    fprintf(stderr, "vmctl: malformed variable expression\n");
                    return 0;
                }

                ++cursor;
            } else {
                while ((*cursor == '_' ||
                        isalnum((unsigned char)*cursor)) &&
                       name_length + 1 < sizeof(variable_name)) {
                    variable_name[name_length++] = *cursor++;
                }
            }

            if (name_length == 0) {
                fprintf(stderr, "vmctl: empty variable name\n");
                return 0;
            }

            variable_name[name_length] = '\0';
            value = session_get(session, variable_name);

            if (!value) {
                fprintf(
                    stderr,
                    "vmctl: undefined variable: %s\n",
                    variable_name
                );
                return 0;
            }

            if (used + strlen(value) >= output_size)
                return 0;

            memcpy(output + used, value, strlen(value));
            used += strlen(value);
        }
    }

    output[used] = '\0';
    return 1;
}

static int create_temp_file(
    char *output,
    size_t output_size,
    FILE **file_out
)
{
#ifdef _WIN32
    char temp_directory[MAX_PATH];
    char temp_filename[MAX_PATH];
    DWORD directory_length;
    FILE *file;

    directory_length = GetTempPathA(
        (DWORD)sizeof(temp_directory),
        temp_directory
    );

    if (directory_length == 0 ||
        directory_length >= sizeof(temp_directory)) {
        return 0;
    }

    if (GetTempFileNameA(
            temp_directory,
            "vmt",
            0,
            temp_filename) == 0) {
        return 0;
    }

    if (strlen(temp_filename) + 1 > output_size) {
        DeleteFileA(temp_filename);
        return 0;
    }

    memcpy(output, temp_filename, strlen(temp_filename) + 1);

    file = fopen(output, "w+b");
    if (!file) {
        DeleteFileA(output);
        return 0;
    }

    *file_out = file;
    return 1;
#else
    char template_path[] = "/tmp/vmctl-XXXXXX";
    int descriptor;
    FILE *file;

    descriptor = mkstemp(template_path);
    if (descriptor < 0)
        return 0;

    if (strlen(template_path) + 1 > output_size) {
        close(descriptor);
        unlink(template_path);
        return 0;
    }

    file = fdopen(descriptor, "w+b");
    if (!file) {
        close(descriptor);
        unlink(template_path);
        return 0;
    }

    memcpy(output, template_path, strlen(template_path) + 1);
    *file_out = file;
    return 1;
#endif
}

static int read_text_file(
    const char *path,
    char *output,
    size_t output_size
)
{
    FILE *file;
    size_t used;
    int ok;

    if (!output || output_size == 0)
        return 0;

    file = fopen(path, "rb");
    if (!file)
        return 0;

    used = fread(output, 1, output_size - 1, file);
    output[used] = '\0';

    ok = !ferror(file);
    fclose(file);

    return ok;
}

static int request(
    vmctl_session_t *session,
    const char *method,
    const char *path,
    const char *json,
    char *response,
    size_t response_size
)
{
    char output_path[512];
    char body_path[512] = "";
    char command[4096];
    FILE *output_file = NULL;
    FILE *body_file = NULL;
    int rc;

    if (!create_temp_file(
            output_path,
            sizeof(output_path),
            &output_file)) {
        fprintf(stderr, "vmctl: could not create response file\n");
        return 1;
    }

    fclose(output_file);
    output_file = NULL;

    if (json) {
        if (!create_temp_file(
                body_path,
                sizeof(body_path),
                &body_file)) {
            remove(output_path);
            fprintf(stderr, "vmctl: could not create request body\n");
            return 1;
        }

        if (fputs(json, body_file) == EOF) {
            fclose(body_file);
            remove(body_path);
            remove(output_path);
            fprintf(stderr, "vmctl: could not write request body\n");
            return 1;
        }

        fclose(body_file);
        body_file = NULL;

        snprintf(
            command,
            sizeof(command),
            "curl -fsS -X %s "
            "-H \"Content-Type: application/json\" "
            "--data-binary @\"%s\" "
            "-o \"%s\" "
            "\"%s%s\"",
            method,
            body_path,
            output_path,
            session->endpoint,
            path
        );
    } else {
        snprintf(
            command,
            sizeof(command),
            "curl -fsS -X %s -o \"%s\" \"%s%s\"",
            method,
            output_path,
            session->endpoint,
            path
        );
    }

    rc = system(command);

    if (body_path[0])
        remove(body_path);

    if (rc != 0) {
        remove(output_path);
        fprintf(stderr, "vmctl: request failed: %s %s\n", method, path);
        return 1;
    }

    if (!read_text_file(output_path, response, response_size)) {
        remove(output_path);
        fprintf(stderr, "vmctl: could not read response\n");
        return 1;
    }

    remove(output_path);
    return 0;
}

static int extract_vm_id(
    const char *json,
    char *output,
    size_t output_size
)
{
    const char *cursor = json;

    while ((cursor = strstr(cursor, "\"id\"")) != NULL) {
        const char *colon = strchr(cursor, ':');
        size_t used = 0;

        if (!colon)
            break;

        cursor = colon + 1;

        while (*cursor && *cursor != '"')
            ++cursor;

        if (*cursor != '"')
            break;

        ++cursor;

        while (*cursor &&
               *cursor != '"' &&
               used + 1 < output_size) {
            output[used++] = *cursor++;
        }

        output[used] = '\0';

        if (strncmp(output, "vm-", 3) == 0)
            return 1;
    }

    return 0;
}

static int dispatch(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    size_t index;

    if (argc == 0)
        return 0;

    for (index = 0;
         index < sizeof(COMMANDS) / sizeof(COMMANDS[0]);
         ++index) {
        if (strcmp(argv[0], COMMANDS[index].name) == 0) {
            return COMMANDS[index].function(
                session,
                argc,
                argv,
                capture,
                capture_size
            );
        }
    }

    fprintf(stderr, "vmctl: unknown command: %s\n", argv[0]);
    return 1;
}

static int evaluate_command(
    vmctl_session_t *session,
    char *command_text,
    char *capture,
    size_t capture_size
)
{
    char *argv[MAX_ARGS];
    char expanded[MAX_ARGS][MAX_TOKEN];
    int argc;
    int index;

    argc = tokenize(command_text, argv, MAX_ARGS);

    if (argc < 0) {
        fprintf(stderr, "vmctl: too many arguments\n");
        return 1;
    }

    for (index = 0; index < argc; ++index) {
        if (!expand_token(
                session,
                argv[index],
                expanded[index],
                sizeof(expanded[index]))) {
            return 1;
        }

        argv[index] = expanded[index];
    }

    if (argc == 0)
        return 0;

    return dispatch(
        session,
        argc,
        argv,
        capture,
        capture_size
    );
}

int cli_eval_line(vmctl_session_t *session, const char *line)
{
    char buffer[MAX_LINE];
    char capture[MAX_RESPONSE];
    char *equals;
    int rc;

    if (strlen(line) >= sizeof(buffer)) {
        fprintf(stderr, "vmctl: input line is too long\n");
        return 1;
    }

    memcpy(buffer, line, strlen(line) + 1);

    strip_comments(buffer);
    trim(buffer);

    if (!buffer[0])
        return 0;

    equals = strchr(buffer, '=');

    if (equals) {
        char variable_name[128];
        char *command_text;
        size_t name_length;

        *equals = '\0';
        trim(buffer);

        name_length = strlen(buffer);

        if (name_length == 0) {
            fprintf(stderr, "vmctl: assignment requires a variable name\n");
            return 1;
        }

        if (name_length >= sizeof(variable_name)) {
            fprintf(stderr, "vmctl: variable name is too long\n");
            return 1;
        }

        memcpy(variable_name, buffer, name_length + 1);

        command_text = equals + 1;
        trim(command_text);

        if (!command_text[0]) {
            fprintf(stderr, "vmctl: assignment requires a command\n");
            return 1;
        }

        capture[0] = '\0';

        rc = evaluate_command(
            session,
            command_text,
            capture,
            sizeof(capture)
        );

        if (rc == 0) {
            trim(capture);

            if (!session_set(session, variable_name, capture)) {
                fprintf(stderr, "vmctl: could not set variable\n");
                rc = 1;
            }
        }
    } else {
        capture[0] = '\0';

        rc = evaluate_command(
            session,
            buffer,
            capture,
            sizeof(capture)
        );

        if (rc == 0 && capture[0])
            printf("%s\n", capture);
    }

    session->last_status = rc;
    return rc;
}

int cli_run(
    vmctl_session_t *session,
    FILE *input,
    int interactive
)
{
    char line[MAX_LINE];

    for (;;) {
        int rc;

        if (interactive) {
            fputs("vmctl> ", stdout);
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), input))
            break;

        rc = cli_eval_line(session, line);

        if (rc == 2)
            break;
    }

    return 0;
}

int cli_run_argv(
    vmctl_session_t *session,
    int argc,
    char **argv
)
{
    char line[MAX_LINE] = "";
    size_t used = 0;
    int index;

    for (index = 1; index < argc; ++index) {
        size_t argument_length = strlen(argv[index]);

        if (used > 0) {
            if (used + 1 >= sizeof(line)) {
                fprintf(stderr, "vmctl: command line is too long\n");
                return 1;
            }

            line[used++] = ' ';
        }

        if (used + argument_length >= sizeof(line)) {
            fprintf(stderr, "vmctl: command line is too long\n");
            return 1;
        }

        memcpy(line + used, argv[index], argument_length);
        used += argument_length;
        line[used] = '\0';
    }

    return cli_eval_line(session, line);
}

static int command_help(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    size_t index;

    (void)session;
    (void)argc;
    (void)argv;
    (void)capture;
    (void)capture_size;

    puts("vmctl commands:");

    for (index = 0;
         index < sizeof(COMMANDS) / sizeof(COMMANDS[0]);
         ++index) {
        printf(
            "  %-9s %s\n",
            COMMANDS[index].name,
            COMMANDS[index].help
        );
    }

    puts("\nExamples:");
    puts("  config show");
    puts("  config set endpoint http://192.168.150.83:5606");
    puts("  set endpoint http://192.168.160.73:5606  # session only");
    puts("  vm create");
    puts("  id = vm create");
    puts("  use $id");
    puts("  current");
    puts("  do build.vmctl");
    puts("  import build.vmctl");

    return 0;
}

static int command_version(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)session;
    (void)argc;
    (void)argv;

    snprintf(capture, capture_size, "vmctl %s", VMCTL_VERSION);
    return 0;
}

static int command_ping(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)argc;
    (void)argv;

    return request(
        session,
        "GET",
        "/v1/ping",
        NULL,
        capture,
        capture_size
    );
}

static int command_config(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)capture;
    (void)capture_size;

    if (argc < 2 || strcmp(argv[1], "show") == 0)
        return config_show(session);

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: config set <endpoint|account|region> <value>\n");
            return 1;
        }

        return config_set_value(session, argv[2], argv[3]);
    }

    fprintf(stderr, "usage: config show\n");
    fprintf(stderr, "       config set <endpoint|account|region> <value>\n");
    return 1;
}

static int command_vm(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    char response[MAX_RESPONSE];

    if (argc < 2) {
        fprintf(stderr, "usage: vm create | vm list\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        return request(
            session,
            "GET",
            "/v1/vms",
            NULL,
            capture,
            capture_size
        );
    }

    if (strcmp(argv[1], "create") == 0) {
        if (argc != 2) {
            fprintf(stderr, "usage: vm create\n");
            return 1;
        }

        if (request(
                session,
                "POST",
                "/v1/vms",
                "{\"kind\":\"generic\"}",
                response,
                sizeof(response)) != 0) {
            return 1;
        }

        if (!extract_vm_id(response, capture, capture_size)) {
            fprintf(stderr, "vmctl: no VM id in response\n%s\n", response);
            return 1;
        }

        return 0;
    }

    fprintf(stderr, "vmctl: unknown vm command: %s\n", argv[1]);
    return 1;
}

static int command_console(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    if (argc < 2 || strcmp(argv[1], "list") == 0) {
        return request(
            session,
            "GET",
            "/v1/consoles",
            NULL,
            capture,
            capture_size
        );
    }

    fprintf(stderr, "usage: console list\n");
    return 1;
}

static int command_use(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    if (argc != 2) {
        fprintf(stderr, "usage: use <vm-id-or-name>\n");
        return 1;
    }

    if (strlen(argv[1]) >= sizeof(session->current_vm)) {
        fprintf(stderr, "vmctl: VM identifier is too long\n");
        return 1;
    }

    memcpy(
        session->current_vm,
        argv[1],
        strlen(argv[1]) + 1
    );

    if (!session_set(session, "vm", session->current_vm)) {
        fprintf(stderr, "vmctl: could not save current VM\n");
        return 1;
    }

    snprintf(capture, capture_size, "%s", session->current_vm);
    return 0;
}

static int command_current(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)argc;
    (void)argv;

    if (!session->current_vm[0]) {
        fprintf(stderr, "vmctl: no current VM\n");
        return 1;
    }

    snprintf(capture, capture_size, "%s", session->current_vm);
    return 0;
}

static int command_set(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    if (argc != 3) {
        fprintf(stderr, "usage: set <name> <value>\n");
        return 1;
    }

    /*
     * Known connection values may be overridden for this process only.
     * Persistent changes belong to: config set <key> <value>
     */
    if (strcmp(argv[1], "endpoint") == 0) {
        if (strlen(argv[2]) >= sizeof(session->endpoint)) {
            fprintf(stderr, "vmctl: endpoint is too long\n");
            return 1;
        }
        memcpy(session->endpoint, argv[2], strlen(argv[2]) + 1);
    } else if (strcmp(argv[1], "account") == 0) {
        if (strlen(argv[2]) >= sizeof(session->account)) {
            fprintf(stderr, "vmctl: account is too long\n");
            return 1;
        }
        memcpy(session->account, argv[2], strlen(argv[2]) + 1);
    } else if (strcmp(argv[1], "region") == 0) {
        if (strlen(argv[2]) >= sizeof(session->region)) {
            fprintf(stderr, "vmctl: region is too long\n");
            return 1;
        }
        memcpy(session->region, argv[2], strlen(argv[2]) + 1);
    }

    if (!session_set(session, argv[1], argv[2])) {
        fprintf(stderr, "vmctl: could not set variable\n");
        return 1;
    }

    snprintf(capture, capture_size, "%s", argv[2]);
    return 0;
}

static int command_unset(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)capture;
    (void)capture_size;

    if (argc != 2) {
        fprintf(stderr, "usage: unset <name>\n");
        return 1;
    }

    if (!session_unset(session, argv[1])) {
        fprintf(stderr, "vmctl: variable not found: %s\n", argv[1]);
        return 1;
    }

    return 0;
}

static int command_vars(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)argc;
    (void)argv;
    (void)capture;
    (void)capture_size;

    session_dump(session);
    return 0;
}

static int command_script(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    FILE *file;
    int rc;

    (void)capture;
    (void)capture_size;

    if (argc != 2) {
        fprintf(stderr, "usage: do <scriptfile>\n");
        fprintf(stderr, "       import <scriptfile>\n");
        return 1;
    }

    file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "vmctl: cannot open %s\n", argv[1]);
        return 1;
    }

    rc = cli_run(session, file, 0);
    fclose(file);

    return rc;
}

static int command_quit(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    (void)session;
    (void)argc;
    (void)argv;
    (void)capture;
    (void)capture_size;

    return 2;
}
