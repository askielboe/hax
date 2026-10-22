#!/usr/bin/env python3
"""OSC 7501 follows user turns, not individual provider calls or prompt redraws."""

import json
import os
import re

import harness


REPORT = re.compile(rb"\x1b\]7501;state=([^\x07]+)\x07")


def reports(path):
    return [match.decode() for match in REPORT.findall(path.read_bytes())] if path.exists() else []


def expect_reports(term, path, expected):
    term.wait_until(lambda: reports(path) == expected)


def recording_repl(script):
    term = harness.Terminal(
        script,
        extra_env={"HAX_NOTIFY": "off"},
        unset_env=("TMUX", "TMUX_PANE", "STY"),
        pane_env={"TERM_PROGRAM": "rex"},
    )
    term.wait_for("❯")
    path = term.workdir / "output"
    term.record_output(path)
    return term, path


def test_quit_reports_idle():
    term, path = recording_repl("text Unused.\nend-turn\n")
    term.send("C-d")
    harness.expect(term.wait_exit() == 0, "REPL exits cleanly")
    harness.expect(reports(path) == ["idle:app=hax"], "quitting reports idle, never clear")


def test_complete_stays_done_at_the_prompt():
    term, path = recording_repl(
        """
        tool bash {"command":"cat fifo"}
        end-turn
        text Done.
        end-turn
        text Compacted context.
        end-turn
        """
    )
    os.mkfifo(term.workdir / "fifo")
    term.type("go")
    term.send("Enter")
    term.wait_for("[bash] cat fifo")
    expect_reports(term, path, ["working:app=hax"])
    term.write_fifo(term.workdir / "fifo", "ready\n")
    expect_reports(term, path, ["working:app=hax", "done:app=hax"])
    term.type("/help")
    term.send("Enter")
    term.wait_for("ctrl-d        quit")
    harness.expect(reports(path) == ["working:app=hax", "done:app=hax"], "done survives redraws")
    term.type("/compact")
    term.send("Enter")
    expect_reports(
        term, path, ["working:app=hax", "done:app=hax", "working:app=hax", "idle:app=hax"]
    )
    term.send("C-d")
    harness.expect(term.wait_exit() == 0, "REPL exits cleanly")
    harness.expect(
        reports(path)
        == ["working:app=hax", "done:app=hax", "working:app=hax", "idle:app=hax", "idle:app=hax"],
        "quitting reports idle, never clear",
    )


def test_manual_compaction_without_summary_reports_idle():
    term, path = recording_repl("text Done.\nend-turn\nend-turn\n")
    term.type("go")
    term.send("Enter")
    expect_reports(term, path, ["working:app=hax", "done:app=hax"])
    term.type("/compact")
    term.send("Enter")
    term.wait_for("compaction produced no summary")
    expect_reports(
        term, path, ["working:app=hax", "done:app=hax", "working:app=hax", "idle:app=hax"]
    )


def test_provider_error():
    missing = harness.scratch_dir() / "missing-script"
    term = harness.Terminal(
        "",
        extra_env={"HAX_MOCK_SCRIPT": str(missing), "HAX_NOTIFY": "off"},
        unset_env=("TMUX", "TMUX_PANE", "STY"),
        pane_env={"TERM_PROGRAM": "rex"},
    )
    term.wait_for("❯")
    path = term.workdir / "output"
    term.record_output(path)
    term.type("go")
    term.send("Enter")
    expect_reports(term, path, ["working:app=hax", "error:app=hax"])


def blocked_repl():
    term = harness.Terminal(
        """
        tool bash {"command":"true"}
        end-turn
        text Continued.
        end-turn
        """,
        extra_env={
            "HAX_MAX_TURNS": "1",
            "HAX_NOTIFY": "off",
            "HAX_NO_SESSION": "1",
        },
        unset_env=("TMUX", "TMUX_PANE", "STY"),
        pane_env={"TERM_PROGRAM": "rex"},
    )
    term.wait_for("❯")
    path = term.workdir / "output"
    term.record_output(path)
    term.type("go")
    term.send("Enter")
    expect_reports(term, path, ["working:app=hax", "blocked:kind=question:app=hax"])
    return term, path


