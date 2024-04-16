#include <iostream> // for io
#include <istream> // for base in class
#include <fstream> // for ifstream
#include <sstream> // for istringstream
#include <unistd.h> // for getpid()
#include <sys/wait.h> // for wait()
#include <memory> // for unique_ptr
#include <vector>
#include <cstring>

#include <error.h>

using namespace std;

char** vectorOfStringToCharArray(const std::vector<std::string>& vec) {
    // Allocate memory for the array of char pointers
    char** charArray = new char*[vec.size()];

    // Populate the array with dynamically allocated char arrays
    for (size_t i = 0; i < vec.size(); ++i) {
        charArray[i] = new char[vec[i].size() + 1]; // +1 for null terminator
        strcpy(charArray[i], vec[i].c_str());
    }

    return charArray;
}

// Function to deallocate memory allocated for char**
void deallocateCharArray(char** charArray, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        delete[] charArray[i];
    }
    delete[] charArray;
}

string trim(const string& str) {
    size_t start = 0;
    size_t end = str.length();

    // Find the first non-whitespace character
    while (start < end && isspace(str[start])) {
        start++;
    }

    // Find the last non-whitespace character
    while (end > start && isspace(str[end - 1])) {
        end--;
    }

    // Return the trimmed string
    return str.substr(start, end - start);
}

vector<string> split(const string& str, const char sep) {
    istringstream iss(str);
    vector<string> res;
    string section;

    while (getline(iss, section, sep)) {
        section = trim(section);
        res.push_back(section);
    }

    return res;
}

void printErr() {
    cerr << "An error has occurred\n";
}

// g++ -o wish wish.cpp -Wall -Werror
int main(int argc, char* argv[]) {
    unique_ptr<istream> in;
    bool readFromFile = false;
    
    if (argc == 1) {
        // Use cin
        in = make_unique<istream>(cin.rdbuf());
    }
    else if (argc == 2) {
        // Use fin
        readFromFile = true;
        in = make_unique<ifstream>(argv[1]);

        if (!(*in)) {
            printErr();
            return 1;
        }
    }
    else {
        printErr();
        return 1;
    }

    vector<string> searchPaths = { "/bin" };
    
    while (true) {
        string line;
        
        if (!getline(*in, line) && readFromFile) {
            return 0;
        }

        istringstream iss(line);
        vector<string> inputs = split(line, '&');

        for (string input : inputs) {
            vector<string> args = split(input, ' ');

            if (args.empty()) {
                continue;
            }

            int count = args.size();
            string cmd = args[0];

            if (cmd == "exit") {
                if (count == 1) {
                    return 0;
                }
                else {
                    printErr();
                }
            }
            else if (cmd == "cd") {
                if (count != 2 || chdir(args[1].c_str())) {
                    printErr();
                }
            }
            else if (cmd == "path") {
                searchPaths.clear();
                searchPaths = vector<string>(args.begin() + 1, args.end());
            }
            else {
                int ret = fork();

                if (ret == 0) {
                    // This is the child process
                    for (string path : searchPaths) {
                        // Check if program exists in path
                        if (access((path + "/" + cmd).c_str(), X_OK)) {
                            // Cannot access file
                            continue;
                        }

                        char** args_exec = vectorOfStringToCharArray(args);

                        execv((path + "/" + cmd).c_str(), args_exec);

                        // execv returned -1, which is an error
                        printErr();

                        deallocateCharArray(args_exec, count);
                        return 0;
                    }

                    // Could not execute within path, also an error
                    printErr();
                    return 0;
                }
                else {
                    // This is the parent process
                    wait(NULL);
                }
            }
        }
    }

    return 0;
}