#include "Parser.h"

#include <fstream>
#include <limits>
#include <sstream>
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
    const std::size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

int parseInteger(const std::string& token, const std::string& fieldName, std::size_t lineNumber) {
    const std::string value = trim(token);
    std::size_t parsedCharacters = 0;
    try {
        const int parsed = std::stoi(value, &parsedCharacters);
        if (parsedCharacters == value.size()) {
            return parsed;
        }
    } catch (const std::invalid_argument&) {
    } catch (const std::out_of_range&) {
    }

    std::ostringstream error;
    error << "Invalid " << fieldName << " on line " << lineNumber
          << ": expected an integer, received '" << value << "'";
    throw std::runtime_error(error.str());
}
}

std::vector<Process> readProcessesFromFile(const std::string& inputFile) {
    std::ifstream input(inputFile.c_str());
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open input file: " + inputFile);
    }

    std::vector<Process> processes;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::size_t commentStart = line.find('#');
        if (commentStart != std::string::npos) {
            line.erase(commentStart);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::size_t firstSeparator = line.find(':');
        const std::size_t secondSeparator = firstSeparator == std::string::npos
            ? std::string::npos : line.find(':', firstSeparator + 1);
        if (secondSeparator == std::string::npos || line.find(':', secondSeparator + 1) != std::string::npos) {
            std::ostringstream error;
            error << "Invalid input on line " << lineNumber
                  << ": expected burst_time:arrival_time:priority";
            throw std::runtime_error(error.str());
        }
        if (processes.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("Too many processes: process IDs exceed the supported integer range.");
        }

        Process process;
        process.id = static_cast<int>(processes.size() + 1);
        process.burstTime = parseInteger(line.substr(0, firstSeparator), "burst time", lineNumber);
        process.arrivalTime = parseInteger(line.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1),
                                           "arrival time", lineNumber);
        process.priority = parseInteger(line.substr(secondSeparator + 1), "priority", lineNumber);
        process.remainingTime = process.burstTime;

        if (process.burstTime <= 0) {
            std::ostringstream error;
            error << "Invalid burst time on line " << lineNumber << ": value must be greater than 0";
            throw std::runtime_error(error.str());
        }
        if (process.arrivalTime < 0) {
            std::ostringstream error;
            error << "Invalid arrival time on line " << lineNumber << ": value cannot be negative";
            throw std::runtime_error(error.str());
        }
        processes.push_back(process);
    }

    if (input.bad() || (input.fail() && !input.eof())) {
        throw std::runtime_error("Failed to read input file: " + inputFile);
    }
    if (processes.empty()) {
        throw std::runtime_error("Input file does not contain any processes.");
    }
    return processes;
}
