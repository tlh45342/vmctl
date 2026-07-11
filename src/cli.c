ChatGPT Plus





















Wednesday 8:51 AM
Wednesday 10:38 AM

vconsole_0_0_3.zip
Zip Archive


vmctl.zip
Zip Archive

Wednesday 1:15 PM
Wednesday 3:42 PM

t32.zip
Zip Archive

Wednesday 6:08 PM

hello.asm
File

Makefile
File


dumbq.md
File

note3.md
File

ok.md
File

part2.md
File

Thursday 5:46 PM
Yesterday 10:47 AM
Yesterday 2:20 PM
Today 7:27 AM

cli.c
File

main.c
File

session.c
File
THis is the CLI from armvm.c  Does this help?

I actually like where this is heading.

The README absolutely should start with something like:

vmctl - Command Line Interface for Foundry

vmctl is the primary command-line client used to communicate with
Foundry servers. It can create, manage, and control virtual machines,
upload programs, manage consoles, and automate Foundry through scripts.

Features
--------
- Interactive shell
- Batch scripting
- VM management
- Console management
- Program loading
- Configuration management
- Future authentication support
I think that establishes its identity immediately.

The set endpoint bug
The error

vmctl> set endpoint http://192.168.150.83:5606
usage: set <name> <value>
tells me the parser is tokenizing incorrectly.

It is almost certainly doing something equivalent to

argc = 4

argv[0] = "set"
argv[1] = "endpoint"
argv[2] = "http"
argv[3] = "//192.168.150.83:5606"
because it is splitting on both whitespace and the : character.

The parser should never split on punctuation. It should split only on whitespace, honoring quoted strings. The colon is part of the URL.

I'd actually like to write the tokenizer so that this becomes legal:

set endpoint http://192.168.150.83:5606
set endpoint https://lab.mycompany.com:5606
set image disk0.img
set hostname VM-Accounting
set prompt "Foundry Lab"
The tokenizer should return:

argc = 3

argv[0] = "set"
argv[1] = "endpoint"
argv[2] = "http://192.168.150.83:5606"
No special treatment for : at all.

The move to .150
I noticed that immediately.

Old:
192.168.160.73

Now:
192.168.150.83
I actually like the move.

It also reinforces why config set endpoint exists—you shouldn't have to rebuild anything because the lab moves from one subnet to another. You just update the configuration.

One architectural change I'd like to make
As we've added commands, I'm becoming less enthusiastic about cli.c knowing how every command works.

Instead, I'd rather see something like:

src/

commands/
    version.c
    ping.c
    config.c
    vm.c
    console.c
    session.c
with a simple dispatcher:

command_table[] =
{
    { "version", cmd_version },
    { "ping",    cmd_ping    },
    { "config",  cmd_config  },
    { "vm",      cmd_vm      },
    { "console", cmd_console },
    ...
};
Then adding a new command is just:

Create one .c file.

Register it in the table.

Done.

vmctl load, vmctl start, vmctl stop, and the future vmctl t32 ... commands all become independent modules instead of growing one large switch statement.

I think that's the direction I'd like to take next. It's a pattern that will scale much better as Foundry grows, and it will make it much easier to add the vm create, vm load, and vm start workflow we've been designing.


The move was a matter of speed for testing....


I agree with your seperation argument above... .it's worthy.

I know we are doing a lot here; but we will never get done unless we start taking bites of the elephant.
Can you throw the "do" script back into contention.  I think it's a simple consumer that just runs a eval if I recall?

Yes. do is exactly that simple consumer.

The shape from the ARM VM was:

static int cmd_do(CLI *cli, int argc, char **argv)
{
    FILE *f;
    CLI script_cli;

    if (argc < 2) {
        log_printf("usage: do <script>\n");
        return -1;
    }

    f = fopen(argv[1], "r");
    if (!f) {
        log_printf("[ERROR] cannot open %s\n", argv[1]);
        return -1;
    }

    cli_init(&script_cli, cli->vm, f, false);
    cli_run(&script_cli);

    fclose(f);
    return 0;
}
For vmctl, we do not need a second session. We want the imported script to share the same variables, current VM, endpoint override, and last status.

So the vmctl version should be closer to:

