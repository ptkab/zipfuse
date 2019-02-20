#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <zip.h>
#include <fuse.h>

static int zipfuse_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	return 0;
}

static int zipfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	return 0;
}

static int zipfuse_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int zipfuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	return 0;
}

static struct fuse_operations zipfuse_oper = {
	.getattr    = zipfuse_getattr,
	.readdir    = zipfuse_readdir,
	.open       = zipfuse_open,
	.read       = zipfuse_read,
};

int main(int argc, char *argv[]) {

    //if the program has less than 3 arguments,  it means user has not
    //passed all required argument to program. In that case, terminate.
    if (argc < 3) {
        printf("\nThis program requires 2 arguments to work: \n");
        printf("    1. Zip archive\n");
        printf("    2. Mount point directory\n");
        printf("usage: ./zipfuse <file.zip> <mount_point>\n\n");
        exit(1);
    }

    //get the name of the archive which is the first argment on command line
    //open the archive wiht zip_open function and validate.
    //if zip_open fails, it means user has passed an invalid or non-zip file.
    //in that case, terminate program. 
    char* archive_name = argv[1];
    zip_t* archive = zip_open(archive_name, 0, NULL);
    if (!archive) {
        printf("\nFailed to open archive. Either archive is invalid "); 
        printf("or not a ZIP file\n\n");
        exit(1);
    }

    //make new arguments array to pass to fuse_main, because fuse_main only
    //understands <filename> <mountpoint> arguments. We have a middle argument
    //where we enter zip file. We need to remove that.
    char* args[2];
    args[0] = argv[0];
    args[1] = argv[2];

    //call fuse_main with custom fuse_operations struct we created above.
    return fuse_main(argc - 1, args, &zipfuse_oper, NULL);
}