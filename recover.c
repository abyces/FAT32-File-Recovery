//
// Created by apple on 11/20/21.
//

#include "nyufile.h"

// recover contiguous file that can be more than one cluster [with SHA1 hash]
void recover_contiguous(void* disk, char* filename, char* sha1){
    char* filename_tail = filename; filename_tail++;
    int recover_cnt = 50;
    DirEntry** files_to_recover = malloc(sizeof (DirEntry*) * recover_cnt);
    int ambiguous_file_cnt = 0;

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
            if(*ptr == 0){
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

            if(*dir_name == 0xe5){
                dir_name++;
                if(strcmp((char*)dir_name, filename_tail) == 0){
                    if(ambiguous_file_cnt >= recover_cnt){
                        recover_cnt += 50;
                        DirEntry** tmp1 = (DirEntry**)realloc(files_to_recover, recover_cnt * sizeof (DirEntry*));
                        if (tmp1){
                            files_to_recover = tmp1;
                        } else {
                            exit(-1);
                        }
                    }
                    files_to_recover[ambiguous_file_cnt] = dirEntry;
                    ambiguous_file_cnt++;
                }
            }

            searched_directory_cnt++;
            existed_file_cnt++;
            dirEntry++;
        }
    }

    if(ambiguous_file_cnt > 1 && (sha1 == NULL || strlen(sha1) == 0))
        fprintf(stderr, "%s: multiple candidates found\n", filename);
    else{
        int found_and_recover = 0;
        for(int i = 0; i<ambiguous_file_cnt; i++){
            DirEntry *tmp_dirEntry = files_to_recover[i];
            // get first cluster and compute the total
            // cluster_num = (file_size + sector_size * ser_per_cluster - 1) / (sector_size * ser_per_cluster), for one cluster size = sector_size * ser_per_cluster
            unsigned int first_cluster = starting_cluster(tmp_dirEntry->DIR_FstClusHI, tmp_dirEntry->DIR_FstClusLO);
            unsigned int cluster_num = (int)((tmp_dirEntry->DIR_FileSize + Cluster_Size - 1) / Cluster_Size);

            // find first cluster
            // disk + reserved * sec_size + FATs * FAT_size * sec_size + (first - 2) * sec_per_clus * sec_size
            char* file_head_ptr = ((char*)disk + Cluster2_Loc + (first_cluster - 2) * Cluster_Size);

            int found = 0;
            // compute sha1
            if(sha1 != NULL && strlen(sha1) > 0){
                unsigned char* hash = malloc(sizeof(unsigned char) * SHA_DIGEST_LENGTH);
                SHA1((unsigned char*)file_head_ptr, tmp_dirEntry->DIR_FileSize, hash);
                char* hash_hex = (char*)malloc(sizeof (char) * 41);
                for(int hash_idx = 0; hash_idx<20; hash_idx++)
                    sprintf(hash_hex+2*hash_idx, "%.2x", hash[hash_idx]);
                hash_hex[40] = '\0';
                if(strcmp(hash_hex, sha1) == 0)
                    found = 1;
                free(hash);
                free(hash_hex);
            } else {
                found = 1;
            }

            if (found){
                // change the first char of filename back
                *tmp_dirEntry->DIR_Name = filename[0];
                // fill FATs
                for(unsigned int fat_idx = 0; fat_idx<bootEntry->BPB_NumFATs; fat_idx++){
                    unsigned int* fat_loc_recover = (unsigned int*)((char*)disk + FAT_Loc + fat_idx * FAT_Size);
                    for(unsigned int fat_to_fill = 0; fat_to_fill<cluster_num; fat_to_fill++){
                        if(fat_to_fill == cluster_num - 1){
                            *(fat_loc_recover + first_cluster + fat_to_fill) = 0xffffff8; // EOF 0F FF FF F8
                        } else {
                            *(fat_loc_recover + first_cluster + fat_to_fill) = first_cluster + fat_to_fill + 1; // point to next cluster
                        }
                    }
                }

                found_and_recover = 1;
                break;
            }
        }

        if(found_and_recover){
            if (sha1 != NULL && strlen(sha1) > 0)
                printf("%s: successfully recovered with SHA-1\n", filename);
            else
                printf("%s: successfully recovered\n", filename);
        } else {
            fprintf(stderr, "%s: file not found\n", filename);
        }

    }
}


