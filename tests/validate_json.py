#!/usr/bin/env python3
import json
import subprocess
import sys


def run_json(command):
    completed = subprocess.run(command, check=True, capture_output=True, text=True)
    return json.loads(completed.stdout)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: validate_json.py BINARY INPUT_FILE")

    binary, input_file = sys.argv[1:]
    report = run_json(
        [binary, "-f", input_file, "--algorithm", "srtf", "--format", "json"]
    )
    assert report["algorithm"] == "Shortest Remaining Time First - Preemptive"
    assert len(report["processes"]) == 5
    assert report["timeline"]
    assert {"waiting_time", "turnaround_time", "response_time"} <= set(report["averages"])

    comparison = run_json(
        [binary, "-f", input_file, "--compare", "--quantum", "2", "--format", "json"]
    )
    names = {item["algorithm"] for item in comparison["algorithms"]}
    assert "Shortest Remaining Time First - Preemptive" in names
    assert len(names) == 5


if __name__ == "__main__":
    main()
