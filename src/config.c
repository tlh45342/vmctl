/*
 * config.c - persistent vmctl configuration
 *
 * Persistent configuration lives in:
 *
 *     ~/.foundry/config
 *
 * The command:
 *
 *     config set endpoint http://host:5606
 *
 * updates the [default] section on disk and updates the current session.
 * The command:
 *
 *     set endpoint http://host:5606
 *
 * changes only the current process and is implemented by cli.c.
 */

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define VMCTL_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define VMCTL_MKDIR(path) mkdir((path), 0700)
#endif

#define CONFIG_LINE_SIZE 512

static void trim(char *text)
{
    char *start = text;
    size_t length;

    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != text)
        memmove(text, start, strlen(start) + 1);

    length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
}

static const char *home_directory(void)
{
    const char *home = getenv("HOME");

    if (home && *home)
        return home;

    home = getenv("USERPROFILE");
    if (home && *home)
        return home;

    return ".";
}

static int config_paths(
    char *directory,
    size_t directory_size,
    char *path,
    size_t path_size,
    char *temporary,
    size_t temporary_size
)
{
    int n1 = snprintf(directory, directory_size, "%s/.foundry", home_directory());
    int n2 = snprintf(path, path_size, "%s/config", directory);
    int n3 = snprintf(temporary, temporary_size, "%s/config.tmp", directory);

    return n1 > 0 && n1 < (int)directory_size &&
           n2 > 0 && n2 < (int)path_size &&
           n3 > 0 && n3 < (int)temporary_size;
}

static int copy_value(char *destination, size_t size, const char *value)
{
    size_t length = strlen(value);

    if (length >= size)
        return 0;

    memcpy(destination, value, length + 1);
    return 1;
}

static int apply_value(
    vmctl_session_t *session,
    const char *key,
    const char *value
)
{
    if (strcmp(key, "endpoint") == 0)
        return copy_value(session->endpoint, sizeof(session->endpoint), value);

    if (strcmp(key, "account") == 0)
        return copy_value(session->account, sizeof(session->account), value);

    if (strcmp(key, "region") == 0)
        return copy_value(session->region, sizeof(session->region), value);

    return 0;
}

int config_load_default(vmctl_session_t *session)
{
    char directory[512];
    char path[512];
    char temporary[512];
    char line[CONFIG_LINE_SIZE];
    char section[128] = "";
    FILE *file;

    if (!config_paths(
            directory, sizeof(directory),
            path, sizeof(path),
            temporary, sizeof(temporary))) {
        fprintf(stderr, "vmctl: config path is too long\n");
        return 1;
    }

    file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "vmctl: cannot open %s\n", path);
        return 1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *equals;

        trim(line);
        if (!line[0] || line[0] == '#' || line[0] == ';')
            continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            size_t length;

            if (!end)
                continue;

            *end = '\0';
            length = strlen(line + 1);
            if (length >= sizeof(section)) {
                fclose(file);
                fprintf(stderr, "vmctl: config section name is too long\n");
                return 1;
            }

            memcpy(section, line + 1, length + 1);
            continue;
        }

        if (strcmp(section, "default") != 0)
            continue;

        equals = strchr(line, '=');
        if (!equals)
            continue;

        *equals = '\0';
        trim(line);
        trim(equals + 1);

        if (strcmp(line, "endpoint") == 0 ||
            strcmp(line, "account") == 0 ||
            strcmp(line, "region") == 0) {
            if (!apply_value(session, line, equals + 1)) {
                fclose(file);
                fprintf(stderr, "vmctl: invalid or oversized config value: %s\n", line);
                return 1;
            }
        }
    }

    fclose(file);

    if (!session->endpoint[0]) {
        fprintf(stderr, "vmctl: default endpoint is missing\n");
        return 1;
    }

    return 0;
}

int config_show(vmctl_session_t *session)
{
    printf("endpoint = %s\n", session->endpoint);
    printf("account = %s\n", session->account);
    printf("region = %s\n", session->region);
    return 0;
}

int config_set_value(
    vmctl_session_t *session,
    const char *key,
    const char *value
)
{
    char directory[512];
    char path[512];
    char temporary[512];
    char line[CONFIG_LINE_SIZE];
    FILE *input;
    FILE *output;
    int in_default = 0;
    int saw_default = 0;
    int replaced = 0;

    if (strcmp(key, "endpoint") != 0 &&
        strcmp(key, "account") != 0 &&
        strcmp(key, "region") != 0) {
        fprintf(stderr, "vmctl: unknown config key: %s\n", key);
        return 1;
    }

    if (!apply_value(session, key, value)) {
        fprintf(stderr, "vmctl: config value is too long\n");
        return 1;
    }

    if (!config_paths(
            directory, sizeof(directory),
            path, sizeof(path),
            temporary, sizeof(temporary))) {
        fprintf(stderr, "vmctl: config path is too long\n");
        return 1;
    }

    VMCTL_MKDIR(directory);

    input = fopen(path, "r");
    output = fopen(temporary, "w");
    if (!output) {
        if (input)
            fclose(input);
        fprintf(stderr, "vmctl: cannot write %s\n", temporary);
        return 1;
    }

    if (input) {
        while (fgets(line, sizeof(line), input)) {
            char working[CONFIG_LINE_SIZE];

            memcpy(working, line, strlen(line) + 1);
            trim(working);

            if (working[0] == '[') {
                char *end = strchr(working, ']');

                if (in_default && !replaced) {
                    fprintf(output, "%s = %s\n", key, value);
                    replaced = 1;
                }

                in_default = 0;
                if (end) {
                    *end = '\0';
                    if (strcmp(working + 1, "default") == 0) {
                        in_default = 1;
                        saw_default = 1;
                    }
                }

                fputs(line, output);
                continue;
            }

            if (in_default) {
                char candidate[CONFIG_LINE_SIZE];
                char *equals;

                memcpy(candidate, working, strlen(working) + 1);
                equals = strchr(candidate, '=');
                if (equals) {
                    *equals = '\0';
                    trim(candidate);
                    if (strcmp(candidate, key) == 0) {
                        fprintf(output, "%s = %s\n", key, value);
                        replaced = 1;
                        continue;
                    }
                }
            }

            fputs(line, output);
        }

        fclose(input);
    }

    if (!saw_default) {
        fprintf(output, "\n[default]\n");
        fprintf(output, "endpoint = %s\n", session->endpoint);
        fprintf(output, "account = %s\n", session->account);
        fprintf(output, "region = %s\n", session->region);
    } else if (in_default && !replaced) {
        fprintf(output, "%s = %s\n", key, value);
    }

    if (fclose(output) != 0) {
        remove(temporary);
        fprintf(stderr, "vmctl: could not finish writing config\n");
        return 1;
    }

#ifdef _WIN32
    remove(path);
#endif

    if (rename(temporary, path) != 0) {
        remove(temporary);
        fprintf(stderr, "vmctl: could not replace %s\n", path);
        return 1;
    }

    printf("%s = %s\n", key, value);
    return 0;
}
