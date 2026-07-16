#include "Parser.h"
#include "Reporter.h"
#include "Scheduler.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
string toLower(string value) {
    for (size_t i = 0; i < value.size(); ++i) {
        value[i] = static_cast<char>(tolower(static_cast<unsigned char>(value[i])));
    }
    return value;
}

int parseIntegerArgument(const string& value, const string& optionName) {
    size_t parsedCharacters = 0;
    try {
        const int parsed = stoi(value, &parsedCharacters);
        if (parsedCharacters != value.size()) {
            throw invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const exception&) {
        throw runtime_error(optionName + " expects an integer, received: " + value);
    }
}

void printHelp() {
    cout << "CPU Scheduler Simulator" << endl;
    cout << "Usage:" << endl;
    cout << "  ./scheduler -f input.txt -o output.txt --algorithm fcfs" << endl;
    cout << "  ./scheduler -f input.txt -o output.txt --algorithm rr --quantum 2" << endl;
    cout << "  ./scheduler -f input.txt --algorithm srtf --format json" << endl;
    cout << "  ./scheduler -f input.txt --compare --quantum 2" << endl;
    cout << endl;
    cout << "Algorithms: none, fcfs, sjf, srtf, priority, rr" << endl;
    cout << "Formats: text, json" << endl;
}

string normalizeAlgorithmName(const string& input) {
    string value = toLower(input);

    if (value == "1" || value == "fcfs" || value == "first-come-first-served") {
        return "fcfs";
    }
    if (value == "2" || value == "sjf" || value == "shortest-job-first") {
        return "sjf";
    }
    if (value == "3" || value == "priority") {
        return "priority";
    }
    if (value == "4" || value == "srtf" || value == "shortest-remaining-time-first") {
        return "srtf";
    }
    if (value == "5" || value == "rr" || value == "round-robin") {
        return "rr";
    }
    if (value == "6" || value == "none" || value == "no-scheduling") {
        return "none";
    }

    return value;
}

ScheduleResult runSelectedAlgorithm(const vector<Process>& processes, const string& algorithm, int quantum) {
    if (algorithm == "fcfs") {
        return runFirstComeFirstServed(processes);
    }
    if (algorithm == "sjf") {
        return runShortestJobFirst(processes);
    }
    if (algorithm == "srtf") {
        return runShortestRemainingTimeFirst(processes);
    }
    if (algorithm == "priority") {
        return runPriorityScheduling(processes);
    }
    if (algorithm == "rr") {
        return runRoundRobin(processes, quantum);
    }
    if (algorithm == "none") {
        return runNoScheduling(processes);
    }

    throw runtime_error("Unsupported scheduling algorithm: " + algorithm);
}

void writeReport(const ScheduleResult& result, const string& format, ostream& output) {
    if (format == "json") {
        printJsonReport(result, output);
    } else {
        printReport(result, output);
    }
}

void writeComparisonReport(const vector<ScheduleResult>& results, const string& format, ostream& output) {
    if (format == "json") {
        printJsonComparisonReport(results, output);
    } else {
        printComparisonReport(results, output);
    }
}
}

int main(int argc, char** argv) {
    try {
        string inputFile;
        string outputFile;
        string algorithm;
        string format = "text";
        int quantum = 0;
        bool compareMode = false;
        bool quantumProvided = false;

        for (int i = 1; i < argc; ++i) {
            string argument = argv[i];

            if (argument == "--help" || argument == "-h") {
                printHelp();
                return 0;
            } else if ((argument == "-f" || argument == "--input") && i + 1 < argc) {
                inputFile = argv[++i];
            } else if ((argument == "-o" || argument == "--output") && i + 1 < argc) {
                outputFile = argv[++i];
            } else if ((argument == "-a" || argument == "--algorithm") && i + 1 < argc) {
                algorithm = normalizeAlgorithmName(argv[++i]);
            } else if ((argument == "-q" || argument == "--quantum") && i + 1 < argc) {
                quantum = parseIntegerArgument(argv[++i], "--quantum");
                quantumProvided = true;
            } else if (argument == "--format" && i + 1 < argc) {
                format = toLower(argv[++i]);
            } else if (argument == "--compare") {
                compareMode = true;
            } else {
                throw runtime_error("Unknown or incomplete argument: " + argument);
            }
        }

        if (format != "text" && format != "json") {
            throw runtime_error("Unsupported output format: " + format + ". Use text or json.");
        }
        if (compareMode && !algorithm.empty()) {
            throw runtime_error("--compare cannot be combined with --algorithm.");
        }

        if (inputFile.empty()) {
            cout << "Enter input file name: ";
            cin >> inputFile;
        }

        if (!compareMode && algorithm.empty()) {
            cout << "Choose scheduling method:" << endl;
            cout << "1. First Come, First Served" << endl;
            cout << "2. Shortest Job First" << endl;
            cout << "3. Priority Scheduling" << endl;
            cout << "4. Shortest Remaining Time First" << endl;
            cout << "5. Round Robin" << endl;
            cout << "6. No Scheduling" << endl;
            cout << "Option: ";
            cin >> algorithm;
            algorithm = normalizeAlgorithmName(algorithm);
        }

        if ((compareMode || algorithm == "rr") && quantum <= 0 && !quantumProvided) {
            cout << "Enter Round Robin quantum: ";
            cin >> quantum;
        }

        vector<Process> processes = readProcessesFromFile(inputFile);

        if (compareMode) {
            vector<ScheduleResult> results = runAllAlgorithms(processes, quantum);
            writeComparisonReport(results, format, cout);

            if (!outputFile.empty()) {
                ofstream output(outputFile.c_str());
                if (!output.is_open()) {
                    throw runtime_error("Failed to open output file: " + outputFile);
                }
                writeComparisonReport(results, format, output);
            }

            return 0;
        }

        ScheduleResult result = runSelectedAlgorithm(processes, algorithm, quantum);
        writeReport(result, format, cout);

        if (!outputFile.empty()) {
            ofstream output(outputFile.c_str());
            if (!output.is_open()) {
                throw runtime_error("Failed to open output file: " + outputFile);
            }
            writeReport(result, format, output);
        }
        return 0;
    } catch (const exception& error) {
        cerr << "Error: " << error.what() << endl;
        return 1;
    }
}
