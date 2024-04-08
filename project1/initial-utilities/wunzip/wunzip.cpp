#include <iostream>

#include <cstdint> // 4 byte int
#include <fcntl.h> // for open
#include <unistd.h> // for write, read, close

using namespace std;

void unzipFile(int fileDescriptor) {
    const int BUFFER_SIZE = 5;
    const int LETTER_POS = 4;
    unsigned char buffer[BUFFER_SIZE];

    while (read(fileDescriptor, buffer, BUFFER_SIZE) > 0) {
        int count = *((int32_t*) buffer);
        char letter = buffer[LETTER_POS];
        for (int i = 0; i < count; ++i) {
            cout << letter;
        }
    }
}

// g++ -o wunzip wunzip.cpp -Wall -Werror
// For this project, you are required to use the following routines to do file input and output: open, read, write, and close.
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "wunzip: file1 [file2 ...]" << endl;
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        const char* fileName = argv[i];
        int fileDescriptor = open(fileName, O_RDONLY);

        if (fileDescriptor < 0) {
            cout << "wunzip: cannot open file" << endl;
            return 1;
        }

        unzipFile(fileDescriptor);
    }

    return 0;
}