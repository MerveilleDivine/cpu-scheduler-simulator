#include "Parser.h"
#include "Reporter.h"
#include "Scheduler.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string trim(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    return value.substr(start, value.find_last_not_of(whitespace) - start + 1);
}

std::string toLower(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    }
    return value;
}

int parseIntegerArgument(const std::string& input, const std::string& optionName) {
    const std::string value = trim(input);
    std::size_t parsedCharacters = 0;
    try {
        const int parsed = std::stoi(value, &parsedCharacters);
        if (parsedCharacters == value.size()) {
            return parsed;
        }
    } catch (const std::invalid_argument&) {
    } catch (const std::out_of_range&) {
    }
    throw std::runtime_error(optionName + " expects an integer, received: " + value);
}

std::string requireOptionValue(int& index, int argc, char** argv) {
    const std::string option = argv[index];
    if (index + 1 >= argc || std::string(argv[index + 1]).empty() || argv[index + 1][0] == '-') {
        // A negative quantum is a value; its range is checked after parsing.
        if ((option == "-q" || option == "--quantum") && index + 1 < argc &&
            argv[index + 1][0] == '-' && argv[index + 1][1] >= '0' && argv[index + 1][1] <= '9') {
            return argv[++index];
        }
        throw std::runtime_error("Missing value for " + option);
    }
    return argv[++index];
}

std::string readInteractiveValue(const std::string& prompt, const std::string& fieldName) {
    std::cerr << prompt << std::flush;
    std::string value;
    if (!std::getline(std::cin, value)) {
        throw std::runtime_error("Failed to read " + fieldName + " from standard input.");
    }

    value = trim(value);
    if (value.empty()) {
        throw std::runtime_error("No value provided for " + fieldName + ".");
    }
    return value;
}

void printHelp() {
    std::cout << "CPU Scheduler Simulator\n"
              << "Usage:\n"
              << "  ./scheduler -f input.txt -o output.txt --algorithm fcfs\n"
              << "  ./scheduler -f input.txt -o output.txt --algorithm rr --quantum 2\n"
              << "  ./scheduler -f input.txt --algorithm srtf --format json\n"
              << "  ./scheduler -f input.txt --compare --quantum 2\n\n"
              << "Options:\n"
              << "  -f, --input FILE        Process input file\n"
              << "  -o, --output FILE       Also save the report to a file\n"
              << "  -a, --algorithm NAME    none, fcfs, sjf, srtf, priority, rr\n"
              << "  -q, --quantum INTEGER   Positive Round Robin time quantum\n"
              << "      --compare           Compare the five scheduling algorithms\n"
              << "      --format FORMAT     text (default) or json\n"
              << "  -h, --help              Show this help\n";
}

std::string normalizeAlgorithmName(const std::string& input) {
    const std::string value = toLower(input);
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
    throw std::runtime_error("Unsupported scheduling algorithm: " + input);
}

ScheduleResult runSelectedAlgorithm(const std::vector<Process>& processes,
                                    const std::string& algorithm, int quantum) {
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
    return runNoScheduling(processes);
}

template <typename ReportWriter>
void writeOutputs(const std::string& outputFile, const ReportWriter& writeReport) {
    writeReport(std::cout);
    std::cout.flush();
    if (!std::cout) {
        throw std::runtime_error("Failed to write report to standard output.");
    }

    if (!outputFile.empty()) {
        std::ofstream output(outputFile.c_str());
        if (!output.is_open()) {
            throw std::runtime_error("Failed to open output file: " + outputFile);
        }
        writeReport(output);
        output.close();
        if (!output) {
            throw std::runtime_error("Failed to write output file: " + outputFile);
        }
    }
}
}

int main(int argc, char** argv) {
    try {
        std::string inputFile;
        std::string outputFile;
        std::string algorithm;
        std::string format = "text";
        int quantum = 0;
        bool compareMode = false;
        bool quantumProvided = false;

        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help" || argument == "-h") {
                printHelp();
                std::cout.flush();
                return std::cout ? 0 : 1;
            } else if (argument == "-f" || argument == "--input") {
                inputFile = requireOptionValue(i, argc, argv);
            } else if (argument == "-o" || argument == "--output") {
                outputFile = requireOptionValue(i, argc, argv);
            } else if (argument == "-a" || argument == "--algorithm") {
                algorithm = normalizeAlgorithmName(requireOptionValue(i, argc, argv));
            } else if (argument == "-q" || argument == "--quantum") {
                quantum = parseIntegerArgument(requireOptionValue(i, argc, argv), "--quantum");
                quantumProvided = true;
            } else if (argument == "--format") {
                format = toLower(requireOptionValue(i, argc, argv));
            } else if (argument == "--compare") {
                compareMode = true;
            } else {
                throw std::runtime_error("Unknown argument: " + argument);
            }
        }

        if (format != "text" && format != "json") {
            throw std::runtime_error("Unsupported output format: " + format + ". Use text or json.");
        }
        if (compareMode && !algorithm.empty()) {
            throw std::runtime_error("--compare cannot be combined with --algorithm.");
        }
        if (quantumProvided && quantum <= 0) {
            throw std::runtime_error("Round Robin quantum must be greater than 0.");
        }

        if (inputFile.empty()) {
            inputFile = readInteractiveValue("Enter input file name: ", "input file name");
        }
        if (!compareMode && algorithm.empty()) {
            algorithm = normalizeAlgorithmName(readInteractiveValue(
                "Choose scheduling method:\n"
                "1. First Come, First Served\n"
                "2. Shortest Job First\n"
                "3. Priority Scheduling\n"
                "4. Shortest Remaining Time First\n"
                "5. Round Robin\n"
                "6. No Scheduling\n"
                "Option: ", "scheduling method"));
        }
        if ((compareMode || algorithm == "rr") && !quantumProvided) {
            quantum = parseIntegerArgument(
                readInteractiveValue("Enter Round Robin quantum: ", "Round Robin quantum"), "--quantum");
            if (quantum <= 0) {
                throw std::runtime_error("Round Robin quantum must be greater than 0.");
            }
        }

        const std::vector<Process> processes = readProcessesFromFile(inputFile);
        if (compareMode) {
            const std::vector<ScheduleResult> results = runAllAlgorithms(processes, quantum);
            writeOutputs(outputFile, [&](std::ostream& output) {
                if (format == "json") {
                    printJsonComparisonReport(results, output);
                } else {
                    printComparisonReport(results, output);
                }
            });
        } else {
            const ScheduleResult result = runSelectedAlgorithm(processes, algorithm, quantum);
            writeOutputs(outputFile, [&](std::ostream& output) {
                if (format == "json") {
                    printJsonReport(result, output);
                } else {
                    printReport(result, output);
                }
            });
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
