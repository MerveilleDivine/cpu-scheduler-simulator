#!/usr/bin/env python3
"""Check CLI reports against a small, independent per-tick scheduler."""

import collections
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile


ALGORITHMS = {
    "fcfs": "First Come, First Served",
    "sjf": "Shortest Job First - Non-Preemptive",
    "srtf": "Shortest Remaining Time First - Preemptive",
    "priority": "Priority Scheduling - Non-Preemptive",
    "rr": "Round Robin - Quantum {}",
    "none": "No Scheduling",
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def invoke(binary, arguments, stdin="", timeout=5):
    return subprocess.run(
        [binary] + arguments, input=stdin, capture_output=True, text=True,
        timeout=timeout,
    )


def read_json(binary, arguments):
    result = invoke(binary, arguments + ["--format", "json"])
    require(result.returncode == 0, "CLI failed: " + result.stderr)
    return json.loads(result.stdout)


def reference(rows, algorithm, quantum):
    """Use time steps, rather than the production event/heap implementation."""
    remaining = [row[0] for row in rows]
    starts = [None] * len(rows)
    completions = [None] * len(rows)
    timeline = []
    ready = collections.deque()
    running = None
    slice_left = 0
    time = 0

    while any(remaining):
        if algorithm == "rr":
            # New arrivals, including those exactly on a slice boundary, go
            # ahead of the process whose slice just expired.
            for index, row in enumerate(rows):
                if row[1] == time:
                    ready.append(index)
            if running is not None and slice_left == 0:
                ready.append(running)
                running = None
            if running is None and ready:
                running = ready.popleft()
                slice_left = quantum
        elif algorithm == "none":
            candidate = next(i for i, value in enumerate(remaining) if value)
            running = candidate if rows[candidate][1] <= time else None
        elif running is None or algorithm == "srtf":
            candidates = [i for i, value in enumerate(remaining)
                          if value and rows[i][1] <= time]
            if candidates:
                if algorithm == "sjf":
                    key = lambda i: (rows[i][0], rows[i][1], i)
                elif algorithm == "srtf":
                    key = lambda i: (remaining[i], rows[i][1], i)
                elif algorithm == "priority":
                    key = lambda i: (rows[i][2], rows[i][1], i)
                else:
                    key = lambda i: (rows[i][1], i)
                running = min(candidates, key=key)
            else:
                running = None

        process_id = None if running is None else running + 1
        if timeline and timeline[-1]["process_id"] == process_id:
            timeline[-1]["end_time"] = time + 1
        else:
            timeline.append({"process_id": process_id, "start_time": time,
                             "end_time": time + 1})
        if running is not None:
            if starts[running] is None:
                starts[running] = time
            remaining[running] -= 1
            slice_left -= 1
            if remaining[running] == 0:
                completions[running] = time + 1
                running = None
        time += 1

    processes = []
    for i, (burst, arrival, priority) in enumerate(rows):
        turnaround = completions[i] - arrival
        processes.append({
            "id": i + 1, "arrival_time": arrival, "burst_time": burst,
            "priority": priority, "start_time": starts[i],
            "completion_time": completions[i],
            "waiting_time": turnaround - burst,
            "turnaround_time": turnaround,
            "response_time": starts[i] - arrival,
        })
    return processes, timeline


def check_report(report, rows, algorithm, quantum, label):
    expected_processes, expected_timeline = reference(rows, algorithm, quantum)
    expected_name = ALGORITHMS[algorithm].format(quantum)
    require(report["algorithm"] == expected_name, label + ": algorithm name")
    require(report["processes"] == expected_processes,
            label + ": process metrics differ\nexpected: " + repr(expected_processes)
            + "\nreceived: " + repr(report["processes"]))
    require(report["timeline"] == expected_timeline,
            label + ": timeline differs\nexpected: " + repr(expected_timeline)
            + "\nreceived: " + repr(report["timeline"]))
    for field in ("waiting_time", "turnaround_time", "response_time"):
        average = sum(p[field] for p in expected_processes) / len(rows)
        require(math.isclose(report["averages"][field], average, abs_tol=0.005001),
                label + ": wrong average " + field)


def write_rows(path, rows):
    path.write_text("".join("{}:{}:{}\n".format(*row) for row in rows))


def check_reference_workloads(binary, path):
    workloads = [
        [(3, 0, 0)],
        [(8, 0, 2), (4, 1, 1), (2, 2, 3)],  # repeated preemption
        [(4, 0, 0), (2, 2, 0), (1, 2, 0)],  # RR boundary arrivals
        [(3, 7, 0), (3, 1, 0), (3, 1, 0)],  # unsorted input and ties
        [(2, 3, -4), (1, 10, -4)],  # idle gaps and negative priorities
        [(5, 0, 1), (3, 2, 1)],  # equal remaining time keeps earlier arrival
    ]
    random_source = random.Random(20261016)
    for _ in range(100):
        workloads.append([(random_source.randint(1, 9), random_source.randint(0, 12),
                           random_source.randint(-3, 3))
                          for _ in range(random_source.randint(1, 8))])

    checks = 0
    for index, rows in enumerate(workloads):
        quantum = (index % 5) + 1
        write_rows(path, rows)
        reports = {}
        for algorithm in ALGORITHMS:
            args = ["-f", str(path), "--algorithm", algorithm]
            if algorithm == "rr":
                args.extend(["--quantum", str(quantum)])
            reports[algorithm] = read_json(binary, args)
            check_report(reports[algorithm], rows, algorithm, quantum,
                         "workload {} {} q={}".format(index, algorithm, quantum))
            checks += 1
        if index < 6:
            comparison = read_json(binary, ["-f", str(path), "--compare",
                                            "--quantum", str(quantum)])
            require(len(comparison["algorithms"]) == 5, "comparison count")
            by_name = {item["algorithm"]: item for item in comparison["algorithms"]}
            for algorithm in ("fcfs", "sjf", "srtf", "priority", "rr"):
                report = reports[algorithm]
                summary = by_name[report["algorithm"]]
                for metric, value in report["averages"].items():
                    require(summary["average_" + metric] == value,
                            "comparison and single-report averages differ")
    return checks


def check_large_times(binary, path):
    # A per-tick SRTF or single-process RR loop would take billions of steps.
    write_rows(path, [(2_000_000_000, 0, 0)])
    for algorithm in ALGORITHMS:
        args = ["-f", str(path), "--algorithm", algorithm]
        if algorithm == "rr":
            args.extend(["--quantum", "1"])
        report = read_json(binary, args)
        require(report["processes"][0]["completion_time"] == 2_000_000_000,
                algorithm + ": large burst completion")
        require(len(report["timeline"]) == 1, algorithm + ": coalesced large burst")

    # The metric sum can exceed INT_MAX even when every timestamp fits int.
    write_rows(path, [(1_000_000_000, 0, 0)] * 2)
    for algorithm in ("fcfs", "sjf", "srtf", "priority", "rr"):
        report = read_json(binary, ["-f", str(path), "--algorithm", algorithm,
                                   "--quantum", "1000000000"])
        require(report["averages"]["turnaround_time"] == 1_500_000_000,
                algorithm + ": average accumulator overflow")

    for rows in ([(1, 2_147_483_647, 0)],
                 [(2_147_483_647, 0, 0), (1, 0, 0)]):
        write_rows(path, rows)
        for algorithm in ALGORITHMS:
            args = ["-f", str(path), "--algorithm", algorithm,
                    "--quantum", "2147483647"]
            result = invoke(binary, args)
            require(result.returncode != 0 and result.stderr,
                    algorithm + ": rejects timestamp overflow")


def check_cli_errors(binary, path, directory):
    def fails(arguments, label, stdin=""):
        result = invoke(binary, arguments, stdin)
        require(result.returncode != 0, label + ": expected failure")
        require("Error:" in result.stderr, label + ": useful stderr diagnostic")

    invalid_inputs = ["", "# no processes\n", "0:0:0\n", "-1:0:0\n",
                      "1:-1:0\n", "1:0\n", "1:0:0:\n", "1:0:0:4\n",
                      "1:0:\n", "1::0\n", "1x:0:0\n", "1:0:1.0\n",
                      "2147483648:0:0\n", "1:2147483648:0\n",
                      "1:0:2147483648\n", "1:0:-2147483649\n"]
    for content in invalid_inputs:
        path.write_text(content)
        fails(["-f", str(path), "--algorithm", "fcfs"], "input " + repr(content))

    write_rows(path, [(3, 0, 0)])
    for option in ("-f", "--input", "-o", "--output", "-a", "--algorithm",
                   "-q", "--quantum", "--format"):
        fails([option], "missing option value " + option)
    for value in ("two", "2x", "1.5", "2147483648", "-2147483649", "0", "-1"):
        fails(["-f", str(path), "--algorithm", "fcfs", "--quantum", value],
              "bad quantum " + value)
    fails(["-f", str(path), "--algorithm", "unknown"], "unknown algorithm")
    fails(["-f", str(path), "--algorithm", "fcfs", "--format", "yaml"], "bad format")
    fails(["-f", str(path), "--algorithm", "fcfs", "--compare"], "conflicting modes")
    fails(["-f", str(directory / "missing.txt"), "--algorithm", "fcfs"], "missing file")
    fails(["-f", str(path), "--algorithm", "fcfs", "-o", str(directory)], "output directory")
    for stdin in ("", "two\n", "2x\n", "0\n", "-1\n"):
        fails(["-f", str(path), "--algorithm", "rr"], "interactive quantum", stdin)
    fails([], "interactive EOF")
    if os.path.exists("/dev/full"):
        fails(["-f", str(path), "--algorithm", "fcfs", "-o", "/dev/full"],
              "output write failure")
        fails(["-f", str(path), "--compare", "--quantum", "2", "-o", "/dev/full"],
              "comparison write failure")

    for arguments, stdin in ((["-f", str(path)], "1\n"),
                             (["-f", str(path), "--algorithm", "rr"], "2\n")):
        result = invoke(binary, arguments + ["--format", "json"], stdin)
        require(result.returncode == 0, "interactive JSON failed: " + result.stderr)
        require(json.loads(result.stdout)["processes"], "interactive JSON report")
        require(result.stderr, "interactive prompt is separate from JSON stdout")

    # JSON written to a file must match the machine-readable stdout exactly.
    output = directory / "report.json"
    args = ["-f", str(path), "--algorithm", "rr", "--quantum", "2", "-o", str(output)]
    report = read_json(binary, args)
    require(json.loads(output.read_text()) == report, "JSON output file differs")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: validate_json.py BINARY INPUT_FILE")
    binary, sample = sys.argv[1:]
    sample_report = read_json(binary, ["-f", sample, "--algorithm", "srtf"])
    require(sample_report["processes"], "sample input has no processes")
    with tempfile.TemporaryDirectory(prefix="scheduler-tests-") as directory:
        directory = Path(directory)
        path = directory / "input.txt"
        checks = check_reference_workloads(binary, path)
        check_large_times(binary, path)
        check_cli_errors(binary, path, directory)
    print("Validated {} reference schedules, large-time cases, and CLI errors.".format(checks))


if __name__ == "__main__":
    main()
