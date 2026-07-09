// vmctl.c - Foundry VM control shim
// build: make

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define VMCTL_VERSION "0.0.2"
#define MAX_LINE 512
#define MAX_VAL  256

typedef struct {
    char endpoint[MAX_VAL];
    char account[MAX_VAL];
    char region[MAX_VAL];
} Profile;

static void trim(char *s) {
    char *p = s;

    while (isspace((unsigned char)*p)) p++;

    if (p != s)
        memmove(s, p, strlen(p) + 1);

    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = 0;
}

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    if (h && *h) return h;

    h = getenv("USERPROFILE");
    if (h && *h) return h;

    return ".";
}

static void build_config_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/.foundry/config", home_dir());
}

static int read_profile(const char *wanted, Profile *p) {
    char path[512];
    char line[MAX_LINE];
    char current_profile[MAX_VAL] = "";

    memset(p, 0, sizeof(*p));
    build_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "vmctl: could not open config: %s\n", path);
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        trim(line);

        if (line[0] == 0 || line[0] == '#' || line[0] == ';')
            continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (!end) continue;

            *end = 0;
            snprintf(current_profile, sizeof(current_profile),
                     "%.*s",
                     (int)sizeof(current_profile) - 1,
                     line + 1);
            current_profile[sizeof(current_profile) - 1] = 0;
            continue;
        }

        if (strcmp(current_profile, wanted) != 0)
            continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = 0;
        char *key = line;
        char *val = eq + 1;

        trim(key);
        trim(val);

        if (strcmp(key, "endpoint") == 0)
            snprintf(p->endpoint, sizeof(p->endpoint), "%s", val);
        else if (strcmp(key, "account") == 0)
            snprintf(p->account, sizeof(p->account), "%s", val);
        else if (strcmp(key, "region") == 0)
            snprintf(p->region, sizeof(p->region), "%s", val);
    }

    fclose(f);

    return p->endpoint[0] != 0;
}

static int shell_quote_json_string(const char *in, char *out, size_t out_sz) {
    // Produces a JSON string value with minimal escaping for names passed to curl.
    // Returns 1 on success, 0 if the output buffer is too small.
    size_t j = 0;

    if (j + 1 >= out_sz) return 0;
    out[j++] = '"';

    for (size_t i = 0; in[i]; i++) {
        unsigned char c = (unsigned char)in[i];

        if (c == '"' || c == '\\') {
            if (j + 2 >= out_sz) return 0;
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c == '\n') {
            if (j + 2 >= out_sz) return 0;
            out[j++] = '\\'; out[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= out_sz) return 0;
            out[j++] = '\\'; out[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= out_sz) return 0;
            out[j++] = '\\'; out[j++] = 't';
        } else if (c < 0x20) {
            // Reject other control chars for now. Names do not need them.
            return 0;
        } else {
            if (j + 1 >= out_sz) return 0;
            out[j++] = (char)c;
        }
    }

    if (j + 2 > out_sz) return 0;
    out[j++] = '"';
    out[j] = 0;
    return 1;
}

static int load_default_profile(Profile *p) {
    if (!read_profile("default", p))
        return 0;
    return 1;
}

static int run_curl_get(const char *path) {
    Profile p;
    if (!load_default_profile(&p))
        return 1;

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "curl -fsS \"%s%s\"",
             p.endpoint, path);

    printf("endpoint = %s\n\n", p.endpoint);

    int rc = system(cmd);
    printf("\n");

    if (rc != 0) {
        fprintf(stderr, "vmctl: request failed: GET %s\n", path);
        return 1;
    }

    return 0;
}

static int run_curl_post_json(const char *path, const char *json) {
    Profile p;
    if (!load_default_profile(&p))
        return 1;

    /*
       Do not pass JSON directly through the shell. On Windows cmd.exe, single
       quotes are not quoting characters, so curl would receive malformed JSON.
       Write the body to a small temp file and let curl read it with @file.
    */
    char tmpname[L_tmpnam];
    if (tmpnam(tmpname) == NULL) {
        fprintf(stderr, "vmctl: could not create temp filename\n");
        return 1;
    }

    FILE *fp = fopen(tmpname, "wb");
    if (!fp) {
        fprintf(stderr, "vmctl: could not open temp file for POST body: %s\n", tmpname);
        return 1;
    }
    fputs(json, fp);
    fclose(fp);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "curl -fsS -X POST -H \"Content-Type: application/json\" "
             "--data-binary @\"%s\" \"%s%s\"",
             tmpname, p.endpoint, path);

    printf("endpoint = %s\n\n", p.endpoint);

    int rc = system(cmd);
    remove(tmpname);
    printf("\n");

    if (rc != 0) {
        fprintf(stderr, "vmctl: request failed: POST %s\n", path);
        return 1;
    }

    return 0;
}

