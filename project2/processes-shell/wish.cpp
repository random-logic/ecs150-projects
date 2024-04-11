#include <iostream>
#include <istream>
#include <fstream>
#include <unistd.h> // for getpid()
#include <memory> // for unique_ptr

using namespace std;

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
    
    while (true) {
        cout << "wish> ";

        string input;
        getline(*in, input);

        if (input == "exit") {
            break;
        }
    }

    return 0;
}