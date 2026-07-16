#include "Parser.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
string trim(const string& value) {
    const string whitespace = " \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == string::npos) {
        return "";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

vector<string> split(const string& line, char delimiter) {
    vector<string> tokens;
    string token;
    stringstream ss(line);

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

int parseInteger(const string& token, const string& fieldName, int lineNumber) {
    const string value = trim(token);
    size_t parsedCharacters = 0;

    try {
        const long long parsed = stoll(value, &parsedCharacters);
        if (value.empty() || parsedCharacters != value.size() ||
            parsed < numeric_limits<int>::min() || parsed > numeric_limits<int>::max()) {
            throw invalid_argument("invalid integer");
        }
        return static_cast<int>(parsed);
    } catch (const exception&) {
        stringstream error;
        error << "Invalid " << fieldName << " on line " << lineNumber
              << ": expected an integer, received '" << value << "'";
        throw runtime_error(error.str());
    }
}
}

vector<Process> readProcessesFromFile(const string& inputFile) {
    ifstream input(inputFile.c_str());

    if (!input.is_open()) {
        throw runtime_error("Failed to open input file: " + inputFile);
    }

    vector<Process> processes;
    string line;
    int lineNumber = 0;
    int processId = 1;

    while (getline(input, line)) {
        ++lineNumber;

        const size_t commentStart = line.find('#');
        if (commentStart != string::npos) {
            line = line.substr(0, commentStart);
        }
        line = trim(line);

        if (line.empty()) {
            continue;
        }

        vector<string> tokens = split(line, ':');
        if (tokens.size() != 3) {
            stringstream error;
            error << "Invalid input on line " << lineNumber
                  << ": expected burst_time:arrival_time:priority";
            throw runtime_error(error.str());
        }

        Process process;
        process.id = processId++;
        process.burstTime = parseInteger(tokens[0], "burst time", lineNumber);
        process.arrivalTime = parseInteger(tokens[1], "arrival time", lineNumber);
        process.priority = parseInteger(tokens[2], "priority", lineNumber);
        process.remainingTime = process.burstTime;

        if (process.burstTime <= 0) {
            stringstream error;
            error << "Invalid burst time on line " << lineNumber << ": value must be greater than 0";
            throw runtime_error(error.str());
        }
        if (process.arrivalTime < 0) {
            stringstream error;
            error << "Invalid arrival time on line " << lineNumber << ": value cannot be negative";
            throw runtime_error(error.str());
        }

        processes.push_back(process);
    }

    if (processes.empty()) {
        throw runtime_error("Input file does not contain any processes.");
    }

    return processes;
}
