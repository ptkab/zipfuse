#include <stdio.h>
#include <stdlib.h>
#include <zip.h>

int main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("\nThis program requires 2 arguments to work: \n");
        printf("    1. Zip archive\n");
        printf("    2. Mount point directory\n");
        printf("usage: ./zipfuse <file.zip> <mount_point>\n\n");
        exit(1);
    }

    char* archive = argv[1];
    printf("\n");
    printf("%s", archive);
    printf("\n");

    return 0;
}