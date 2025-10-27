#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    char hello[64];
    snprintf(hello, sizeof(hello), "Hello, %s!\n", (argc > 1) ? argv[1] : "world");
    puts(hello);
    return 0;
}
