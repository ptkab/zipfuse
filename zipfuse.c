#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <zip.h>
#include <fuse.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>

zip_t *archive;

enum type
{
    DIRECTORY = 1,
    FILES = 2
};

static char* addslash(const char* path)
{
    int length = strlen(path);
    char *temp_str = malloc(length + 2);
    memcpy((void*) temp_str, (const void*) path, sizeof(char)* length);
    temp_str[length] ='/';
    temp_str[length + 1] = '\0';
    return temp_str;
}

static int zipfuse_open(const char *path, struct fuse_file_info *fi)
{
    (void) fi;
    // if the file in zip cannot be located by name, then -1 is returned.
    // in that case, return with error.
    if(zip_name_locate(archive, path + 1, 0) < 0) {
        return -ENOENT;
    }
    return 0;
}

static void zipfuse_destroy(void* private_data)
{
    (void) private_data;
    zip_close(archive);
}

static enum type get_file_type(const char* path)
{
    // check if the path is root, if yes, return type DIRECTORY
    if (strcmp(path, "/") == 0) {
        return DIRECTORY;
    }

    char *full_path = addslash(path + 1);
    if (zip_name_locate(archive, full_path, 0) != -1)
        return DIRECTORY;
    else if (zip_name_locate(archive, path + 1, 0) != -1)
        return FILES;
    else
        return -1;
}

static int zipfuse_getattr(const char *path, struct stat *stbuf)
{

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 1;
        return 0;
    }

    char* path_slash = addslash(path + 1);
    enum type filetype = get_file_type(path);

    zip_stat_t stat_buffer;
    zip_stat_init(&stat_buffer);

    //stbuf stats modified with reference to hello.c from libfuse documentation
    if (filetype == DIRECTORY) {
        zip_stat(archive, path_slash, 0, &stat_buffer);
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_size = 0;
        stbuf->st_nlink = 2;
        stbuf->st_mtime = stat_buffer.mtime;
    } else if (filetype == FILES) {
        zip_stat(archive, path + 1, 0, &stat_buffer);
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_size = stat_buffer.size;
        stbuf->st_nlink = 1;
        stbuf->st_mtime = stat_buffer.mtime;
    } else {
        return -ENOENT;
    }

    return 0;
}

static int zipfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

    (void) offset;
    (void) fi;

    // add current and parent directories to the buf as done in hello.c
    // of libfuse documentation
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    int enteries = zip_get_num_entries(archive, 0);
    for (int i = 0; i < enteries; i++) {
        
        zip_stat_t stat_buffer;
        zip_stat_index(archive, i, 0, &stat_buffer);

        // pre-pend slah to the current entery at index i
        // this is required for the dirname function from libgen library
        char* prepended_path = malloc(strlen(stat_buffer.name) + 2);
        *prepended_path = '/';
        strcpy(prepended_path + 1, stat_buffer.name);

        char* base_name = strdup(prepended_path);

        struct stat st;
        memset(&st, 0, sizeof(st));

        char* parent_name = strdup(prepended_path);
        if (strcmp(path, dirname(parent_name)) == 0)
        {
            // remove the ending slash if present and terinate the string.
            if (prepended_path[strlen(prepended_path) - 1] == '/') {
                prepended_path[strlen(prepended_path) - 1] = '\0';
            }

            zipfuse_getattr(prepended_path, &st);
            char* name = basename(base_name);
            if (filler(buf, name, &st, 0))
                break;
        }

        free(base_name);
        free(parent_name);
        free(prepended_path);
    }

    return 0;
}

static int zipfuse_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info* fi)
{
    (void) fi;

    // initialize the stat_buffer with current path file's stats
    zip_stat_t stat_buffer;
    zip_stat_init(&stat_buffer);
    zip_stat(archive, path + 1, 0, &stat_buffer);

    // get the current file's descriptor
    zip_file_t* desc = zip_fopen(archive, path + 1, 0);

    int total_length = stat_buffer.size + size + offset;
    char temp[total_length];
    memset(temp, 0, total_length);

    // if we cannot read the file then return with ENOENT error.
    if (zip_fread(desc, temp, stat_buffer.size) == -1)
        return -ENOENT;

    memcpy(buf, temp + offset, size);
    zip_fclose(desc);

    return size;
}

static int zipfuse_write(const char* path, const char *buf, size_t size, 
                        off_t offset, struct fuse_file_info* fi)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -EROFS;
}

static int zipfuse_mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void) mode;
    return -EROFS;
}

static int zipfuse_unlink(const char* path)
{
    (void)path;
    return -EROFS;
}

static int zipfuse_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}

static int zipfuse_access(const char* path, int mask)
{
    (void) mask;
    if (get_file_type(path) >= 0)
        return 0;
    return -ENOENT;
}

static int zipfuse_statfs(const char* path, struct statvfs* stbuf){

    // if stat for the file at current path cannot be found, then return error.
    if (statvfs(path, stbuf) == -1)
    return -errno;
    return 0;
}

// override the fuse operations with our custom functions to operate on zip file
static struct fuse_operations zipfuse_oper = {
	.open       = zipfuse_open,
    .destroy    = zipfuse_destroy,
    .getattr    = zipfuse_getattr,
	.readdir    = zipfuse_readdir,
	.read       = zipfuse_read,
    .write      = zipfuse_write,
    .mkdir      = zipfuse_mkdir,
    .unlink     = zipfuse_unlink,
    .rmdir      = zipfuse_rmdir,
	.access     = zipfuse_access,
    .statfs     = zipfuse_statfs
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