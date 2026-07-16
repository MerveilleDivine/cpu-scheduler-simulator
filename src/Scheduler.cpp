#include "Scheduler.h"

#include <algorithm>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
void validateProcesses(const vector<Process>& processes) {
    if (processes.empty()) {
        throw runtime_error("At least one process is required.");
    }

    for (size_t i = 0; i < processes.size(); ++i) {
        if (processes[i].burstTime <= 0 || processes[i].remainingTime <= 0) {
            throw runtime_error("Process burst and remaining time must be greater than 0.");
        }
        if (processes[i].arrivalTime < 0) {
            throw runtime_error("Process arrival time cannot be negative.");
        }
    }
}

void calculateMetrics(Process& process) {
    process.turnaroundTime = process.completionTime - process.arrivalTime;
    process.waitingTime = process.turnaroundTime - process.burstTime;
    process.responseTime = process.startTime - process.arrivalTime;
}

void sortByProcessId(vector<Process>& processes) {
    sort(processes.begin(), processes.end(), [](const Process& first, const Process& second) {
        return first.id < second.id;
    });
}

void addGanttSegment(vector<GanttSegment>& ganttChart, int processId, int startTime, int endTime) {
    if (startTime == endTime) {
        return;
    }

    if (!ganttChart.empty() && ganttChart.back().processId == processId &&
        ganttChart.back().endTime == startTime) {
        ganttChart.back().endTime = endTime;
        return;
    }

    ganttChart.push_back(GanttSegment(processId, startTime, endTime));
}

ScheduleResult makeResult(const string& algorithmName, vector<Process> processes, vector<GanttSegment> ganttChart) {
    sortByProcessId(processes);

    ScheduleResult result;
    result.algorithmName = algorithmName;
    result.processes = processes;
    result.ganttChart = ganttChart;
    return result;
}
}

ScheduleResult runFirstComeFirstServed(const vector<Process>& inputProcesses) {
    validateProcesses(inputProcesses);
    vector<Process> processes = inputProcesses;
    vector<GanttSegment> ganttChart;

    sort(processes.begin(), processes.end(), [](const Process& first, const Process& second) {
        if (first.arrivalTime == second.arrivalTime) {
            return first.id < second.id;
        }
        return first.arrivalTime < second.arrivalTime;
    });

    int currentTime = 0;

    for (size_t i = 0; i < processes.size(); ++i) {
        if (currentTime < processes[i].arrivalTime) {
            addGanttSegment(ganttChart, 0, currentTime, processes[i].arrivalTime);
            currentTime = processes[i].arrivalTime;
        }

        processes[i].startTime = currentTime;
        int endTime = currentTime + processes[i].burstTime;
        addGanttSegment(ganttChart, processes[i].id, currentTime, endTime);
        currentTime = endTime;
        processes[i].completionTime = currentTime;
        processes[i].remainingTime = 0;
        calculateMetrics(processes[i]);
    }

    return makeResult("First Come, First Served", processes, ganttChart);
}

ScheduleResult runNoScheduling(const vector<Process>& inputProcesses) {
    validateProcesses(inputProcesses);
    vector<Process> processes = inputProcesses;
    vector<GanttSegment> ganttChart;
    int currentTime = 0;

    for (size_t i = 0; i < processes.size(); ++i) {
        if (currentTime < processes[i].arrivalTime) {
            addGanttSegment(ganttChart, 0, currentTime, processes[i].arrivalTime);
            currentTime = processes[i].arrivalTime;
        }

        processes[i].startTime = currentTime;
        int endTime = currentTime + processes[i].burstTime;
        addGanttSegment(ganttChart, processes[i].id, currentTime, endTime);
        currentTime = endTime;
        processes[i].completionTime = currentTime;
        processes[i].remainingTime = 0;
        calculateMetrics(processes[i]);
    }

    return makeResult("No Scheduling", processes, ganttChart);
}

