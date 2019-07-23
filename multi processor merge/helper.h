#ifndef _HELPER_H
#define _HELPER_H

#define SIZE 44

struct rec {
    int freq;
    char word[SIZE];
};

struct rec2{
    int freq;
    char word[SIZE];
    int child_index;
    int rec_index;
};

int get_file_size(char *filename);
int compare_freq(const void *rec1, const void *rec2);
int compare_freq_rec2(const void *rec1, const void *rec2);
void read_input_file(char* infile,int pipe_fd[][2], int read_offset, int i, int read_num, struct rec* read_content);
void reading_from_pipe(int record_num, int chunk_num, struct rec** file_content, int pipe_fd[][2]);
void generate_merge_array(int chunk_num, struct rec2* merge_array, struct rec** file_content);
int read_rec_num(int child_index, int chunk_num, int record_num);
void merge(int record_num, int chunk_num, struct rec** file_content, struct rec* sorted_file_content);
void deallocate(int chunk_num, struct rec** file_content, struct rec* sorted_file_content);
void writing(char* outfile, struct rec* sorted_file_content, int record_num);
#endif /* _HELPER_H */
