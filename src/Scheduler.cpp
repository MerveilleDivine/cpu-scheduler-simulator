#include "Scheduler.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
bool arrivesBefore(const Process& first, const Process& second) {
    if (first.arrivalTime != second.arrivalTime) {
        return first.arrivalTime < second.arrivalTime;
    }
    return first.id < second.id;
}

int checkedEndTime(int startTime, int duration) {
    if (duration > std::numeric_limits<int>::max() - startTime) {
        throw std::overflow_error("Schedule duration exceeds the supported time range (INT_MAX).");
    }
    return startTime + duration;
}

std::vector<Process> prepareProcesses(const std::vector<Process>& input, bool sortByArrival) {
    if (input.empty()) {
        throw std::runtime_error("At least one process is required.");
    }

    std::unordered_set<int> ids;
    ids.reserve(input.size());
    std::vector<Process> processes = input;
    for (Process& process : processes) {
        if (process.id <= 0) {
            throw std::runtime_error("Process IDs must be greater than 0; ID 0 is reserved for idle time.");
        }
        if (!ids.insert(process.id).second) {
            throw std::runtime_error("Process IDs must be unique.");
        }
        if (process.burstTime <= 0) {
            throw std::runtime_error("Process burst time must be greater than 0.");
        }
        if (process.arrivalTime < 0) {
            throw std::runtime_error("Process arrival time cannot be negative.");
        }

        process.remainingTime = process.burstTime;
        process.startTime = -1;
        process.completionTime = 0;
        process.waitingTime = 0;
        process.turnaroundTime = 0;
        process.responseTime = 0;
    }

    if (sortByArrival) {
        std::sort(processes.begin(), processes.end(), arrivesBefore);
    }

    // Arrival order gives the common finish time of work-conserving policies.
    // For the input-order baseline, this is its actual finish time instead.
    int finishTime = 0;
    for (const Process& process : processes) {
        finishTime = checkedEndTime(std::max(finishTime, process.arrivalTime), process.burstTime);
    }
    return processes;
}

void addGanttSegment(std::vector<GanttSegment>& chart, int processId, int startTime, int endTime) {
    if (startTime == endTime) {
        return;
    }
    if (!chart.empty() && chart.back().processId == processId && chart.back().endTime == startTime) {
        chart.back().endTime = endTime;
    } else {
        chart.push_back(GanttSegment(processId, startTime, endTime));
    }
}

void advanceToArrival(int& currentTime, int arrivalTime, std::vector<GanttSegment>& chart) {
    if (currentTime < arrivalTime) {
        addGanttSegment(chart, 0, currentTime, arrivalTime);
        currentTime = arrivalTime;
    }
}

void executeFor(Process& process, int duration, int& currentTime, std::vector<GanttSegment>& chart) {
    const int endTime = checkedEndTime(currentTime, duration);
    if (process.startTime == -1) {
        process.startTime = currentTime;
    }
    addGanttSegment(chart, process.id, currentTime, endTime);
    process.remainingTime -= duration;
    currentTime = endTime;

    if (process.remainingTime == 0) {
        process.completionTime = currentTime;
        process.turnaroundTime = currentTime - process.arrivalTime;
        process.waitingTime = process.turnaroundTime - process.burstTime;
        process.responseTime = process.startTime - process.arrivalTime;
    }
}

ScheduleResult makeResult(const std::string& name, std::vector<Process> processes,
                          std::vector<GanttSegment> chart) {
    std::sort(processes.begin(), processes.end(), [](const Process& first, const Process& second) {
        return first.id < second.id;
    });

    ScheduleResult result;
    result.algorithmName = name;
    result.processes = std::move(processes);
    result.ganttChart = std::move(chart);
    return result;
}

struct ReadyOrder {
    const std::vector<Process>* processes;
    int Process::*field;

    bool operator()(std::size_t firstIndex, std::size_t secondIndex) const {
        const Process& first = (*processes)[firstIndex];
        const Process& second = (*processes)[secondIndex];
        if (first.*field != second.*field) {
            return first.*field > second.*field;
        }
        return arrivesBefore(second, first);
    }
};

typedef std::priority_queue<std::size_t, std::vector<std::size_t>, ReadyOrder> ReadyQueue;

template <typename Queue>
void admitArrivals(const std::vector<Process>& processes, std::size_t& nextArrival,
                   int currentTime, Queue& ready) {
    while (nextArrival < processes.size() && processes[nextArrival].arrivalTime <= currentTime) {
        ready.push(nextArrival++);
    }
}

ScheduleResult runInOrder(const std::vector<Process>& input, bool sortByArrival,
                          const std::string& name) {
    std::vector<Process> processes = prepareProcesses(input, sortByArrival);
    std::vector<GanttSegment> chart;
    int currentTime = 0;

    for (Process& process : processes) {
        advanceToArrival(currentTime, process.arrivalTime, chart);
        executeFor(process, process.remainingTime, currentTime, chart);
    }
    return makeResult(name, std::move(processes), std::move(chart));
}