static int cmd_ping(void) {
    int rc = run_curl_get("/v1/ping");
    if (rc != 0)
        fprintf(stderr, "vmctl: ping failed\n");
    return rc;
}

static int cmd_shim(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: vmctl shim create <name>\n");
        fprintf(stderr, "       vmctl shim list\n");
        return 1;
    }

    if (strcmp(argv[2], "create") == 0) {
        if (argc < 4) {
            fprintf(stderr, "usage: vmctl shim create <name>\n");
            return 1;
        }

        char qname[512];
        char json[1024];

        if (!shell_quote_json_string(argv[3], qname, sizeof(qname))) {
            fprintf(stderr, "vmctl: invalid or too-long VM name\n");
            return 1;
        }

        snprintf(json, sizeof(json),
                 "{\"kind\":\"shim\",\"name\":%s}",
                 qname);

        printf("creating shim VM: %s\n", argv[3]);
        return run_curl_post_json("/v1/vms", json);
    }

    if (strcmp(argv[2], "list") == 0) {
        return run_curl_get("/v1/vms?kind=shim");
    }

    fprintf(stderr, "vmctl: unknown shim command: %s\n", argv[2]);
    fprintf(stderr, "usage: vmctl shim create <name>\n");
    fprintf(stderr, "       vmctl shim list\n");
    return 1;
}

static int cmd_vm(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[2], "list") == 0) {
        return run_curl_get("/v1/vms");
    }

    fprintf(stderr, "vmctl: unknown vm command: %s\n", argv[2]);
    fprintf(stderr, "usage: vmctl vm list\n");
    return 1;
}

static int cmd_console(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[2], "list") == 0) {
        return run_curl_get("/v1/consoles");
    }

    fprintf(stderr, "vmctl: unknown console command: %s\n", argv[2]);
    fprintf(stderr, "usage: vmctl console list\n");
    return 1;
}

static void usage(void) {
    printf("vmctl %s\n", VMCTL_VERSION);
    printf("\n");
    printf("usage:\n");
    printf("  vmctl version\n");
    printf("  vmctl ping\n");
    printf("  vmctl profile\n");
    printf("  vmctl profile show [name]\n");
    printf("  vmctl shim create <name>\n");
    printf("  vmctl shim list\n");
    printf("  vmctl vm list\n");
    printf("  vmctl console list\n");
}

static int cmd_profile(int argc, char **argv) {
    const char *profile_name = "default";

    if (argc >= 3 && strcmp(argv[2], "show") == 0) {
        if (argc >= 4)
            profile_name = argv[3];
    }

    Profile p;
    if (!read_profile(profile_name, &p))
        return 1;

    printf("profile  = %s\n", profile_name);
    printf("endpoint = %s\n", p.endpoint[0] ? p.endpoint : "(unset)");
    printf("account  = %s\n", p.account[0] ? p.account : "(unset)");
    printf("region   = %s\n", p.region[0] ? p.region : "(unset)");

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "version") == 0) {
        printf("vmctl %s\n", VMCTL_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "ping") == 0) {
        return cmd_ping();
    }

    if (strcmp(argv[1], "profile") == 0) {
        return cmd_profile(argc, argv);
    }

    if (strcmp(argv[1], "shim") == 0) {
        return cmd_shim(argc, argv);
    }

    if (strcmp(argv[1], "vm") == 0 || strcmp(argv[1], "vms") == 0) {
        return cmd_vm(argc, argv);
    }

    if (strcmp(argv[1], "console") == 0 || strcmp(argv[1], "consoles") == 0) {
        return cmd_console(argc, argv);
    }

    usage();
    return 1;
}
