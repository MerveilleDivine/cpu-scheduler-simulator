#include "Reporter.h"

#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

using namespace std;

namespace {
double averageWaitingTime(const ScheduleResult& result) {
    int total = 0;
    for (size_t i = 0; i < result.processes.size(); ++i) {
        total += result.processes[i].waitingTime;
    }
    return total / static_cast<double>(result.processes.size());
}

double averageTurnaroundTime(const ScheduleResult& result) {
    int total = 0;
    for (size_t i = 0; i < result.processes.size(); ++i) {
        total += result.processes[i].turnaroundTime;
    }
    return total / static_cast<double>(result.processes.size());
}

double averageResponseTime(const ScheduleResult& result) {
    int total = 0;
    for (size_t i = 0; i < result.processes.size(); ++i) {
        total += result.processes[i].responseTime;
    }
    return total / static_cast<double>(result.processes.size());
}

void printGanttChart(const vector<GanttSegment>& ganttChart, ostream& output) {
    if (ganttChart.empty()) {
        return;
    }

    output << "Gantt Chart:" << endl;

    for (size_t i = 0; i < ganttChart.size(); ++i) {
        output << "| ";
        if (ganttChart[i].processId == 0) {
            output << "Idle";
        } else {
            output << "P" << ganttChart[i].processId;
        }
        output << " ";
    }
    output << "|" << endl;

    output << ganttChart.front().startTime;
    for (size_t i = 0; i < ganttChart.size(); ++i) {
        output << setw(6) << ganttChart[i].endTime;
    }
    output << endl;
}

void printJsonString(const string& value, ostream& output) {
    static const char* hexDigits = "0123456789abcdef";
    output << '"';
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(value[i]);
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u00"
                           << hexDigits[(character >> 4) & 0x0f]
                           << hexDigits[character & 0x0f];
                } else {
                    output << value[i];
                }
        }
    }
    output << '"';
}
}

void printReport(const ScheduleResult& result, ostream& output) {
    int totalWaitingTime = 0;
    int totalTurnaroundTime = 0;
    int totalResponseTime = 0;

    output << "Scheduling Method: " << result.algorithmName << endl;
    output << left << setw(10) << "Process"
           << setw(12) << "Arrival"
           << setw(10) << "Burst"
           << setw(10) << "Priority"
           << setw(10) << "Start"
           << setw(12) << "Complete"
           << setw(10) << "Waiting"
           << setw(12) << "Turnaround"
           << setw(10) << "Response" << endl;

    for (size_t i = 0; i < result.processes.size(); ++i) {
        const Process& process = result.processes[i];
        output << left << setw(10) << ("P" + to_string(process.id))
               << setw(12) << process.arrivalTime
               << setw(10) << process.burstTime
               << setw(10) << process.priority
               << setw(10) << process.startTime
               << setw(12) << process.completionTime
               << setw(10) << process.waitingTime
               << setw(12) << process.turnaroundTime
               << setw(10) << process.responseTime << endl;

        totalWaitingTime += process.waitingTime;
        totalTurnaroundTime += process.turnaroundTime;
        totalResponseTime += process.responseTime;
    }

    output << fixed << setprecision(2);
    output << "Average Waiting Time: "
           << totalWaitingTime / static_cast<double>(result.processes.size()) << " ms" << endl;
    output << "Average Turnaround Time: "
           << totalTurnaroundTime / static_cast<double>(result.processes.size()) << " ms" << endl;
    output << "Average Response Time: "
           << totalResponseTime / static_cast<double>(result.processes.size()) << " ms" << endl;
    output << endl;
    printGanttChart(result.ganttChart, output);
}

void printComparisonReport(const vector<ScheduleResult>& results, ostream& output) {
    output << "Scheduling Algorithm Comparison" << endl;
    output << left << setw(48) << "Algorithm"
           << setw(18) << "Avg Waiting"
           << setw(20) << "Avg Turnaround"
           << setw(18) << "Avg Response" << endl;

    output << fixed << setprecision(2);
    for (size_t i = 0; i < results.size(); ++i) {
        output << left << setw(48) << results[i].algorithmName
               << setw(18) << averageWaitingTime(results[i])
               << setw(20) << averageTurnaroundTime(results[i])
               << setw(18) << averageResponseTime(results[i]) << endl;
    }
}

void printJsonReport(const ScheduleResult& result, ostream& output) {
    output << "{\n  \"algorithm\": ";
    printJsonString(result.algorithmName, output);
    output << fixed << setprecision(2);
    output << ",\n  \"averages\": {"
           << "\"waiting_time\": " << averageWaitingTime(result) << ", "
           << "\"turnaround_time\": " << averageTurnaroundTime(result) << ", "
           << "\"response_time\": " << averageResponseTime(result) << "},\n";

    output << "  \"processes\": [\n";
    for (size_t i = 0; i < result.processes.size(); ++i) {
        const Process& process = result.processes[i];
        output << "    {\"id\": " << process.id
               << ", \"arrival_time\": " << process.arrivalTime
               << ", \"burst_time\": " << process.burstTime
               << ", \"priority\": " << process.priority
               << ", \"start_time\": " << process.startTime
               << ", \"completion_time\": " << process.completionTime
               << ", \"waiting_time\": " << process.waitingTime
               << ", \"turnaround_time\": " << process.turnaroundTime
               << ", \"response_time\": " << process.responseTime << "}";
        output << (i + 1 == result.processes.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"timeline\": [\n";
    for (size_t i = 0; i < result.ganttChart.size(); ++i) {
        const GanttSegment& segment = result.ganttChart[i];
        output << "    {\"process_id\": ";
        if (segment.processId == 0) {
            output << "null";
        } else {
            output << segment.processId;
        }
        output << ", \"start_time\": " << segment.startTime
               << ", \"end_time\": " << segment.endTime << "}";
        output << (i + 1 == result.ganttChart.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

void printJsonComparisonReport(const vector<ScheduleResult>& results, ostream& output) {
    output << fixed << setprecision(2);
    output << "{\n  \"algorithms\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        output << "    {\"algorithm\": ";
        printJsonString(results[i].algorithmName, output);
        output << ", \"average_waiting_time\": " << averageWaitingTime(results[i])
               << ", \"average_turnaround_time\": " << averageTurnaroundTime(results[i])
               << ", \"average_response_time\": " << averageResponseTime(results[i]) << "}";
        output << (i + 1 == results.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}
