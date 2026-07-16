#include "Parser.h"
#include "Reporter.h"
#include "Scheduler.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
int failures = 0;

void expect(bool condition, const string& message) {
    if (!condition) {
        cerr << "FAILED: " << message << endl;
        ++failures;
    }
}

void expectEqual(int actual, int expected, const string& message) {
    if (actual != expected) {
        cerr << "FAILED: " << message << " (expected " << expected
             << ", received " << actual << ")" << endl;
        ++failures;
    }
}

Process makeProcess(int id, int burstTime, int arrivalTime, int priority = 0) {
    Process process;
    process.id = id;
    process.burstTime = burstTime;
    process.remainingTime = burstTime;
    process.arrivalTime = arrivalTime;
    process.priority = priority;
    return process;
}

void testShortestRemainingTimeFirstPreempts() {
    vector<Process> processes;
    processes.push_back(makeProcess(1, 8, 0));
    processes.push_back(makeProcess(2, 4, 1));
    processes.push_back(makeProcess(3, 2, 2));

    const ScheduleResult result = runShortestRemainingTimeFirst(processes);

    expect(result.algorithmName == "Shortest Remaining Time First - Preemptive", "SRTF name");
    expectEqual(static_cast<int>(result.ganttChart.size()), 5, "SRTF segment count");
    expectEqual(result.ganttChart[0].processId, 1, "SRTF starts P1");
    expectEqual(result.ganttChart[0].endTime, 1, "SRTF preempts P1 at t=1");
    expectEqual(result.ganttChart[1].processId, 2, "SRTF runs P2 after first arrival");
    expectEqual(result.ganttChart[2].processId, 3, "SRTF runs shortest arriving P3");

    expectEqual(result.processes[0].completionTime, 14, "P1 completion");
    expectEqual(result.processes[0].waitingTime, 6, "P1 waiting time");
    expectEqual(result.processes[1].completionTime, 7, "P2 completion");
    expectEqual(result.processes[1].waitingTime, 2, "P2 waiting time");
    expectEqual(result.processes[2].completionTime, 4, "P3 completion");
    expectEqual(result.processes[2].waitingTime, 0, "P3 waiting time");
}

void testShortestRemainingTimeFirstRecordsIdleTime() {
    vector<Process> processes;
    processes.push_back(makeProcess(1, 2, 3));

    const ScheduleResult result = runShortestRemainingTimeFirst(processes);

    expectEqual(static_cast<int>(result.ganttChart.size()), 2, "idle workload segment count");
    expectEqual(result.ganttChart[0].processId, 0, "idle segment process id");
    expectEqual(result.ganttChart[0].startTime, 0, "idle segment start");
    expectEqual(result.ganttChart[0].endTime, 3, "idle segment end");
    expectEqual(result.processes[0].responseTime, 0, "response time excludes pre-arrival idle time");
}

void testParserSupportsCommentsAndWhitespace() {
    const string path = "scheduler_parser_valid_input.txt";
    {
        ofstream output(path.c_str());
        output << "# burst:arrival:priority\n";
        output << " 5 : 0 : 2  # first process\n";
        output << "3:1:1\n";
    }

    const vector<Process> processes = readProcessesFromFile(path);
    remove(path.c_str());

    expectEqual(static_cast<int>(processes.size()), 2, "parser process count");
    expectEqual(processes[0].burstTime, 5, "trimmed burst time");
    expectEqual(processes[1].arrivalTime, 1, "second arrival time");
}

void testParserRejectsInvalidValues() {
    const string path = "scheduler_parser_invalid_input.txt";
    {
        ofstream output(path.c_str());
        output << "0:0:1\n";
    }

    bool rejected = false;
    try {
        readProcessesFromFile(path);
    } catch (const runtime_error& error) {
        rejected = string(error.what()).find("greater than 0") != string::npos;
    }
    remove(path.c_str());

    expect(rejected, "parser rejects a zero burst with a useful message");
}

void testJsonReportContainsStructuredData() {
    vector<Process> processes;
    processes.push_back(makeProcess(1, 3, 0));
    const ScheduleResult result = runFirstComeFirstServed(processes);
    stringstream output;

    printJsonReport(result, output);

    expect(output.str().find("\"algorithm\": \"First Come, First Served\"") != string::npos,
           "JSON algorithm field");
    expect(output.str().find("\"processes\": [") != string::npos, "JSON process array");
    expect(output.str().find("\"timeline\": [") != string::npos, "JSON timeline array");
}

void testRoundRobinRejectsInvalidQuantum() {
    vector<Process> processes;
    processes.push_back(makeProcess(1, 3, 0));
    bool rejected = false;
    try {
        runRoundRobin(processes, 0);
    } catch (const runtime_error&) {
        rejected = true;
    }
    expect(rejected, "Round Robin rejects a non-positive quantum");
}

void testSchedulerRejectsInvalidProcessData() {
    vector<Process> processes;
    processes.push_back(makeProcess(1, 0, 0));
    bool rejected = false;
    try {
        runShortestRemainingTimeFirst(processes);
    } catch (const runtime_error&) {
        rejected = true;
    }
    expect(rejected, "scheduler rejects invalid process data from direct callers");
}
}

int main() {
    testShortestRemainingTimeFirstPreempts();
    testShortestRemainingTimeFirstRecordsIdleTime();
    testParserSupportsCommentsAndWhitespace();
    testParserRejectsInvalidValues();
    testJsonReportContainsStructuredData();
    testRoundRobinRejectsInvalidQuantum();
    testSchedulerRejectsInvalidProcessData();

    if (failures != 0) {
        cerr << failures << " scheduler unit test(s) failed." << endl;
        return 1;
    }

    cout << "All scheduler unit tests passed." << endl;
    return 0;
}