ScheduleResult runNonPreemptive(const std::vector<Process>& input, int Process::*field,
                                const std::string& name) {
    std::vector<Process> processes = prepareProcesses(input, true);
    std::vector<GanttSegment> chart;
    const ReadyOrder order = {&processes, field};
    ReadyQueue ready(order);
    std::size_t nextArrival = 0;
    int currentTime = 0;

    while (nextArrival < processes.size() || !ready.empty()) {
        if (ready.empty()) {
            advanceToArrival(currentTime, processes[nextArrival].arrivalTime, chart);
        }
        admitArrivals(processes, nextArrival, currentTime, ready);

        Process& process = processes[ready.top()];
        ready.pop();
        executeFor(process, process.remainingTime, currentTime, chart);
    }
    return makeResult(name, std::move(processes), std::move(chart));
}
}

ScheduleResult runFirstComeFirstServed(const std::vector<Process>& processes) {
    return runInOrder(processes, true, "First Come, First Served");
}

ScheduleResult runNoScheduling(const std::vector<Process>& processes) {
    return runInOrder(processes, false, "No Scheduling");
}

ScheduleResult runShortestJobFirst(const std::vector<Process>& processes) {
    return runNonPreemptive(processes, &Process::burstTime, "Shortest Job First - Non-Preemptive");
}

ScheduleResult runShortestRemainingTimeFirst(const std::vector<Process>& input) {
    std::vector<Process> processes = prepareProcesses(input, true);
    std::vector<GanttSegment> chart;
    const ReadyOrder order = {&processes, &Process::remainingTime};
    ReadyQueue ready(order);
    std::size_t nextArrival = 0;
    int currentTime = 0;

    while (nextArrival < processes.size() || !ready.empty()) {
        if (ready.empty()) {
            advanceToArrival(currentTime, processes[nextArrival].arrivalTime, chart);
        }
        admitArrivals(processes, nextArrival, currentTime, ready);

        const std::size_t selected = ready.top();
        ready.pop();
        Process& process = processes[selected];
        int duration = process.remainingTime;
        if (nextArrival < processes.size()) {
            duration = std::min(duration, processes[nextArrival].arrivalTime - currentTime);
        }
        // Only a new arrival can change which ready process has the least work left.
        executeFor(process, duration, currentTime, chart);
        if (process.remainingTime > 0) {
            ready.push(selected);
        }
    }
    return makeResult("Shortest Remaining Time First - Preemptive", std::move(processes),
                      std::move(chart));
}

ScheduleResult runPriorityScheduling(const std::vector<Process>& processes) {
    return runNonPreemptive(processes, &Process::priority, "Priority Scheduling - Non-Preemptive");
}

ScheduleResult runRoundRobin(const std::vector<Process>& input, int quantum) {
    if (quantum <= 0) {
        throw std::runtime_error("Round Robin quantum must be greater than 0.");
    }
    std::vector<Process> processes = prepareProcesses(input, true);
    std::vector<GanttSegment> chart;
    std::queue<std::size_t> ready;
    std::size_t nextArrival = 0;
    int currentTime = 0;

    while (nextArrival < processes.size() || !ready.empty()) {
        if (ready.empty()) {
            advanceToArrival(currentTime, processes[nextArrival].arrivalTime, chart);
        }
        admitArrivals(processes, nextArrival, currentTime, ready);

        const std::size_t selected = ready.front();
        ready.pop();
        Process& process = processes[selected];
        int duration = std::min(quantum, process.remainingTime);
        if (ready.empty()) {
            duration = process.remainingTime;
            if (nextArrival < processes.size()) {
                const long long gap = processes[nextArrival].arrivalTime - currentTime;
                // Skip uncontested slices, stopping at the first quantum boundary after an arrival.
                const long long boundary = ((gap + quantum - 1) / quantum) * quantum;
                duration = static_cast<int>(std::min<long long>(duration, boundary));
            }
        }
        executeFor(process, duration, currentTime, chart);

        // Arrivals during this slice join the queue before the running process returns to it.
        admitArrivals(processes, nextArrival, currentTime, ready);
        if (process.remainingTime > 0) {
            ready.push(selected);
        }
    }

    return makeResult("Round Robin - Quantum " + std::to_string(quantum), std::move(processes),
                      std::move(chart));
}

std::vector<ScheduleResult> runAllAlgorithms(const std::vector<Process>& processes, int quantum) {
    if (quantum <= 0) {
        throw std::runtime_error("Round Robin quantum must be greater than 0.");
    }
    std::vector<ScheduleResult> results;
    results.reserve(5);
    results.push_back(runFirstComeFirstServed(processes));
    results.push_back(runShortestJobFirst(processes));
    results.push_back(runShortestRemainingTimeFirst(processes));
    results.push_back(runPriorityScheduling(processes));
    results.push_back(runRoundRobin(processes, quantum));
    return results;
}