static int cmd_do(
    vmctl_session_t *session,
    int argc,
    char **argv,
    char *capture,
    size_t capture_size
)
{
    FILE *script;
    int rc;

    (void)capture;
    (void)capture_size;

    if (argc != 2) {
        session_eprintf(session, "usage: do <scriptfile>\n");
        return 1;
    }

    script = fopen(argv[1], "r");
    if (!script) {
        session_eprintf(
            session,
            "vmctl: cannot open script: %s\n",
            argv[1]
        );
        return 1;
    }

    rc = cli_run(session, script, 0);

    fclose(script);
    return rc;
}
Then register it:

{
    "do",
    cmd_do,
    "do <scriptfile>"
},
That gives you:

vmctl> do examples/build.vmctl
and the script runs through the same evaluator as interactive input.

The useful part is that this works:

# build.vmctl

id = vm create
use $id
current
and afterward, back at the prompt:

vmctl> current
vm-12345678
because the script reused the same session.

I would support both:

do build.vmctl
and:

import build.vmctl
but have both call the same underlying function. do is pleasant in the interactive shell; import is more descriptive from the operating-system command line.

One correction on the endpoint bug: http://192.168.150.83:5606 was probably being truncated by the comment stripper, because it interpreted the // in http:// as a comment. The tokenizer itself likely was not splitting on the colon. The comment parser should only recognize // when it begins a command line or follows whitespace—not when it appears inside a token such as a URL.


can you build this?

Worked for 1m 18s
Built and tested.

Download vmctl 0.0.6 with do scripting

This revision includes:

do <scriptfile>
import <scriptfile>
vmctl < scriptfile
Both do and import use the same evaluator and preserve the current session, variables, selected VM, and temporary endpoint.

Example:

vmctl> do examples/session.vmctl
I also fixed two parser bugs:

http://192.168.150.83:5606 is no longer mistaken for a // comment.

The in-place tokenizer was accidentally stopping after the first token, which caused:

usage: set <name> <value>
This now works:

vmctl> set endpoint http://192.168.150.83:5606
http://192.168.150.83:5606
And:

vmctl> vars
endpoint=http://192.168.150.83:5606
The README now begins by identifying vmctl as the command-line tool for working with Foundry, and the examples use the new .150 endpoint.


For the readme....  can we add that this is part of my Foundry project.

And that it can be retrieved by  git clone https://github.com/tlh45342/vmctl.git

I would. In fact, I'd make the README feel like the front door to the project.

Something like this:

# vmctl

`vmctl` is the command-line client for **Foundry**, an experimental virtualization
platform currently under development.

It communicates with one or more Foundry servers to create, configure, and
manage virtual machines, upload programs, control virtual consoles, and
automate operations through scripts.

---

## Part of the Foundry Project

`vmctl` is one component of the Foundry ecosystem.

Current components include:

- **Foundry** – Virtualization management server
- **vmctl** – Command-line management client
- **vconsole** – Remote virtual console service
- **t32-node** – T32 virtual machine node
- **t32-asm** – T32 assembler
- **t32-cc** – T32 C compiler (work in progress)

---

## Repository

Clone the project:

```bash
git clone https://github.com/tlh45342/vmctl.git
```

---

## Building

### Linux

```bash
make
sudo make install
```

### Windows (MinGW)

```bat
make
make install
```

---

## Current Features

- Interactive shell
- Batch scripting
- `do` / `import` script execution
- VM management
- Console management
- Configuration management
- Session variables

---

## Example

```text
vmctl
vmctl> id = vm create
vmctl> use $id
vmctl> current
vmctl> do examples/build.vmctl
```

---

## Status

This project is under active development as part of the Foundry virtualization
platform. Interfaces and commands may evolve as the architecture matures.
One thing I'd add that many people overlook is a family of repositories section. As Foundry grows, someone who lands on vmctl will naturally wonder, "Where is the server?"

I'd add:

## Related Projects

- Foundry Server
  https://github.com/tlh45342/foundry

- vmctl
  https://github.com/tlh45342/vmctl

- vconsole
  https://github.com/tlh45342/vconsole

- t32-node
  https://github.com/tlh45342/t32-node

- t32-asm
  https://github.com/tlh45342/t32-asm

- t32-cc
  https://github.com/tlh45342/t32-cc
As those repositories come online, every project becomes an entry point into the ecosystem. Someone might discover t32-asm first and then realize there's an entire virtualization platform behind it. I think that's a worthwhile long-term structure.