def test_disabling_reports_idle():
    term, path = blocked_repl()
    expected = [*reports(path), "idle:app=hax"]
    term.type("/config program_status off")
    term.send("Enter")
    expect_reports(term, path, expected)
    term.send("Enter")
    term.wait_for("Continued.")
    term.send("C-d")
    harness.expect(term.wait_exit() == 0, "REPL exits cleanly")
    harness.expect(reports(path) == expected, "disabled turns and exit emit no more reports")


def test_enabling_reports_after_disabled_refresh():
    term = harness.Terminal(
        "text Done.\nend-turn\n",
        extra_env={"HAX_PROGRAM_STATUS": "off", "HAX_NOTIFY": "off"},
        unset_env=("TMUX", "TMUX_PANE", "STY"),
        pane_env={"TERM_PROGRAM": "rex"},
    )
    term.wait_for("❯")
    path = term.workdir / "output"
    term.record_output(path)
    term.type("/config program_status off")
    term.send("Enter")
    term.wait_for("program_status = off")
    expect_reports(term, path, [])
    term.type("/config program_status auto")
    term.send("Enter")
    term.wait_for("program_status = auto")
    term.type("go")
    term.send("Enter")
    expect_reports(term, path, ["working:app=hax", "done:app=hax"])


def test_turn_limit_blocks_until_continued():
    term, path = blocked_repl()
    expected = reports(path)
    term.send("Enter")
    expect_reports(term, path, [*expected, "working:app=hax", "done:app=hax"])


def test_new_clears_blocked_status():
    term, path = blocked_repl()
    expected = reports(path)
    term.type("/new")
    term.send("Enter")
    expect_reports(term, path, [*expected, "idle:app=hax"])


def test_undo_clears_blocked_status():
    term, path = blocked_repl()
    expected = reports(path)
    term.type("/undo 1")
    term.send("Enter")
    expect_reports(term, path, [*expected, "idle:app=hax"])


def test_failed_fork_keeps_blocked_status():
    term, path = blocked_repl()
    expected = reports(path)
    term.type("/fork 1")
    term.send("Enter")
    term.wait_for("/fork needs session recording")
    expect_reports(term, path, expected)


def test_cancel_reports_idle():
    term, path = recording_repl("text Waiting.\ndelay 30000\ntext Never delivered.\nend-turn\n")
    term.type("go")
    term.send("Enter")
    term.wait_for("Waiting.")
    term.send("Escape", "Escape")
    expect_reports(term, path, ["working:app=hax", "idle:app=hax"])


def test_oneshot_reports_on_stderr_without_touching_json_stdout():
    cases = [
        ("text Done.", {}, 0, "complete", "done"),
        ('tool bash {"command":"true"}', {"HAX_MAX_TURNS": "1"}, 1, "max_turns", "error"),
    ]
    for directive, env, exit_code, outcome, status in cases:
        home, workdir = harness.make_home()
        stdout = workdir / "stdout.jsonl"
        term = harness.Terminal(
            f"delay 1000\n{directive}\nend-turn\n",
            args=["-p", "--json", "go"],
            home=home,
            workdir=workdir,
            extra_env=env,
            stdout_path=stdout,
            unset_env=("TMUX", "TMUX_PANE", "STY"),
            pane_env={"TERM_PROGRAM": "rex"},
        )
        path = workdir / "output"
        term.record_output(path)
        harness.expect(term.wait_exit() == exit_code, "one-shot returns the expected exit code")
        harness.expect(reports(path)[-1:] == [f"{status}:app=hax"], "stderr reports the outcome")
        records = [json.loads(line) for line in stdout.read_text().splitlines()]
        harness.expect(records[-1]["outcome"] == outcome, "stdout remains valid JSONL")


if __name__ == "__main__":
    harness.main(globals())
