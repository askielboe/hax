/* SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "agent_loop.h"
#include "agent_status.h"
#include "harness.h"
#include "terminal/program_status.h"

static const struct {
    enum agent_loop_outcome outcome;
    enum program_status status;
} OUTCOMES[] = {
    {AGENT_LOOP_COMPLETE, PROGRAM_STATUS_DONE},
    {AGENT_LOOP_PROVIDER_ERROR, PROGRAM_STATUS_ERROR},
    {AGENT_LOOP_INTERRUPTED, PROGRAM_STATUS_IDLE},
    {AGENT_LOOP_PAUSED, PROGRAM_STATUS_IDLE},
    {AGENT_LOOP_MAX_TURNS, PROGRAM_STATUS_BLOCKED},
};

static void test_loop_outcomes(void)
{
    for (size_t i = 0; i < sizeof(OUTCOMES) / sizeof(OUTCOMES[0]); i++)
        EXPECT(agent_status_after_turn(OUTCOMES[i].outcome, 0, 0) == OUTCOMES[i].status);
}

static void test_frontend_failure_overrides_outcome_except_pause(void)
{
    for (size_t i = 0; i < sizeof(OUTCOMES) / sizeof(OUTCOMES[0]); i++) {
        enum program_status expected =
            OUTCOMES[i].outcome == AGENT_LOOP_PAUSED ? PROGRAM_STATUS_IDLE : PROGRAM_STATUS_ERROR;
        EXPECT(agent_status_after_turn(OUTCOMES[i].outcome, 0, 1) == expected);
    }
}

static void test_cancellation_overrides_outcome_and_failure(void)
{
    for (size_t i = 0; i < sizeof(OUTCOMES) / sizeof(OUTCOMES[0]); i++) {
        EXPECT(agent_status_after_turn(OUTCOMES[i].outcome, 1, 0) == PROGRAM_STATUS_IDLE);
        EXPECT(agent_status_after_turn(OUTCOMES[i].outcome, 1, 1) == PROGRAM_STATUS_IDLE);
    }
}

int main(void)
{
    test_loop_outcomes();
    test_frontend_failure_overrides_outcome_except_pause();
    test_cancellation_overrides_outcome_and_failure();
    T_REPORT();
}
