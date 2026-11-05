#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    const char *msg = "Hello from ARM!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    printf("ARM program executing...\n");
    return 0;
}
