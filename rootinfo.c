//
// Created by apple on 11/19/21.
//

#include "nyufile.h"

void print_rootinfo(void* disk){
    // loc for root_dir in fats
    // disk + reserved * sector_size + 2 (two no used cluster)
    int root_cluster_cnt = 1;
    unsigned int* roots = malloc(sizeof(unsigned int) * 100);
    unsigned int* fat_loc = (unsigned int*)((char*)disk + FAT_Loc) + bootEntry->BPB_RootClus;
    roots[root_cluster_cnt-1] = bootEntry->BPB_RootClus;
    while(*fat_loc > 0 && *fat_loc < 0xffffff8){
        root_cluster_cnt += 1;
        roots[root_cluster_cnt-1] = *fat_loc;
        fat_loc = (unsigned int*)((char*)disk + FAT_Loc) + (*fat_loc);
    }

    int existed_file_cnt = 0;
    for(int searched_cluster = 0; searched_cluster<root_cluster_cnt; searched_cluster++){
        // disk + (reserved + fats_num * sector_per_fats) * sector_size
        DirEntry* dirEntry = (DirEntry*)((char*)disk + Cluster2_Loc + Cluster_Size * (roots[searched_cluster] - 2));

        unsigned int searched_directory_cnt = 0;
        while(searched_directory_cnt < Maximum_DirEntry_Num){
            unsigned char *ptr = dirEntry->DIR_Name;

            // unallocated(deleted)
            if(*ptr == 0 || *ptr == 0xe5){
                searched_directory_cnt++;
                dirEntry++;
                continue;
            }

            unsigned char* dir_name = malloc(sizeof (unsigned char) * 13);
            unsigned char* p = dir_name;
            // 8 char, file name
            for(int i = 0; i<8; i++, ptr++){
                if(*ptr != 32){
                    *p = *ptr;
                    p++;
                }
            }
            if(dirEntry->DIR_Attr == 16){
                // dir
                *p = '/'; p++;
            } else if (dirEntry->DIR_Attr == 32){
                // file with extension
                for(int i = 0; i<3; i++, ptr++){
                    if(*ptr != 32){
                        if(i == 0){
                            *p = '.'; p++;
                        }
                        *p = *ptr;
                        p++;
                    }
                }
            }
            *p ='\0';

            printf("%s (size = %d, starting cluster = %u)\n", (char*)dir_name, dirEntry->DIR_FileSize, starting_cluster(dirEntry->DIR_FstClusHI, dirEntry->DIR_FstClusLO));

            searched_directory_cnt++;
            existed_file_cnt += 1;
            dirEntry++;
        }
    }

    printf("Total number of entries = %d\n", existed_file_cnt);
}


// combine hi && lo of starting cluster
unsigned int starting_cluster(unsigned short hi, unsigned short lo){
    return ((unsigned int)hi << 16) + lo;
}
