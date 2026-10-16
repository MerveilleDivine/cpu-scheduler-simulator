#include "Reporter.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Averages {
    double waiting;
    double turnaround;
    double response;
};

Averages calculateAverages(const ScheduleResult& result) {
    if (result.processes.empty()) {
        throw std::invalid_argument("Cannot report an empty schedule.");
    }

    long double waiting = 0;
    long double turnaround = 0;
    long double response = 0;
    for (const Process& process : result.processes) {
        waiting += process.waitingTime;
        turnaround += process.turnaroundTime;
        response += process.responseTime;
    }
    const long double count = static_cast<long double>(result.processes.size());
    return {static_cast<double>(waiting / count),
            static_cast<double>(turnaround / count),
            static_cast<double>(response / count)};
}

// Formatting a report must not change how the caller's next output is printed.
class FormatGuard {
public:
    explicit FormatGuard(std::ostream& stream)
        : stream_(stream), flags_(stream.flags()), precision_(stream.precision()),
          fill_(stream.fill()) {}
    ~FormatGuard() {
        stream_.flags(flags_);
        stream_.precision(precision_);
        stream_.fill(fill_);
    }
    FormatGuard(const FormatGuard&) = delete;
    FormatGuard& operator=(const FormatGuard&) = delete;

private:
    std::ostream& stream_;
    std::ios::fmtflags flags_;
    std::streamsize precision_;
    char fill_;
};

void printGanttChart(const std::vector<GanttSegment>& timeline, std::ostream& output) {
    if (timeline.empty()) {
        return;
    }

    std::string bars;
    std::string times = std::to_string(timeline.front().startTime);
    for (const GanttSegment& segment : timeline) {
        const std::string label = segment.processId == 0
            ? "Idle" : "P" + std::to_string(segment.processId);
        const std::string end = std::to_string(segment.endTime);
        const std::size_t width = std::max(label.size() + 3,
            std::max(std::to_string(segment.startTime).size(), end.size()) + 1);
        bars += "| " + label;
        bars.resize(bars.size() + width - label.size() - 2, ' ');
        times.resize(bars.size(), ' ');
        times += end;
    }
    output << "Gantt Chart:\n" << bars << "|\n" << times << '\n';
}

void printJsonString(const std::string& value, std::ostream& output) {
    static const char* hexDigits = "0123456789abcdef";
    output << '"';
    for (unsigned char character : value) {
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
                    output << "\\u00" << hexDigits[character >> 4]
                           << hexDigits[character & 0x0f];
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
}
}

void printReport(const ScheduleResult& result, std::ostream& output) {
    const Averages averages = calculateAverages(result);
    const FormatGuard guard(output);
    output << std::dec << std::setfill(' ');
    const int width = 13;
    output << "Scheduling Method: " << result.algorithmName << '\n';
    const std::array<std::string, 9> headings = {{"Process", "Arrival", "Burst", "Priority",
        "Start", "Complete", "Waiting", "Turnaround", "Response"}};
    for (const std::string& heading : headings) {
        output << std::left << std::setw(width) << heading;
    }
    output << '\n';

    for (const Process& process : result.processes) {
        output << std::left << std::setw(width) << ("P" + std::to_string(process.id))
               << std::setw(width) << process.arrivalTime
               << std::setw(width) << process.burstTime
               << std::setw(width) << process.priority
               << std::setw(width) << process.startTime
               << std::setw(width) << process.completionTime
               << std::setw(width) << process.waitingTime
               << std::setw(width) << process.turnaroundTime
               << std::setw(width) << process.responseTime << '\n';
    }

    output << std::fixed << std::setprecision(2)
           << "Average Waiting Time: " << averages.waiting << " ms\n"
           << "Average Turnaround Time: " << averages.turnaround << " ms\n"
           << "Average Response Time: " << averages.response << " ms\n\n";
    printGanttChart(result.ganttChart, output);
}

void printComparisonReport(const std::vector<ScheduleResult>& results, std::ostream& output) {
    const FormatGuard guard(output);
    output << std::dec << std::setfill(' ')
           << "Scheduling Algorithm Comparison\n"
           << std::left << std::setw(48) << "Algorithm"
           << std::setw(18) << "Avg Waiting"
           << std::setw(20) << "Avg Turnaround"
           << std::setw(18) << "Avg Response" << '\n'
           << std::fixed << std::setprecision(2);

    for (const ScheduleResult& result : results) {
        const Averages averages = calculateAverages(result);
        output << std::left << std::setw(48) << result.algorithmName
               << std::setw(18) << averages.waiting
               << std::setw(20) << averages.turnaround
               << std::setw(18) << averages.response << '\n';
    }
}

void printJsonReport(const ScheduleResult& result, std::ostream& output) {
    const Averages averages = calculateAverages(result);
    // JSON numbers always use a decimal point, regardless of the caller's locale.
    std::ostringstream json;
    json.imbue(std::locale::classic());
    json << "{\n  \"algorithm\": ";
    printJsonString(result.algorithmName, json);
    json << std::fixed << std::setprecision(2)
         << ",\n  \"averages\": {\"waiting_time\": " << averages.waiting
         << ", \"turnaround_time\": " << averages.turnaround
         << ", \"response_time\": " << averages.response << "},\n"
         << "  \"processes\": [\n";

    for (std::size_t i = 0; i < result.processes.size(); ++i) {
        const Process& process = result.processes[i];
        json << "    {\"id\": " << process.id
             << ", \"arrival_time\": " << process.arrivalTime
             << ", \"burst_time\": " << process.burstTime
             << ", \"priority\": " << process.priority
             << ", \"start_time\": " << process.startTime
             << ", \"completion_time\": " << process.completionTime
             << ", \"waiting_time\": " << process.waitingTime
             << ", \"turnaround_time\": " << process.turnaroundTime
             << ", \"response_time\": " << process.responseTime << "}"
             << (i + 1 == result.processes.size() ? "\n" : ",\n");
    }
    json << "  ],\n  \"timeline\": [\n";
    for (std::size_t i = 0; i < result.ganttChart.size(); ++i) {
        const GanttSegment& segment = result.ganttChart[i];
        json << "    {\"process_id\": ";
        if (segment.processId == 0) {
            json << "null";
        } else {
            json << segment.processId;
        }
        json << ", \"start_time\": " << segment.startTime
             << ", \"end_time\": " << segment.endTime << "}"
             << (i + 1 == result.ganttChart.size() ? "\n" : ",\n");
    }
    json << "  ]\n}\n";
    output << json.str();
}

void printJsonComparisonReport(const std::vector<ScheduleResult>& results, std::ostream& output) {
    std::ostringstream json;
    json.imbue(std::locale::classic());
    json << std::fixed << std::setprecision(2) << "{\n  \"algorithms\": [\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const ScheduleResult& result = results[i];
        const Averages averages = calculateAverages(result);
        json << "    {\"algorithm\": ";
        printJsonString(result.algorithmName, json);
        json << ", \"average_waiting_time\": " << averages.waiting
             << ", \"average_turnaround_time\": " << averages.turnaround
             << ", \"average_response_time\": " << averages.response << "}"
             << (i + 1 == results.size() ? "\n" : ",\n");
    }
    json << "  ]\n}\n";
    output << json.str();
}
