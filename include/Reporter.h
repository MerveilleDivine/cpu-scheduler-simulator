#ifndef REPORTER_H
#define REPORTER_H

#include <ostream>
#include <vector>

#include "Process.h"

void printReport(const ScheduleResult& result, std::ostream& output);
void printComparisonReport(const std::vector<ScheduleResult>& results, std::ostream& output);
void printJsonReport(const ScheduleResult& result, std::ostream& output);
void printJsonComparisonReport(const std::vector<ScheduleResult>& results, std::ostream& output);

#endif