btw, both linux and mac complain along the same line of...


root@nodex:~/vmctl# make
cc -Iinclude -Wall -Wextra -O2 -c src/main.c -o build/main.o
cc -Iinclude -Wall -Wextra -O2 -c src/cli.c -o build/cli.o
cc -Iinclude -Wall -Wextra -O2 -c src/session.c -o build/session.o
cc -Iinclude -Wall -Wextra -O2 -c src/config.c -o build/config.o
cc build/main.o build/cli.o build/session.o build/config.o -o vmctl
/usr/bin/x86_64-linux-gnu-ld.bfd: build/cli.o: in function request':
cli.c:(.text+0xf96): warning: the use of tmpnam' is dangerous, better use mkstemp'

That's actually a good warning to fix now. Both GCC and Clang are telling you the same thing because tmpnam() has been considered unsafe for a long time.

The issue is a classic race condition:

tmpnam() returns a filename.

Your program closes the gap before opening it.

Another process could create that file first.

For a single-user lab it's unlikely, but we should fix it because it's simple.

Linux / macOS
Replace:

char body_path[L_tmpnam];
tmpnam(body_path);

FILE *f = fopen(body_path, "wb");
with:

#include <unistd.h>

char body_path[] = "/tmp/vmctl-body-XXXXXX";

int fd = mkstemp(body_path);
if (fd < 0) {
    perror("mkstemp");
    return 1;
}

FILE *f = fdopen(fd, "wb");
if (!f) {
    close(fd);
    unlink(body_path);
    return 1;
}
When you're finished:

fclose(f);
...
unlink(body_path);
Do the same thing for the response file.

Windows
Windows doesn't have mkstemp(), so I would not try to force one implementation.

Instead, hide it behind one helper.

int make_temp_file(char *path, size_t path_size);
Linux:

mkstemp()
Windows:

GetTempPath()
GetTempFileName()
The rest of vmctl never knows how the temporary file was created.

Or...
Since we're already talking about architecture...

I'd actually move all of this into http.c.

Instead of:

cli
  |
  +-- request()
I'd like:

cli
   |
   +-- http_post_json()
            |
            +-- temp file
            +-- curl
            +-- response
Then all of the ugly platform-specific temporary-file logic lives in one place.

Even better...
I think this is one of those times where we can see the future.

Eventually, I don't think vmctl should shell out to:

curl ...
at all.

I think it should call:

libcurl
directly.

Then there are:

no temporary files,

no system(),

no quoting problems,

no shell injection issues,

easier authentication,

easier TLS,

easier timeouts,

easier progress callbacks.

That's a larger refactor, though. For today, I'd do this in two steps:

Replace tmpnam() with a platform-specific helper (mkstemp / GetTempFileName) so the warning goes away and the code is safer.

Later, when the command set stabilizes, replace the external curl invocation with libcurl and eliminate temporary files entirely.

I think that's the right progression: fix the warning now without biting off a much larger transport rewrite.


You didn't specify the file and approximate line numbers.  Can you simply refactor the file and present it to me?

Worked for 52s
Yes. I refactored the actual file and removed tmpnam() entirely.

vmctl-cli-0.0.7-no-tmpnam.c

Download the full vmctl 0.0.7 source package

The new temporary-file handling uses:

Windows:     GetTempPathA() + GetTempFileNameA()
Linux/macOS: mkstemp()
I rebuilt it on Linux with:

make clean
make
The tmpnam() linker warning is gone.

The README also now states that vmctl is part of your Foundry project and includes:

git clone https://github.com/tlh45342/vmctl.git

and question.... some of the stuff I have been working with I think it's trying to correct me and suggest that local LINUX exe should be ~/.local/bin
Is that a better choice?

Yes—and this is one of those cases where the Linux community is trying to steer you toward modern conventions.

There are really three installation targets:

1. /usr/local/bin (system-wide)
sudo make install
This installs for every user on the machine.

This is appropriate for:

Foundry server

t32-node

vconsole

vmctl on a dedicated lab server

Pros:

Traditional Unix location.

On nearly every system's PATH.

Good for appliances and servers.

Cons:

Requires sudo.

Modifies the whole machine.

2. ~/.local/bin (per-user)
make PREFIX=$HOME/.local install
This installs only for the current user.

Pros:

No sudo.

Doesn't affect other users.

