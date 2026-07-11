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
