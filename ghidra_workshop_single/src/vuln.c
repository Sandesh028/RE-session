#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void secret() {
    puts("Congratulations! You reached secret().");
    /* typically you would see a 'flag' printed here in CTFs */
}

void vuln(const char *data) {
    char buf[64];
    /* intentionally unsafe copy */
    strcpy(buf, data);
    printf("You said: %.40s\n", buf);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <input>\n", argv[0]);
        return 1;
    }
    vuln(argv[1]);
    puts("Done.");
    return 0;
}
