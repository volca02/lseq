#include <iostream>

#include "lseq.h"

int main() {
    try {
        LSeq s;
        s.run();
    } catch (const std::exception &e) {
        std::cerr << "Terminating with an error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
