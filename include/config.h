#ifndef VMCTL_CONFIG_H
#define VMCTL_CONFIG_H

#include "session.h"

int config_load_default(vmctl_session_t *session);
int config_show(vmctl_session_t *session);
int config_set_value(
    vmctl_session_t *session,
    const char *key,
    const char *value
);

#endif