ScheduleResult runShortestJobFirst(const vector<Process>& inputProcesses) {
    validateProcesses(inputProcesses);
    vector<Process> processes = inputProcesses;
    vector<GanttSegment> ganttChart;
    int currentTime = 0;
    int completedProcesses = 0;
    vector<bool> completed(processes.size(), false);

    while (completedProcesses < static_cast<int>(processes.size())) {
        int selectedIndex = -1;

        for (size_t i = 0; i < processes.size(); ++i) {
            if (completed[i] || processes[i].arrivalTime > currentTime) {
                continue;
            }

            if (selectedIndex == -1 ||
                processes[i].burstTime < processes[selectedIndex].burstTime ||
                (processes[i].burstTime == processes[selectedIndex].burstTime &&
                 processes[i].arrivalTime < processes[selectedIndex].arrivalTime) ||
                (processes[i].burstTime == processes[selectedIndex].burstTime &&
                 processes[i].arrivalTime == processes[selectedIndex].arrivalTime &&
                 processes[i].id < processes[selectedIndex].id)) {
                selectedIndex = static_cast<int>(i);
            }
        }

        if (selectedIndex == -1) {
            int nextArrival = -1;
            for (size_t i = 0; i < processes.size(); ++i) {
                if (!completed[i] && (nextArrival == -1 || processes[i].arrivalTime < nextArrival)) {
                    nextArrival = processes[i].arrivalTime;
                }
            }
            addGanttSegment(ganttChart, 0, currentTime, nextArrival);
            currentTime = nextArrival;
            continue;
        }

        Process& process = processes[selectedIndex];
        process.startTime = currentTime;
        int endTime = currentTime + process.burstTime;
        addGanttSegment(ganttChart, process.id, currentTime, endTime);
        currentTime = endTime;
        process.completionTime = currentTime;
        process.remainingTime = 0;
        calculateMetrics(process);

        completed[selectedIndex] = true;
        ++completedProcesses;
    }

    return makeResult("Shortest Job First - Non-Preemptive", processes, ganttChart);
}

ScheduleResult runShortestRemainingTimeFirst(const vector<Process>& inputProcesses) {
    validateProcesses(inputProcesses);
    vector<Process> processes = inputProcesses;
    vector<GanttSegment> ganttChart;
    int currentTime = 0;
    int completedProcesses = 0;

    while (completedProcesses < static_cast<int>(processes.size())) {
        int selectedIndex = -1;

        for (size_t i = 0; i < processes.size(); ++i) {
            if (processes[i].remainingTime <= 0 || processes[i].arrivalTime > currentTime) {
                continue;
            }

            if (selectedIndex == -1 ||
                processes[i].remainingTime < processes[selectedIndex].remainingTime ||
                (processes[i].remainingTime == processes[selectedIndex].remainingTime &&
                 processes[i].arrivalTime < processes[selectedIndex].arrivalTime) ||
                (processes[i].remainingTime == processes[selectedIndex].remainingTime &&
                 processes[i].arrivalTime == processes[selectedIndex].arrivalTime &&
                 processes[i].id < processes[selectedIndex].id)) {
                selectedIndex = static_cast<int>(i);
            }
        }

        if (selectedIndex == -1) {
            int nextArrival = -1;
            for (size_t i = 0; i < processes.size(); ++i) {
                if (processes[i].remainingTime > 0 &&
                    (nextArrival == -1 || processes[i].arrivalTime < nextArrival)) {
                    nextArrival = processes[i].arrivalTime;
                }
            }
            addGanttSegment(ganttChart, 0, currentTime, nextArrival);
            currentTime = nextArrival;
            continue;
        }

        Process& process = processes[selectedIndex];
        if (process.startTime == -1) {
            process.startTime = currentTime;
        }

        addGanttSegment(ganttChart, process.id, currentTime, currentTime + 1);
        --process.remainingTime;
        ++currentTime;

        if (process.remainingTime == 0) {
            process.completionTime = currentTime;
            calculateMetrics(process);
            ++completedProcesses;
        }
    }

    return makeResult("Shortest Remaining Time First - Preemptive", processes, ganttChart);
}

