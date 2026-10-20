#!/usr/bin/env python3
"""Render scheduler JSON as a comparison SVG using Python's standard library."""

import argparse
import html
import json
from pathlib import Path
import subprocess

ALGORITHMS = ("fcfs", "sjf", "srtf", "priority", "rr")
LABELS = ("FCFS", "SJF", "SRTF", "Priority", "Round Robin")
COLORS = ("#d6e8ff", "#e3d8ff", "#bcebd7", "#ffdda8", "#f9cbd8")


def render(reports, quantum, source_name):
    finish = max(s["end_time"] for r in reports for s in r["timeline"])
    pieces = ['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="800" '
              'viewBox="0 0 1280 800" role="img" aria-labelledby="title description">',
              '<title id="title">CPU scheduling on one workload</title>',
              '<desc id="description">Execution timelines and average process times from '
              'the simulator. Colors identify processes; dark intervals are idle.</desc>',
              '<rect width="1280" height="800" fill="#101114" rx="24"/>',
              '<g font-family="Arial, sans-serif" fill="#f4f4f5">']

    def text(x, y, value, size=14, color="#f4f4f5", anchor="start"):
        pieces.append('<text x="{}" y="{}" font-size="{}" fill="{}" '
                      'text-anchor="{}">{}</text>'.format(x, y, size, color, anchor, html.escape(str(value))))

    text(54, 66, "C++ / OPERATING SYSTEMS", 15, "#a1a1aa")
    text(54, 116, "One workload. Five scheduling policies.", 34)
    text(54, 155, "Computed by the simulator. Round Robin quantum: {}".format(quantum), 17, "#b8bbc3")
    text(54, 210, "ALGORITHM", 13, "#b8bbc3")
    text(170, 210, "EXECUTION TIMELINE", 13, "#b8bbc3")
    metrics = ((930, "waiting_time", "AVG WAIT"), (1045, "turnaround_time", "AVG TURNAROUND"),
               (1170, "response_time", "AVG RESPONSE"))
    for x, _, heading in metrics:
        text(x, 195, heading, 13, "#b8bbc3", "middle")
    for index, (label, report) in enumerate(zip(LABELS, reports)):
        y = 230 + index * 86
        text(54, y + 27, label, 17)
        for segment in report["timeline"]:
            process_id = segment["process_id"]
            start, end = segment["start_time"], segment["end_time"]
            x = 170 + start / finish * 680
            width = (end - start) / finish * 680
            color = "#34363d" if process_id is None else COLORS[(process_id - 1) % len(COLORS)]
            label = "Idle" if process_id is None else "P{}".format(process_id)
            pieces.append('<rect x="{:.3f}" y="{}" width="{:.3f}" height="42" '
                          'fill="{}" stroke="#101114" stroke-width="2">'
                          '<title>{}: {} to {}</title></rect>'.format(x, y, width, color, label, start, end))
            if width >= 29:
                text(round(x + width / 2, 3), y + 26, label, 13,
                     "#e4e4e7" if process_id is None else "#15171c", "middle")
        text(170, y + 62, 0, 12, "#a1a1aa")
        text(850, y + 62, finish, 12, "#a1a1aa", "end")
        for x, metric, _ in metrics:
            text(x, y + 27, "{:.2f}".format(report["averages"][metric]), 20, anchor="middle")
    process_ids = sorted(p["id"] for p in reports[0]["processes"])
    for index, process_id in enumerate(process_ids[:10]):
        x = 54 + index * 91
        pieces.append('<rect x="{}" y="695" width="15" height="15" rx="3" fill="{}"/>'.format(
            x, COLORS[(process_id - 1) % len(COLORS)]))
        text(x + 24, 708, "P{}".format(process_id))
    text(54, 752, "Source: {} · All times use the input unit. Context-switch cost: 0.".format(source_name),
         14, "#a1a1aa")
    return "\n".join(pieces + ['</g></svg>\n'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="Path to the compiled scheduler")
    parser.add_argument("input", type=Path, help="Process input file")
    parser.add_argument("--quantum", type=int, default=2)
    parser.add_argument("--output", type=Path, default=Path("examples/sample_timelines.svg"))
    args = parser.parse_args()
    if args.quantum <= 0:
        parser.error("--quantum must be positive")
    reports = []
    for algorithm in ALGORITHMS:
        command = [str(args.binary.resolve()), "-f", str(args.input), "--algorithm", algorithm,
                   "--quantum", str(args.quantum), "--format", "json"]
        try:
            result = subprocess.run(command, capture_output=True, text=True, check=True, timeout=30)
            reports.append(json.loads(result.stdout))
        except (OSError, subprocess.SubprocessError, ValueError) as error:
            parser.exit(1, "Could not read {} report: {}\n".format(algorithm, error))
    args.output.write_text(render(reports, args.quantum, args.input.name), encoding="utf-8")
    print("Wrote {}".format(args.output))


if __name__ == "__main__":
    main()
