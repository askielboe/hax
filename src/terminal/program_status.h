/* SPDX-License-Identifier: MIT */
#ifndef HAX_TERMINAL_PROGRAM_STATUS_H
#define HAX_TERMINAL_PROGRAM_STATUS_H

#include <stdio.h>

enum program_status {
    PROGRAM_STATUS_IDLE,
    PROGRAM_STATUS_WORKING,
    PROGRAM_STATUS_DONE,
    PROGRAM_STATUS_BLOCKED,
    PROGRAM_STATUS_ERROR,
};

/* When disabled, retire this process's last non-idle report once on a supported terminal. */
void program_status_refresh(FILE *sink);

/* Report hax's root state via OSC 7501 and flush it when active. Non-TTY sinks, TERM=dumb,
 * tmux, GNU screen, and program_status=off are silent. BLOCKED asks the user to continue
 * at a turn limit. */
void program_status_report(FILE *sink, enum program_status status);

#endif /* HAX_TERMINAL_PROGRAM_STATUS_H */
