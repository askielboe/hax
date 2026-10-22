/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "harness.h"
#include "terminal/program_status.h"

static FILE *pty_open(int *master)
{
    *master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    EXPECT(*master >= 0);
    if (*master < 0)
        return NULL;
    int rc = grantpt(*master);
    EXPECT(rc == 0);
    if (rc != 0)
        goto out;
    rc = unlockpt(*master);
    EXPECT(rc == 0);
    if (rc != 0)
        goto out;
    char *path = ptsname(*master);
    EXPECT(path != NULL);
    if (!path)
        goto out;
    FILE *sink = fopen(path, "w");
    EXPECT(sink != NULL);
    if (sink)
        return sink;
out:
    close(*master);
    return NULL;
}

static void pty_expect(int master, const char *expected)
{
    char bytes[256] = {0};
    ssize_t len = read(master, bytes, sizeof(bytes) - 1);
    if (*expected) {
        EXPECT(len > 0);
        EXPECT_STR_EQ(bytes, expected);
    } else {
        EXPECT(len == -1 && errno == EAGAIN);
    }
}

static void expect_report(enum program_status status, const char *expected)
{
    int master;
    FILE *sink = pty_open(&master);
    if (!sink)
        return;
    program_status_report(sink, status);
    pty_expect(master, expected);
    fclose(sink);
    close(master);
}

static void test_states(void)
{
    expect_report(PROGRAM_STATUS_IDLE, "\x1b]7501;state=idle:app=hax\x07");
    expect_report(PROGRAM_STATUS_WORKING, "\x1b]7501;state=working:app=hax\x07");
    expect_report(PROGRAM_STATUS_DONE, "\x1b]7501;state=done:app=hax\x07");
    expect_report(PROGRAM_STATUS_BLOCKED, "\x1b]7501;state=blocked:kind=question:app=hax\x07");
    expect_report(PROGRAM_STATUS_ERROR, "\x1b]7501;state=error:app=hax\x07");
}

static void test_auto_requires_rex(void)
{
    unsetenv("TERM_PROGRAM");
    expect_report(PROGRAM_STATUS_WORKING, "");

    static const char *const UNSUPPORTED[] = {"",        "Apple_Terminal", "iTerm.app", "kitty",
                                              "ghostty", "WezTerm",        "rex-other", "Rex"};
    for (size_t i = 0; i < sizeof(UNSUPPORTED) / sizeof(UNSUPPORTED[0]); i++) {
        setenv("TERM_PROGRAM", UNSUPPORTED[i], 1);
        expect_report(PROGRAM_STATUS_WORKING, "");
    }

    setenv("TERM_PROGRAM", "rex", 1);
    expect_report(PROGRAM_STATUS_WORKING, "\x1b]7501;state=working:app=hax\x07");
}

static void test_off_setting_is_silent(void)
{
    EXPECT(config_load("{\"program_status\": \"off\"}") == 0);
    expect_report(PROGRAM_STATUS_WORKING, "");
    expect_report(PROGRAM_STATUS_DONE, "");
    expect_report(PROGRAM_STATUS_IDLE, "");

    setenv("HAX_PROGRAM_STATUS", "auto", 1);
    expect_report(PROGRAM_STATUS_DONE, "\x1b]7501;state=done:app=hax\x07");
    setenv("HAX_PROGRAM_STATUS", "OFF", 1);
    expect_report(PROGRAM_STATUS_ERROR, "");
    unsetenv("HAX_PROGRAM_STATUS");

    config_set_override("program_status", "auto");
    expect_report(PROGRAM_STATUS_DONE, "\x1b]7501;state=done:app=hax\x07");
    config_set_override("program_status", "off");
    expect_report(PROGRAM_STATUS_DONE, "");
    expect_report(PROGRAM_STATUS_IDLE, "");
    config_free();
}