// recover non-contiguous file that with in first 12 cluster with SHA1 hash
void recover_non_contiguous(void* disk, char* filename, char* sha1){
    char* filename_tail = filename; filename_tail++;
    int recover_cnt = 50;
    DirEntry** files_to_recover = malloc(sizeof (DirEntry*) * recover_cnt);
    int ambiguous_file_cnt = 0;

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
            if(*ptr == 0){
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
            if(*dir_name == 0xe5){
                dir_name++;
                if(strcmp((char*)dir_name, filename_tail) == 0){
                    if(ambiguous_file_cnt >= recover_cnt){
                        recover_cnt += 50;
                        DirEntry** tmp1 = realloc(files_to_recover, recover_cnt * sizeof (DirEntry*));
                        if (tmp1){
                            files_to_recover = tmp1;
                        } else {
                            exit(-1);
                        }
                    }
                    files_to_recover[ambiguous_file_cnt] = dirEntry;
                    ambiguous_file_cnt++;
                }
            }

            searched_directory_cnt++;
            existed_file_cnt++;
            dirEntry++;
        }
    }

    // under the assumption that the deleted, non-contiguous file only existed in first 12 clusters
    unsigned int clusters[] = {2,3,4,5,6,7,8,9,10,11,12,13};

    int found_and_recover = 0;
    for(int i = 0; i<ambiguous_file_cnt && !found_and_recover; i++){
        DirEntry *tmp_dirEntry = files_to_recover[i];
        // get first cluster and compute the total, this stands for K(cluster_num of this file) for later permutation computation
        // cluster_num = (file_size + cluster_size - 1) / cluster_size
        unsigned int first_cluster = starting_cluster(tmp_dirEntry->DIR_FstClusHI, tmp_dirEntry->DIR_FstClusLO);
        unsigned int cluster_num = (unsigned int)((tmp_dirEntry->DIR_FileSize + Cluster_Size - 1) / Cluster_Size);

        // compute permutation nPk = n! / (n-k)!
        unsigned int possible_perm_num = 1;
        for(unsigned int n = 12; n >= 12 - cluster_num + 1; n--)
            possible_perm_num *= n;
        // initialize the permutation calculator
        perm_result = malloc(sizeof (unsigned int*) * possible_perm_num);
        valid_perm_cnt = 0;
        // generate permutations of choose k(cluster_num) out of n(total 12 clusters)
        generate_permutations(clusters, 12, cluster_num, cluster_num, first_cluster);
        // for each possible permutation starts from the first cluster of this file, we update its sha1 hash value by iteration
        for(unsigned int perm = 0; perm<valid_perm_cnt; perm++){
            if(perm_result[perm][0] == first_cluster){
                unsigned int remain_size = (int)tmp_dirEntry->DIR_FileSize;
                // initialize SHA1
                SHA_CTX ctx;
                SHA1_Init(&ctx);
                for(unsigned int c = 0; c<cluster_num; c++){
                    // locate the cluster
                    char* cluster_ptr = ((char*)disk + Cluster2_Loc + (perm_result[perm][c] - 2) * Cluster_Size);
                    // update sha1 hash with [ cluster size || remain size ]
                    if(remain_size < Cluster_Size){
                        SHA1_Update(&ctx, cluster_ptr, remain_size);
                        remain_size -= remain_size;
                    } else {
                        SHA1_Update(&ctx, cluster_ptr, Cluster_Size);
                        remain_size -= Cluster_Size;
                    }
                }

                // generate, convert and check the final hash value
                unsigned char* hash = malloc(sizeof(unsigned char) * SHA_DIGEST_LENGTH);
                SHA1_Final(hash, &ctx);
                char* hash_hex = (char*)malloc(sizeof (char) * 41);
                for(int hash_idx = 0; hash_idx<20; hash_idx++)
                    sprintf(hash_hex+2*hash_idx, "%.2x", hash[hash_idx]);
                hash_hex[40] = '\0';

                // found the target, we re-delete it by changing the name and filling the FATs
                if(strcmp(hash_hex, sha1) == 0){
                    // change the first char of filename back
                    *tmp_dirEntry->DIR_Name = filename[0];
                    // fill FATs
                    for(unsigned int fat_idx = 0; fat_idx<bootEntry->BPB_NumFATs; fat_idx++){
                        unsigned int* fat_loc_recover = (unsigned int*)((char*)disk + FAT_Loc + fat_idx * FAT_Size);
                        for(unsigned int fat_to_fill = 0; fat_to_fill<cluster_num; fat_to_fill++){
                            if(fat_to_fill == cluster_num - 1){
                                *(fat_loc_recover + perm_result[perm][fat_to_fill]) = 0xffffff8; // EOF 0F FF FF F8
                            } else {
                                *(fat_loc_recover + perm_result[perm][fat_to_fill]) = perm_result[perm][fat_to_fill+1]; // point to next cluster
                            }
                        }
                    }

                    found_and_recover = 1;

                    free(hash);
                    free(hash_hex);
                    break;
                }

                free(hash);
                free(hash_hex);
            }
        }

        // free the perm_result for this file
        for(unsigned int perm = 0; perm<valid_perm_cnt; perm++)
            free(perm_result[perm]);
        free(perm_result);
    }

    if(found_and_recover){
        printf("%s: successfully recovered with SHA-1\n", filename);
    } else {
        fprintf(stderr, "%s: file not found\n", filename);
    }
}

// generate the permutation given cluster num, n using backtracing.
// the algorithm returns permutations of choosing K from N without ordering and fills the pre-defined result array.
// n(length of cluster num), k(target clusters), i = k
void generate_permutations(unsigned int* clusters, unsigned int n, unsigned int k, unsigned int i, unsigned int first_cluster){
    if(i == 0){
        unsigned int* res = malloc(sizeof(unsigned int) * k);
        for(unsigned int j = n; j<n+k; j++)
            res[j-n] = clusters[j];
        if(res[0] == first_cluster){
            perm_result[valid_perm_cnt] = res;
            valid_perm_cnt++;
        } else {
            free(res);
        }
        return;
    }
    for(unsigned int j = 0; j<n; j++)
    {
        swap(&clusters[j], &clusters[n-1]);
        generate_permutations(clusters, n-1, k, i-1, first_cluster);
        swap(&clusters[j], &clusters[n-1]);
    }
}

// swap two entries of an array
void swap(unsigned int* a, unsigned int* b){
    unsigned int tmp = *a;
    *a = *b;
    *b = tmp;
}
