#include <iostream>

#include <fcntl.h> // for open
#include <unistd.h> // for write, read, close

using namespace std;

// g++ -o wcat wcat.cpp -Wall -Werror
// For this project, you are required to use the following routines to do file input and output: open, read, write, and close.
int main(int argc, char* argv[]) { 
    for (int i = 1; i < argc; ++i) {
        const char* fileName = argv[i];
        int fileDescriptor = open(fileName, O_RDONLY);

        if (fileDescriptor < 0) {
            cout << "wcat: cannot open file" << endl;
            return 1;
        }
        else {
            int bufferSize = 8192;
            char buffer[bufferSize];
            int bytesRead;
            while ((bytesRead = read(fileDescriptor, buffer, bufferSize)) > 0) {
                write(STDOUT_FILENO, &buffer, sizeof(char) * bytesRead);
            }
        }

        close(fileDescriptor);
    }

    return 0;
}