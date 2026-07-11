#ifndef VMCTL_SESSION_H
#define VMCTL_SESSION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
typedef struct { char *key; char *value; } vmctl_var_t;
typedef struct { vmctl_var_t *items; size_t count, capacity; } vmctl_vars_t;
typedef struct {
    char endpoint[256];
    char account[128];
    char region[128];
    char current_vm[128];
    vmctl_vars_t vars;
    int last_status;
} vmctl_session_t;
void session_init(vmctl_session_t *s);
void session_shutdown(vmctl_session_t *s);
bool session_set(vmctl_session_t *s,const char *k,const char *v);
const char *session_get(vmctl_session_t *s,const char *k);
bool session_unset(vmctl_session_t *s,const char *k);
void session_dump(vmctl_session_t *s);
#endif
