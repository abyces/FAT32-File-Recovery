//
// Created by apple on 11/19/21.
//

#include "nyufile.h"

char* opt_error = "Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.\n";

void* open_filesys(char* disk_file);

int main(int argc, char** argv) {
    int i_flag = 0, l_flag = 0, r_flag = 0, R_flag = 0, s_flag = 0;
    char* r_val_cont = NULL;
    char* R_val_non_cont = NULL;
    char* s_val = NULL;
    bootEntry = NULL;

    char* disk_file = argv[1];
    // TODO valid check

    int c;
    while ((c = getopt (argc, argv, ":ilr:R:s:")) != -1){
        switch (c)
        {
            case 'i':
                i_flag = 1;
                break;
            case 'l':
                l_flag = 1;
                break;
            case 'r':
                r_flag = 1;
                r_val_cont = optarg;
                break;
            case 'R':
                R_flag = 1;
                R_val_non_cont = optarg;
                break;
            case 's':
                s_flag = 1;
                s_val = optarg;
                break;
            default:
                printf("%s", opt_error);
                exit(-1);
        }
    }


    void* disk = NULL;
    if (!i_flag && !l_flag && !r_flag && !R_flag){
        printf("%s", opt_error);
    } else if (i_flag){
        if((l_flag || r_flag || R_flag || s_flag) || argc != 3){
            printf("%s", opt_error);
            exit(-1);
        }
        disk = open_filesys(disk_file);
        print_sysinfo();
    } else if (l_flag){
        if((r_flag || R_flag || s_flag) || argc != 3){
            printf("%s", opt_error);
            exit(-1);
        }
        disk = open_filesys(disk_file);
        print_rootinfo(disk);
    } else if (r_flag){
        if((s_flag && argc != 6) || (!s_flag && argc != 4)){
            printf("%s", opt_error);
            exit(-1);
        }
        disk = open_filesys(disk_file);
        recover_contiguous(disk, r_val_cont, s_val);
    } else {
        if(!s_flag || argc != 6){
            printf("%s", opt_error);
        } else {
            disk = open_filesys(disk_file);
            recover_non_contiguous(disk, R_val_non_cont, s_val);
        }
    }
}

// open the disk image, fill the bootEntry and other key parameters
void* open_filesys(char* disk_file){
    int fd = open(disk_file, O_RDWR);
    if(fd == -1){
        printf("%s", opt_error);
        exit(-1);
    }

    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    if(err < 0){
        printf("%s", opt_error);
        exit(-1);
    }

    void* filesys = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    // once open, we save the filesys info immediately and fill some important args
    bootEntry = (BootEntry*)filesys;
    FAT_Loc = bootEntry->BPB_RsvdSecCnt * bootEntry->BPB_BytsPerSec;
    FAT_Size = bootEntry->BPB_FATSz32 * bootEntry->BPB_BytsPerSec;
    Cluster_Size = bootEntry->BPB_SecPerClus * bootEntry->BPB_BytsPerSec;
    Cluster2_Loc = (bootEntry->BPB_RsvdSecCnt + bootEntry->BPB_FATSz32 * bootEntry->BPB_NumFATs) * bootEntry->BPB_BytsPerSec;
    Maximum_DirEntry_Num = Cluster_Size / sizeof (DirEntry);
    return filesys;
}
