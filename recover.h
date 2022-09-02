//
// Created by apple on 11/20/21.
//

#ifndef NYUFILE_RECOVER_H
#define NYUFILE_RECOVER_H

#endif //NYUFILE_RECOVER_H

void recover_contiguous(void* disk, char* filename, char* sha1);
void recover_non_contiguous(void* disk, char* filename, char* sha1);
void generate_permutations(unsigned int* clusters, unsigned int n, unsigned int k, unsigned int i, unsigned int first_cluster);
void swap(unsigned int* a, unsigned int* b);

unsigned int** perm_result;
unsigned int valid_perm_cnt;