Recommended by the XDG Base Directory Specification.

Fedora, Ubuntu, Debian, and many others already include it in PATH (or prompt you to).

This is where the suggestions you've been seeing come from.

3. Project-local
~/foundry/bin
or

~/bin
This is what a lot of developers use for in-development tools.

For Foundry...
I actually think we should support both.

Your Makefile already does:

PREFIX ?= /usr/local
That's good.

Then users can choose:

make install
or

make PREFIX=$HOME/.local install
without changing the Makefile.

I would make one small improvement
Instead of:

PREFIX ?= /usr/local
I'd do:

PREFIX ?= $(HOME)/.local
only if the target is Linux and the install is clearly a developer tool.

But for vmctl, I'm inclined to leave /usr/local as the default because:

It's a systems-administration tool.

It's likely to be used on Foundry hosts.

sudo make install is a natural workflow there.

Developers who prefer user installs can override PREFIX without any code changes.

One thing I'd like to standardize across your projects
I think every project should support exactly the same install interface:

make
make test
make install
make uninstall
And every Makefile should have:

PREFIX ?= /usr/local
That consistency matters when you have six or seven related repositories.

My recommendation
For the Foundry ecosystem, I'd standardize on:

Linux servers: /usr/local/bin

Developer workstations: ~/.local/bin (by overriding PREFIX)

Windows: C:\Program Files\libvm\bin

That gives you sensible defaults while keeping the installation process flexible and familiar on each platform.


Do you want to make the improvement for the Makefile to install it in ~/.local/bin  
Thanks!

I would, but with one small twist.

I wouldn't change the Windows behavior at all.

Instead, I'd make the Makefile platform-aware.

Something like this:

ifeq ($(OS),Windows_NT)

    CC := gcc
    EXEEXT := .exe

    GNU_BIN ?= C:/Program Files/GNU/bin
    MKDIR_P ?= "$(GNU_BIN)/mkdir.exe" -p
    RM_RF   ?= "$(GNU_BIN)/rm.exe" -rf
    CP      ?= "$(GNU_BIN)/cp.exe" -f

    PREFIX ?= C:/Program Files/libvm

else

    CC ?= cc
    EXEEXT :=

    MKDIR_P ?= mkdir -p
    RM_RF   ?= rm -rf
    CP      ?= cp -f

    #
    # Developer-friendly default.
    # Users can still override:
    #
    #   make PREFIX=/usr/local install
    #
    PREFIX ?= $(HOME)/.local

endif
Then your install target stays exactly the same:

install: $(TARGET)
	@$(MKDIR_P) "$(PREFIX)/bin"
	@$(CP) "$(TARGET)" "$(PREFIX)/bin/$(TARGET)"
Now you get the best of both worlds.

On Windows:

make install
installs to

C:\Program Files\libvm\bin
On Linux/macOS:

make install
installs to

~/.local/bin
If you're setting up a production Foundry server:

sudo make PREFIX=/usr/local install
or simply:

sudo make install PREFIX=/usr/local
installs to

/usr/local/bin
One more tiny improvement
I'd also add this to the README:

## Installing

### Developer Install (recommended)

```bash
make
make install
This installs vmctl into:

~/.local/bin
System-wide Install
sudo make PREFIX=/usr/local install
This installs vmctl into:

/usr/local/bin

I actually like this quite a bit. It matches modern Linux conventions, avoids requiring `sudo` for developers, and still gives administrators a familiar system-wide installation path. I think it's a nice refinement for the project.

I might argue if the same thing might be a better more standard choice even among the windows.
I like it as it supports a standard.  We can let it sit for the second...

BUt I might ask if you can refactor and replace the Makefile with code that supports linux/mac

Here is the replacement Makefile:

Download the refactored vmctl Makefile

Rename it to:

Makefile
It now supports:

Windows
Linux
macOS
Default installs:

Windows: C:/Program Files/libvm/bin
Linux:   ~/.local/bin
macOS:   ~/.local/bin
Typical Linux/macOS use:

make clean
make
make test
make install
System-wide install remains available:

sudo make PREFIX=/usr/local install
I also added:

make show-config
which displays the compiler, platform, target, prefix, and final binary directory before you install. 


Library
/
Foundry
/
vmctl-cli-0.0.7-no-tmpnam.c


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
#define VMCTL_VERSION  "0.0.7"

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
