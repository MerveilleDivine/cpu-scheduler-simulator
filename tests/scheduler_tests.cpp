#include "Parser.h"
#include "Reporter.h"
#include "Scheduler.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void expectEqual(int actual, int expected, const std::string& message) {
    std::ostringstream description;
    description << message << " (expected " << expected << ", received " << actual << ')';
    expect(actual == expected, description.str());
}

Process makeProcess(int id, int burst, int arrival, int priority = 0) {
    Process process;
    process.id = id;
    process.burstTime = burst;
    process.remainingTime = burst;
    process.arrivalTime = arrival;
    process.priority = priority;
    return process;
}

ScheduleResult roundRobin(const std::vector<Process>& processes) {
    return runRoundRobin(processes, 2);
}

typedef ScheduleResult (*Scheduler)(const std::vector<Process>&);
const Scheduler schedulers[] = {
    runFirstComeFirstServed, runShortestJobFirst, runShortestRemainingTimeFirst,
    runPriorityScheduling, roundRobin, runNoScheduling
};
const char* schedulerNames[] = {"FCFS", "SJF", "SRTF", "priority", "RR", "input order"};
const std::size_t schedulerCount = sizeof(schedulers) / sizeof(schedulers[0]);

void expectTimeline(const ScheduleResult& result, const std::vector<GanttSegment>& expected,
                    const std::string& label) {
    expect(result.ganttChart.size() == expected.size(), label + " segment count");
    const std::size_t count = std::min(result.ganttChart.size(), expected.size());
    for (std::size_t i = 0; i < count; ++i) {
        expectEqual(result.ganttChart[i].processId, expected[i].processId, label + " process");
        expectEqual(result.ganttChart[i].startTime, expected[i].startTime, label + " start");
        expectEqual(result.ganttChart[i].endTime, expected[i].endTime, label + " end");
    }
}

void testPreemptionAndMetrics() {
    const std::vector<Process> processes = {
        makeProcess(1, 8, 0), makeProcess(2, 4, 1), makeProcess(3, 2, 2)
    };
    const ScheduleResult result = runShortestRemainingTimeFirst(processes);
    expectTimeline(result, {GanttSegment(1, 0, 1), GanttSegment(2, 1, 2),
                           GanttSegment(3, 2, 4), GanttSegment(2, 4, 7),
                           GanttSegment(1, 7, 14)}, "SRTF preemption");
    expectEqual(result.processes[0].completionTime, 14, "P1 completion");
    expectEqual(result.processes[0].waitingTime, 6, "P1 waiting time");
    expectEqual(result.processes[1].completionTime, 7, "P2 completion");
    expectEqual(result.processes[1].waitingTime, 2, "P2 waiting time");
    expectEqual(result.processes[2].completionTime, 4, "P3 completion");
    expectEqual(result.processes[2].responseTime, 0, "P3 response time");

    const ScheduleResult equalRemaining = runShortestRemainingTimeFirst(
        {makeProcess(1, 6, 0), makeProcess(2, 3, 3)});
    expectTimeline(equalRemaining, {GanttSegment(1, 0, 6), GanttSegment(2, 6, 9)},
                   "equal remaining time keeps the earlier arrival");
}

void testIdleGapsAndTies() {
    for (std::size_t i = 0; i < schedulerCount; ++i) {
        const ScheduleResult idle = schedulers[i]({makeProcess(1, 2, 3), makeProcess(2, 1, 10)});
        expectTimeline(idle, {GanttSegment(0, 0, 3), GanttSegment(1, 3, 5),
                             GanttSegment(0, 5, 10), GanttSegment(2, 10, 11)},
                       schedulerNames[i]);
        expectEqual(idle.processes[0].responseTime, 0, "idle time before arrival is not response time");

        const ScheduleResult tied = schedulers[i]({makeProcess(9, 2, 0, -3), makeProcess(2, 2, 0, -3)});
        const int firstId = i == schedulerCount - 1 ? 9 : 2;
        expectEqual(tied.ganttChart.front().processId, firstId, "ties use process ID (input order excepted)");
        expectEqual(tied.processes.front().id, 2, "results sorted by ID independently of execution order");
    }
}

