#include <iostream>

#include <cstdint> // 4 byte int
#include <fcntl.h> // for open
#include <unistd.h> // for write, read, close

using namespace std;

void output(char letter, int32_t count) {
    write(STDOUT_FILENO, (void*) &count, sizeof(int32_t));
    write(STDOUT_FILENO, (void*) &letter, sizeof(char));
}

// g++ -o wzip wzip.cpp -Wall -Werror
// For this project, you are required to use the following routines to do file input and output: open, read, write, and close.
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "wzip: file1 [file2 ...]" << endl;
        return 1;
    }

    const int BUFFER_SIZE = 1;
    
    int32_t count = 0;
    char currChar = '\0';
    char buffer[BUFFER_SIZE];

    for (int i = 1; i < argc; ++i) {
        const char* fileName = argv[i];
        int fileDescriptor = open(fileName, O_RDONLY);

        if (fileDescriptor < 0) {
            cout << "wzip: cannot open file" << endl;
            close(fileDescriptor);
            return 1;
        }

        while (read(fileDescriptor, buffer, BUFFER_SIZE) > 0) {
            char letter = buffer[0];
            
            if (letter == currChar) {
                ++count;
            }
            else {
                if (currChar != '\0') output(currChar, count);
                count = 1;
                currChar = letter;
            }
        }

        close(fileDescriptor);
    }

    output(currChar, count);

    return 0;
}