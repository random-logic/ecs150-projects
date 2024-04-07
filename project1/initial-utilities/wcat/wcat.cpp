#include <iostream>
#include <string>

#include <fcntl.h> // for open
#include <unistd.h> // for write, read, close

using namespace std;

// g++ -o wcat wcat.cpp -Wall -Werror
// For this project, you are required to use the following routines to do file input and output: open, read, write, and close.
int main(int argc, char* argv[]) { 
    int errStatus = 0;

    for (int i = 1; i < argc; ++i) {
        const char* fileName = argv[i];
        int fileDescriptor = open(fileName, O_RDONLY);

        if (fileDescriptor == -1) {
            errStatus = 1;
            cout << "wcat: cannot open file";
        }
        else {
            int bufferSize = 4096;
            char buffer[bufferSize];
            while (read(fileDescriptor, buffer, bufferSize) > 0) {
                cout << buffer;
            }
        }

        cout << endl;
    }

    return errStatus;
}