static void test_refresh_retires_state_when_disabled(void)
{
    int master;
    FILE *sink = pty_open(&master);
    if (!sink)
        return;
    EXPECT(config_load("{\"program_status\": \"off\"}") == 0);
    program_status_report(sink, PROGRAM_STATUS_WORKING);
    program_status_refresh(sink);
    pty_expect(master, "");
    program_status_refresh(sink);
    pty_expect(master, "");
    config_free();
    program_status_refresh(sink);
    pty_expect(master, "");
    config_set_override("program_status", "off");
    program_status_refresh(sink);
    pty_expect(master, "");
    config_set_override("program_status", "auto");
    program_status_report(sink, PROGRAM_STATUS_IDLE);
    pty_expect(master, "\x1b]7501;state=idle:app=hax\x07");
    config_set_override("program_status", "off");
    program_status_refresh(sink);
    pty_expect(master, "");
    config_set_override("program_status", "auto");
    program_status_report(sink, PROGRAM_STATUS_DONE);
    pty_expect(master, "\x1b]7501;state=done:app=hax\x07");
    program_status_report(sink, PROGRAM_STATUS_IDLE);
    pty_expect(master, "\x1b]7501;state=idle:app=hax\x07");
    config_set_override("program_status", "off");
    program_status_refresh(sink);
    pty_expect(master, "");
    config_set_override("program_status", "auto");
    program_status_report(sink, PROGRAM_STATUS_DONE);
    pty_expect(master, "\x1b]7501;state=done:app=hax\x07");
    config_set_override("program_status", "off");
    program_status_refresh(sink);
    pty_expect(master, "\x1b]7501;state=idle:app=hax\x07");
    program_status_report(sink, PROGRAM_STATUS_ERROR);
    program_status_refresh(sink);
    pty_expect(master, "");
    config_set_override("program_status", "auto");
    program_status_report(sink, PROGRAM_STATUS_WORKING);
    pty_expect(master, "\x1b]7501;state=working:app=hax\x07");
    config_set_override("program_status", "off");
    setenv("TMUX", "/tmp/tmux-test,1,0", 1);
    program_status_refresh(sink);
    pty_expect(master, "");
    unsetenv("TMUX");
    setenv("STY", "123.test", 1);
    program_status_refresh(sink);
    pty_expect(master, "");
    unsetenv("STY");
    setenv("TERM", "dumb", 1);
    program_status_refresh(sink);
    pty_expect(master, "");
    setenv("TERM", "xterm-256color", 1);
    setenv("TERM_PROGRAM", "kitty", 1);
    program_status_refresh(sink);
    pty_expect(master, "");
    setenv("TERM_PROGRAM", "rex", 1);
    program_status_refresh(sink);
    pty_expect(master, "\x1b]7501;state=idle:app=hax\x07");
    program_status_refresh(sink);
    pty_expect(master, "");
    fclose(sink);
    close(master);
    config_free();
}

static void test_tmux_is_silent(void)
{
    setenv("TMUX", "/tmp/tmux-test,1,0", 1);
    setenv("TMUX_PANE", "%1", 1);
    static const enum program_status STATES[] = {
        PROGRAM_STATUS_IDLE,    PROGRAM_STATUS_WORKING, PROGRAM_STATUS_DONE,
        PROGRAM_STATUS_BLOCKED, PROGRAM_STATUS_ERROR,
    };
    for (size_t i = 0; i < sizeof(STATES) / sizeof(STATES[0]); i++)
        expect_report(STATES[i], "");
    unsetenv("TMUX_PANE");
    unsetenv("TMUX");
}

static void test_screen_is_silent(void)
{
    setenv("STY", "123.test", 1);
    expect_report(PROGRAM_STATUS_WORKING, "");
    unsetenv("STY");
}

static void test_dumb_terminal_is_silent(void)
{
    setenv("TERM", "dumb", 1);
    expect_report(PROGRAM_STATUS_WORKING, "");
    setenv("TERM", "xterm-256color", 1);
}

static void test_redirected_output_is_silent(void)
{
    FILE *sink = tmpfile();
    EXPECT(sink != NULL);
    if (!sink)
        return;
    program_status_report(sink, PROGRAM_STATUS_WORKING);
    config_set_override("program_status", "off");
    program_status_refresh(sink);
    EXPECT(ftell(sink) == 0);
    config_free();
    fclose(sink);
}

int main(void)
{
    unsetenv("STY");
    unsetenv("TMUX");
    setenv("TERM", "xterm-256color", 1);
    setenv("TERM_PROGRAM", "rex", 1);
    test_refresh_retires_state_when_disabled();
    test_states();
    test_auto_requires_rex();
    test_off_setting_is_silent();
    test_tmux_is_silent();
    test_screen_is_silent();
    test_dumb_terminal_is_silent();
    test_redirected_output_is_silent();
    T_REPORT();
}
