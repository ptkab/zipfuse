#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <zip.h>
#include <fuse.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h> 

zip_t* archive;

// this function is called by fuse everytime before any other function call
// so this needs to be the first function that should be implemented
static int zipfuse_getattr(const char *path, struct stat *stbuf)
			 //struct fuse_file_info *fi)
{
    // (void) fi;
	// int res = 0;

    //declare and initialize zip stat buffer using libzip 
    zip_stat_t sfile;
    zip_stat_init(&sfile);

    // check if path is root. Set file stats nlink as 2 and
    // mode masked as S_IFDIR masked with 0755, as given in hello.c file
    // of libfuse documentation.
    // else check if the zip file path is dir or file. 
    if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
    } else {
        
        // paths do not have an ending slash. we need to add it ourselves to check
        // if they are directories.
        int path_length = strlen(path);
        char* path_with_slash = malloc(path_length + 3);
        strcpy(path_with_slash, path);
        path_with_slash[path_length] = '/';
        path_with_slash[path_length + 1] = 0;

        //check if path is actually a directory. If yes, then set file stats
        // of a directory in stbuf
        if (zip_name_locate(archive, path_with_slash, 0) != -1) {
            zip_stat(archive, path_with_slash, 0, &sfile);
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            stbuf->st_size = 0;
            stbuf->st_mtime = sfile.mtime;
        //if not, check if path is a regular file. Then set file stats of file
        // in stbuf
        } else if (zip_name_locate(archive, path + 1, 0) != -1){
            zip_stat(archive, path + 1, 0, &sfile);
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = sfile.size;
            stbuf->st_mtime = sfile.mtime;

        // if all fails, that means we dont have permission to this file or it
        // is invalid. Return ENOENT in this case. 
        //(reference hello.c example from libfuse documentation)
        } else {
            return -ENOENT;
        }
    }

	return 0;
}

static int zipfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	// (void) offset;
	// (void) fi;
	// (void) flags;

	// if (strcmp(path, "/") != 0)
	// 	return -ENOENT;

	// filler(buf, ".", NULL, 0, 0);
	// filler(buf, "..", NULL, 0, 0);
	// filler(buf, options.filename, NULL, 0, 0);

	return 0;
}

static int zipfuse_open(const char *path, struct fuse_file_info *fi)
{
    // check if file present by tring to open it
    // zip_fopen(archive, path, 0);
    // zip_name_locate(archive, path + 1, 0)
    
    // if name_locate returns index anything but -1, means file can be opened.
    if (zip_name_locate(archive, path + 1, 0) != 1) {
        return 0;
    }

	return -ENOENT;
}

static int zipfuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	// (void) fi;

    // get file descriptor of file at current path
    zip_file_t* desc = zip_fopen(archive, path + 1, 0);
    //if desc is not valid, then return with ENOENT
    if (!desc) {
        return -ENOENT;
    }

    // initialize zip file stats buffer like we did in get_attr function
    // and fill it with zip_stat() . We need size from sfile to set buffer size.
    zip_stat_t sfile;
    zip_stat_init(&sfile);
    zip_stat(archive, path + 1, 0, &sfile);

    // read bytes from the 'offset' till 'size' in a buffer.
    // Total size is offset + size + the size returned by stat buffer.
    char buffer[offset + size + sfile.size];

    //read data in our buffer. check if data is read properly else return ENOENT
    if (zip_fread(desc, buffer, sfile.size) == -1) {
        return -ENOENT;
    }

    // copy the data into 'buf'
    memcpy(buf, buffer + offset, size);
    zip_fclose(desc);

    // I dont know why 'size' is returned. It was in hello.c file
    // so I have left it as it is.
    return size;
}

// override the fuse operations with our custom functions to operate on zip file
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
    archive = zip_open(archive_name, 0, NULL);
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