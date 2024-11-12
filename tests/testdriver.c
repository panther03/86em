#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    opterr = 0;
    int stopearly = 0;
    int c;

    while ((c = getopt(argc, argv, "s")) != -1 ) {
        switch (c) {
            case 's': stopearly = 1; break;
            default: 
                abort();
        }
    }
}