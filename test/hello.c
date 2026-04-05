#include <stdio.h>
#include <string.h>

int add(int a, int b) {
    return a + b;
}

int main(int argc, char* argv[]) {
    const char* greeting = "Hello, x64lldbg!";
    printf("%s\n", greeting);

    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum = add(sum, i);
    }

    printf("Sum: %d\n", sum);

    char buffer[64];
    strcpy(buffer, "Test string in buffer");
    printf("%s\n", buffer);

    return 0;
}
