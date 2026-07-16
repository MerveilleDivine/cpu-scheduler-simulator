#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>

#include "Process.h"

ScheduleResult runFirstComeFirstServed(const std::vector<Process>& processes);
ScheduleResult runNoScheduling(const std::vector<Process>& processes);
ScheduleResult runShortestJobFirst(const std::vector<Process>& processes);
ScheduleResult runShortestRemainingTimeFirst(const std::vector<Process>& processes);
ScheduleResult runPriorityScheduling(const std::vector<Process>& processes);
ScheduleResult runRoundRobin(const std::vector<Process>& processes, int quantum);
std::vector<ScheduleResult> runAllAlgorithms(const std::vector<Process>& processes, int roundRobinQuantum);

#endif