ScheduleResult runPriorityScheduling(const vector<Process>& inputProcesses) {
    validateProcesses(inputProcesses);
    vector<Process> processes = inputProcesses;
    vector<GanttSegment> ganttChart;
    int currentTime = 0;
    int completedProcesses = 0;
    vector<bool> completed(processes.size(), false);

    while (completedProcesses < static_cast<int>(processes.size())) {
        int selectedIndex = -1;

        for (size_t i = 0; i < processes.size(); ++i) {
            if (completed[i] || processes[i].arrivalTime > currentTime) {
                continue;
            }

            if (selectedIndex == -1 ||
                processes[i].priority < processes[selectedIndex].priority ||
                (processes[i].priority == processes[selectedIndex].priority &&
                 processes[i].arrivalTime < processes[selectedIndex].arrivalTime) ||
                (processes[i].priority == processes[selectedIndex].priority &&
                 processes[i].arrivalTime == processes[selectedIndex].arrivalTime &&
                 processes[i].id < processes[selectedIndex].id)) {
                selectedIndex = static_cast<int>(i);
            }
        }

        if (selectedIndex == -1) {
            int nextArrival = -1;
            for (size_t i = 0; i < processes.size(); ++i) {
                if (!completed[i] && (nextArrival == -1 || processes[i].arrivalTime < nextArrival)) {
                    nextArrival = processes[i].arrivalTime;
                }
            }
            addGanttSegment(ganttChart, 0, currentTime, nextArrival);
            currentTime = nextArrival;
            continue;
        }

        Process& process = processes[selectedIndex];
        process.startTime = currentTime;
        int endTime = currentTime + process.burstTime;
        addGanttSegment(ganttChart, process.id, currentTime, endTime);
        currentTime = endTime;
        process.completionTime = currentTime;
        process.remainingTime = 0;
        calculateMetrics(process);

        completed[selectedIndex] = true;
        ++completedProcesses;
    }

    return makeResult("Priority Scheduling - Non-Preemptive", processes, ganttChart);
}

ScheduleResult runRoundRobin(const vector<Process>& inputProcesses, int quantum) {
    if (quantum <= 0) {
        throw runtime_error("Round Robin quantum must be greater than 0.");
    }
    validateProcesses(inputProcesses);

    vector<Process> processes = inputProcesses;
    vector<GanttSegment> ganttChart;

    sort(processes.begin(), processes.end(), [](const Process& first, const Process& second) {
        if (first.arrivalTime == second.arrivalTime) {
            return first.id < second.id;
        }
        return first.arrivalTime < second.arrivalTime;
    });

    queue<int> readyQueue;
    int currentTime = 0;
    size_t nextProcessIndex = 0;
    int completedProcesses = 0;

    while (completedProcesses < static_cast<int>(processes.size())) {
        if (readyQueue.empty() && nextProcessIndex < processes.size() &&
            currentTime < processes[nextProcessIndex].arrivalTime) {
            addGanttSegment(ganttChart, 0, currentTime, processes[nextProcessIndex].arrivalTime);
            currentTime = processes[nextProcessIndex].arrivalTime;
        }

        while (nextProcessIndex < processes.size() &&
               processes[nextProcessIndex].arrivalTime <= currentTime) {
            readyQueue.push(static_cast<int>(nextProcessIndex));
            ++nextProcessIndex;
        }

        if (readyQueue.empty()) {
            continue;
        }

        int processIndex = readyQueue.front();
        readyQueue.pop();
        Process& process = processes[processIndex];

        if (process.startTime == -1) {
            process.startTime = currentTime;
        }

        int timeSlice = min(quantum, process.remainingTime);
        int endTime = currentTime + timeSlice;
        addGanttSegment(ganttChart, process.id, currentTime, endTime);
        process.remainingTime -= timeSlice;
        currentTime = endTime;

        while (nextProcessIndex < processes.size() &&
               processes[nextProcessIndex].arrivalTime <= currentTime) {
            readyQueue.push(static_cast<int>(nextProcessIndex));
            ++nextProcessIndex;
        }

        if (process.remainingTime > 0) {
            readyQueue.push(processIndex);
        } else {
            process.completionTime = currentTime;
            calculateMetrics(process);
            ++completedProcesses;
        }
    }

    stringstream name;
    name << "Round Robin - Quantum " << quantum;
    return makeResult(name.str(), processes, ganttChart);
}

vector<ScheduleResult> runAllAlgorithms(const vector<Process>& processes, int roundRobinQuantum) {
    vector<ScheduleResult> results;
    results.push_back(runFirstComeFirstServed(processes));
    results.push_back(runShortestJobFirst(processes));
    results.push_back(runShortestRemainingTimeFirst(processes));
    results.push_back(runPriorityScheduling(processes));
    results.push_back(runRoundRobin(processes, roundRobinQuantum));
    return results;
}
