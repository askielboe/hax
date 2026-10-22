/* SPDX-License-Identifier: MIT */
#ifndef HAX_AGENT_STATUS_H
#define HAX_AGENT_STATUS_H

#include "agent_loop.h"
#include "terminal/program_status.h"

/* Pure post-turn presentation policy. cancelled includes requests latched after the loop;
 * failed includes frontend failures and outcomes the frontend treats as fatal. A user cancel or
 * clean pause takes precedence over failure. */
enum program_status agent_status_after_turn(enum agent_loop_outcome outcome, int cancelled,
                                            int failed);

#endif /* HAX_AGENT_STATUS_H */
