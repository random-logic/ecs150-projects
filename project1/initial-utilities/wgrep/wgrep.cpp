#include <string>
#include <iostream>
#include <vector>

#include <fcntl.h> // for open
#include <unistd.h> // for write, read, close

using namespace std;

void searchLine(string & line, string & searchTerm) {
    size_t pos = line.find(searchTerm);

    if (pos != string::npos) {
        cout << line << endl;
    }
}

void searchLine(vector<char> & line, string & searchTerm) {
    string content(line.begin(), line.end());

    searchLine(content, searchTerm);

    line.clear();
}

void searchInput(string & searchTerm) {
    string line;
    
    while (getline(cin, line)) {
        searchLine(line, searchTerm);
    }
}

void searchFile(int fileDescriptor, string & searchTerm) {
    int bufferSize = 8192;
    char buffer[bufferSize + 1];
    int bytesRead;

    vector<char> line;

    while ((bytesRead = read(fileDescriptor, buffer, bufferSize)) > 0) {
        buffer[bytesRead] = '\0'; // ensure null termination

        for (int i = 0; i < bufferSize; ++i) {
            if (buffer[i] == '\0') {
                break;
            }
            else if (buffer[i] == '\n') {
                searchLine(line, searchTerm);
            }
            else {
                line.push_back(buffer[i]);
            }
        }
    }

    // In case last line has no trailing \n
    searchLine(line, searchTerm);
}

// g++ -o wgrep wgrep.cpp -Wall -Werror
// For this project, you are required to use the following routines to do file input and output: open, read, write, and close.
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "wgrep: searchterm [file ...]" << endl;
        return 1;
    }

    string searchTerm(argv[1]);

    if (argc == 2) {
        searchInput(searchTerm);
    }
    else {
        for (int i = 2; i < argc; ++i) {
            const char* fileName = argv[i];
            int fileDescriptor = open(fileName, O_RDONLY);

            if (fileDescriptor < 0) {
                cout << "wgrep: cannot open file" << endl;
                return 1;
            }
            else {
                searchFile(fileDescriptor, searchTerm);
            }
        }
    }

    return 0;
}