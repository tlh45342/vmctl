# vmctl 0.0.5

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
set endpoint http://192.168.160.73:5606
```

This changes only the running vmctl process. It does not edit the config file.

## VM scripting

```text
id = vm create
use $id
current
```
