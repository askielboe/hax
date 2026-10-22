/* SPDX-License-Identifier: MIT */
#include "agent_status.h"

#include "agent_loop.h"
#include "terminal/program_status.h"

enum program_status agent_status_after_turn(enum agent_loop_outcome outcome, int cancelled,
                                            int failed)
{
    if (cancelled || outcome == AGENT_LOOP_PAUSED)
        return PROGRAM_STATUS_IDLE;
    if (failed)
        return PROGRAM_STATUS_ERROR;

    switch (outcome) {
    case AGENT_LOOP_COMPLETE:
        return PROGRAM_STATUS_DONE;
    case AGENT_LOOP_PROVIDER_ERROR:
        return PROGRAM_STATUS_ERROR;
    case AGENT_LOOP_MAX_TURNS:
        return PROGRAM_STATUS_BLOCKED;
    case AGENT_LOOP_INTERRUPTED:
    case AGENT_LOOP_PAUSED:
        return PROGRAM_STATUS_IDLE;
    }
    return PROGRAM_STATUS_IDLE;
}
