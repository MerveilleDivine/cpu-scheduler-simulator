#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>

#include "Process.h"

// Inputs use positive, unique IDs, nonnegative arrivals, and positive bursts.
// Each run starts from burstTime; existing metrics and remainingTime are ignored.
// Results are sorted by ID. ID 0 denotes idle time in the timeline.
// Invalid inputs or schedules that exceed INT_MAX time units throw std::runtime_error.
ScheduleResult runFirstComeFirstServed(const std::vector<Process>& processes);
// Baseline: run processes in input order, waiting for each one's arrival if needed.
ScheduleResult runNoScheduling(const std::vector<Process>& processes);
ScheduleResult runShortestJobFirst(const std::vector<Process>& processes);
ScheduleResult runShortestRemainingTimeFirst(const std::vector<Process>& processes);
ScheduleResult runPriorityScheduling(const std::vector<Process>& processes);
ScheduleResult runRoundRobin(const std::vector<Process>& processes, int quantum);
std::vector<ScheduleResult> runAllAlgorithms(const std::vector<Process>& processes, int roundRobinQuantum);

#endif
