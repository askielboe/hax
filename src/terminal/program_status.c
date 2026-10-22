/* SPDX-License-Identifier: MIT */
#include "terminal/program_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "config.h"
#include "terminal/ansi.h"

static enum program_status last_status = PROGRAM_STATUS_IDLE;

static int reporting_disabled(void)
{
    const char *value = config_str("program_status");
    return value && strcasecmp(value, "off") == 0;
}

static int terminal_supported(FILE *sink)
{
    if (!isatty(fileno(sink)))
        return 0;
    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0)
        return 0;
    /* Multiplexers may drop reports or let panes overwrite the outer terminal's root record. */
    if (getenv("TMUX") || getenv("STY"))
        return 0;

    const char *term_program = getenv("TERM_PROGRAM");
    return term_program && strcmp(term_program, "rex") == 0;
}

static int emit(FILE *sink, const char *state)
{
    if (fprintf(sink, ANSI_ESC "]7501;state=%s:app=hax" ANSI_BEL, state) < 0)
        return 0;
    return fflush(sink) == 0;
}

static int program_status_active(FILE *sink)
{
    return !reporting_disabled() && terminal_supported(sink);
}

void program_status_refresh(FILE *sink)
{
    if (last_status != PROGRAM_STATUS_IDLE && reporting_disabled() && terminal_supported(sink) &&
        emit(sink, "idle"))
        last_status = PROGRAM_STATUS_IDLE;
}

static const char *status_name(enum program_status status)
{
    switch (status) {
    case PROGRAM_STATUS_IDLE:
        return "idle";
    case PROGRAM_STATUS_WORKING:
        return "working";
    case PROGRAM_STATUS_DONE:
        return "done";
    case PROGRAM_STATUS_BLOCKED:
        return "blocked:kind=question";
    case PROGRAM_STATUS_ERROR:
        return "error";
    }
    return "idle";
}

void program_status_report(FILE *sink, enum program_status status)
{
    if (!program_status_active(sink))
        return;

    if (emit(sink, status_name(status)))
        last_status = status;
}
