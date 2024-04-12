#include <iostream> // for io
#include <istream> // for base in class
#include <fstream> // for ifstream
#include <sstream> // for istringstream
#include <unistd.h> // for getpid()
#include <memory> // for unique_ptr
#include <vector>

using namespace std;

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
    
    if (argc == 1) {
        // Use cin
        in = make_unique<istream>(cin.rdbuf());
    }
    else if (argc == 2) {
        // Use fin
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

    vector<string> searchPaths;
    
    while (true) {
        cout << "wish> ";

        string line;
        getline(*in, line);

        istringstream iss(line);
        vector<string> inputs = split(line, '&');

        for (string input : inputs) {
            vector<string> args = split(input, ' ');

            if (args.empty()) {
                continue;
            }

            vector<string>::size_type count = args.size();
            string cmd = args[0];

            if (cmd == "exit") {
                break;
            }
            else if (cmd == "cd") {
                if (count != 2 || chdir(args[1].c_str())) {
                    printErr();
                }
            }
            else if (cmd == "path") {
                searchPaths.clear();
                for (int i = 1; i < count; ++i) {
                    searchPaths.push_back(args[i]);
                }
            }
            else {
                // ??
            }
        }
    }

    return 0;
}