void testNonPreemptiveSelection() {
    const std::vector<Process> processes = {
        makeProcess(1, 5, 0, 5), makeProcess(2, 1, 1, -5), makeProcess(3, 2, 1, 0)
    };
    const std::vector<GanttSegment> expected = {
        GanttSegment(1, 0, 5), GanttSegment(2, 5, 6), GanttSegment(3, 6, 8)
    };
    expectTimeline(runShortestJobFirst(processes), expected, "SJF is non-preemptive");
    expectTimeline(runPriorityScheduling(processes), expected, "priority is non-preemptive");
    const std::vector<Process> unsorted = {makeProcess(9, 2, 5), makeProcess(2, 1, 0)};
    expectTimeline(runNoScheduling(unsorted), {GanttSegment(0, 0, 5), GanttSegment(9, 5, 7),
                                             GanttSegment(2, 7, 8)}, "input order waits for first input");
    expectTimeline(runFirstComeFirstServed(unsorted), {GanttSegment(2, 0, 1), GanttSegment(0, 1, 5),
                                                     GanttSegment(9, 5, 7)}, "FCFS orders arrivals");
}

void testRoundRobinBoundary() {
    const ScheduleResult result = runRoundRobin(
        {makeProcess(20, 4, 0), makeProcess(30, 1, 2), makeProcess(10, 1, 2)}, 2);
    expectTimeline(result, {GanttSegment(20, 0, 2), GanttSegment(10, 2, 3),
                           GanttSegment(30, 3, 4), GanttSegment(20, 4, 6)},
                   "RR enqueues boundary arrivals before the expired process");
    for (int quantum = -1; quantum <= 0; ++quantum) {
        bool rejected = false;
        try {
            runRoundRobin({makeProcess(1, 3, 0)}, quantum);
        } catch (const std::exception&) {
            rejected = true;
        }
        expect(rejected, "RR rejects a non-positive quantum");
    }
}

void testFreshSimulationState() {
    Process process = makeProcess(7, 3, 2);
    process.remainingTime = 0;
    process.startTime = 100;
    process.completionTime = 200;
    process.waitingTime = 300;
    process.turnaroundTime = 400;
    process.responseTime = 500;
    for (std::size_t i = 0; i < schedulerCount; ++i) {
        const ScheduleResult result = schedulers[i]({process});
        const Process& completed = result.processes.front();
        expectEqual(completed.startTime, 2, "fresh start ignores stale computed state");
        expectEqual(completed.completionTime, 5, "fresh completion ignores stale computed state");
        expectEqual(completed.waitingTime, 0, "fresh waiting time");
        expectEqual(completed.turnaroundTime, 3, "fresh turnaround time");
        expectEqual(completed.responseTime, 0, "fresh response time");
        expectEqual(completed.remainingTime, 0, "completed remaining time");
        expectTimeline(schedulers[i](result.processes), result.ganttChart, "rerun a completed result");
    }
    expectEqual(process.startTime, 100, "input start time is not mutated");
    expectEqual(process.remainingTime, 0, "input remaining time is not mutated");
}

void testApiValidationAndTimeLimits() {
    const std::vector<std::vector<Process> > invalid = {
        {}, {makeProcess(1, 0, 0)}, {makeProcess(1, -1, 0)},
        {makeProcess(1, 1, -1)}, {makeProcess(0, 1, 0)}, {makeProcess(-1, 1, 0)},
        {makeProcess(1, 1, 0), makeProcess(1, 2, 1)}
    };
    const int maxTime = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < schedulerCount; ++i) {
        for (std::size_t input = 0; input < invalid.size(); ++input) {
            bool rejected = false;
            try {
                schedulers[i](invalid[input]);
            } catch (const std::exception&) {
                rejected = true;
            }
            expect(rejected, std::string(schedulerNames[i]) + " rejects invalid API input");
        }
        bool rejected = false;
        try {
            schedulers[i]({makeProcess(1, 1, maxTime)});
        } catch (const std::exception&) {
            rejected = true;
        }
        expect(rejected, std::string(schedulerNames[i]) + " rejects completion time overflow");
        const ScheduleResult boundary = schedulers[i]({makeProcess(1, 1, maxTime - 1)});
        expectEqual(boundary.processes.front().completionTime, maxTime, "INT_MAX completion is valid");
    }
}

