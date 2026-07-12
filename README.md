# vmctl 0.0.8 x

`vmctl` is the command-line tool for the **Foundry** project. It communicates
with a Foundry server to manage virtual-machine objects, consoles, configuration,
and automation scripts.

Clone the repository:

```bash
git clone https://github.com/tlh45342/vmctl.git
```

## Persistent configuration

```text
vmctl config show
vmctl config set endpoint http://192.168.150.83:5606
vmctl config set account tom
vmctl config set region local
```

These commands update `~/.foundry/config`.

## Session-only values

Inside interactive or script mode:

```text
set endpoint http://192.168.150.83:5606
```

This changes only the running `vmctl` process. It does not edit the config
file. URLs containing `http://` or `https://` are preserved correctly.

## Script execution

Both forms use the same evaluator and share the current session:

```text
do examples/build.vmctl
import examples/build.vmctl
```

From the operating-system shell, stdin is also supported:

```text
vmctl < examples/build.vmctl
```

## VM scripting

```text
id = vm create
use $id
current
```

## Building and installing

Linux and macOS default to a per-user installation:

```bash
make clean
make
make test
make install
```

This installs `vmctl` to:

```text
~/.local/bin/vmctl
```

For a system-wide installation:

```bash
sudo make PREFIX=/usr/local install
```

Windows continues to use the configured Windows prefix in the Makefile.

## Parser behavior verified in 0.0.8

The interactive parser accepts multiple arguments and URLs:

```text
vmctl> vm list
vmctl> set endpoint http://192.168.150.83:5606
```

`http://` and `https://` are not treated as comments.

export PATH="$HOME/.local/bin:$PATH"
echo $PATH