class TemporaryInput {
public:
    explicit TemporaryInput(const std::string& contents) : path_("scheduler_unit_test_input.txt") {
        std::ofstream output(path_.c_str());
        if (!(output << contents)) {
            throw std::runtime_error("Could not create test input");
        }
    }
    ~TemporaryInput() { std::remove(path_.c_str()); }
    const std::string& path() const { return path_; }
private:
    std::string path_;
};

void testParser() {
    {
        TemporaryInput input("# burst:arrival:priority\r\n 5 : 0 : -2 # first process\r\n\n3:1:1");
        const std::vector<Process> processes = readProcessesFromFile(input.path());
        expectEqual(static_cast<int>(processes.size()), 2, "parser process count");
        expectEqual(processes[0].id, 1, "comments do not consume process IDs");
        expectEqual(processes[1].id, 2, "sequential process IDs");
        expectEqual(processes[0].burstTime, 5, "trimmed burst");
        expectEqual(processes[0].priority, -2, "signed priority");
        expectEqual(processes[1].remainingTime, 3, "initial remaining time");
    }
    const char* invalid[] = {"", "# comment\n", "0:0:1\n", "1:-1:0\n", "1:0:0:\n",
                             "1:0:0:1\n", "1:0:\n", "1::0\n", "1.0:0:0\n",
                             "1:0:1x\n", "2147483648:0:0\n"};
    for (std::size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        TemporaryInput input(invalid[i]);
        bool rejected = false;
        try {
            readProcessesFromFile(input.path());
        } catch (const std::runtime_error& error) {
            rejected = !std::string(error.what()).empty();
        }
        expect(rejected, std::string("parser rejects '") + invalid[i] + "'");
    }
    TemporaryInput input("# comment\n1:0:0\ninvalid\n");
    bool hasLineNumber = false;
    try {
        readProcessesFromFile(input.path());
    } catch (const std::runtime_error& error) {
        hasLineNumber = std::string(error.what()).find("line 3") != std::string::npos;
    }
    expect(hasLineNumber, "parser error identifies the physical line number");
}

class CommaDecimal : public std::numpunct<char> {
protected:
    char do_decimal_point() const { return ','; }
};

void testReporter() {
    ScheduleResult result = runFirstComeFirstServed({makeProcess(1, 3, 0), makeProcess(2, 1, 0)});
    result.algorithmName = "quoted \"name\"\n\\\t";
    std::ostringstream json;
    json.imbue(std::locale(std::locale::classic(), new CommaDecimal));
    json.setf(std::ios::scientific, std::ios::floatfield);
    json.precision(7);
    const std::ios::fmtflags originalFlags = json.flags();
    const std::streamsize originalPrecision = json.precision();
    const std::locale originalLocale = json.getloc();
    printJsonReport(result, json);
    expect(json.str().find("quoted \\\"name\\\"\\n\\\\\\t") != std::string::npos, "JSON escapes strings");
    expect(json.str().find("\"waiting_time\": 1.50") != std::string::npos,
           "JSON uses decimal point regardless of caller locale");
    expect(json.flags() == originalFlags && json.precision() == originalPrecision &&
           json.getloc() == originalLocale, "JSON preserves caller stream format and locale");

    const ScheduleResult large = runFirstComeFirstServed(
        {makeProcess(1, 1000000000, 0), makeProcess(2, 1000000000, 0)});
    std::ostringstream output;
    printJsonReport(large, output);
    expect(output.str().find("\"turnaround_time\": 1500000000.00") != std::string::npos,
           "average accumulation does not overflow int");
}

void runTest(void (*test)(), const char* name) {
    try {
        test();
    } catch (const std::exception& error) {
        expect(false, std::string(name) + " threw unexpectedly: " + error.what());
    }
}
}

int main() {
    runTest(testPreemptionAndMetrics, "preemption and metrics");
    runTest(testIdleGapsAndTies, "idle gaps and ties");
    runTest(testNonPreemptiveSelection, "non-preemptive selection");
    runTest(testRoundRobinBoundary, "Round Robin boundary");
    runTest(testFreshSimulationState, "fresh simulation state");
    runTest(testApiValidationAndTimeLimits, "API validation and time limits");
    runTest(testParser, "parser");
    runTest(testReporter, "reporter");
    if (failures != 0) {
        std::cerr << failures << " scheduler unit test(s) failed.\n";
        return 1;
    }
    std::cout << "All scheduler unit tests passed.\n";
    return 0;
}
