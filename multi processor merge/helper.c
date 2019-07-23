#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "helper.h"


int get_file_size(char *filename) {
    struct stat sbuf;

    if ((stat(filename, &sbuf)) == -1) {
       perror("stat");
       exit(1);
    }

    return sbuf.st_size;
}

/* A comparison function to use for qsort */
int compare_freq(const void *rec1, const void *rec2) {

    struct rec *r1 = (struct rec *) rec1;
    struct rec *r2 = (struct rec *) rec2;

    if (r1->freq == r2->freq) {
        return 0;
    } else if (r1->freq > r2->freq) {
        return 1;
    } else {
        return -1;
    }
}

/*
 * rec2 is struct rec with augmented info, index of child process and index of the record in 2-dimension
 * array file_content.
 * A comparison function to use for qsort rec2.
 * */
int compare_freq_rec2(const void *rec1, const void *rec2) {

    struct rec2 *r1 = (struct rec2 *) rec1;
    struct rec2 *r2 = (struct rec2 *) rec2;

    if (r1->freq == r2->freq) {
        return 0;
    } else if (r1->freq > r2->freq) {
        return 1;
    } else {
        return -1;
    }
}

/*
 * In child process, read i-th chunk of input file. Sort records and write to i-th child pipe
 */
void read_input_file(char* infile,int pipe_fd[][2], int read_offset, int i, int read_num, struct rec* read_content){
    //open the input file and check error.
    FILE* fp = fopen(infile, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open file\n");
        exit(1);
    }
    //Set the file position and check error.
    if(fseek(fp, read_offset * sizeof(struct rec), SEEK_SET) == -1){
        perror("fseek fail");
        exit(1);
    }
    //Close reading end from current child and check error.
    if(close(pipe_fd[i][0]) == -1){
        perror("closing reading end from inside child");
        exit(1);
    }
    //Close reading ends from previous children and check error.
    for(int child_no = 0; child_no < i; child_no++){
        if(close(pipe_fd[child_no][0]) == -1){
            perror("closing reading end from previous children");
            exit(1);
        }
    }
    //Declare an array of records to store data to be read. And it will be written to pipe.
    //struct rec read_content[read_num];

    //Read data from input file and check error.
    for(int rec_index =0; rec_index< read_num; rec_index++){
        if(fread(&(read_content[rec_index]), sizeof(struct rec), 1, fp)!= 1){
            perror("read fail");
            exit(1);
        }
    }
    // Sort records.
    qsort(read_content, read_num, sizeof(struct rec), compare_freq);
    // Close the input file pointer and check error.
    if(fclose(fp)!=0){
        perror("closing input file");
        exit(1);
    }
    // Write records to pipes and check error.
    for(int rec_index = 0; rec_index < read_num; rec_index++){
        if(write(pipe_fd[i][1], &(read_content[rec_index]), sizeof(struct rec)) != sizeof(struct rec)){
            perror("writing from child to pipe");
            exit(1);
        }
    }

    // Close writing end from child and check error.
    if(close(pipe_fd[i][1]) == -1){
        perror("closing writing end from child after writing");
        exit(1);
    }
}

/*
 * In parent process, read records from pipe.
 */
void reading_from_pipe(int record_num, int chunk_num, struct rec** file_content, int pipe_fd[][2]){
    //Store sorted array of records read from each of child pipe  into 2-dimension array -- file_content
    for(int i = 0; i < chunk_num; i++){
        //find the number of records in i-th chunk of input file
        int read_num = read_rec_num(i, chunk_num, record_num);
        //Allocate memory for i-th sorted array read from pipe and check error
        file_content[i] = malloc(read_num * sizeof(struct rec));
        if(file_content[i] == NULL){
            perror("Allocating the memory of file_content fails");
            exit(1);
        }
        // Read records from pipe and check error
        for(int j=0; j< read_num;j++){
            if(read(pipe_fd[i][0], &(file_content[i][j]), sizeof(struct rec)) == -1){
                perror("reading from a child process");
                exit(1);
            }
        }
        // Close i-th child pipe's reading end from parent.
        if(close(pipe_fd[i][0]) == -1){
            perror("closing reading end from parent");
            exit(1);
        }
    }
}

/*
 * merge_array stores records in the format of rec2, the record with augmented info.
 * merge_array is the array of records with smallest frequency in the unsorted part of each array read from child pipe.
 * This function is to initialize merge_array.
 */
void generate_merge_array(int chunk_num, struct rec2* merge_array, struct rec** file_content){
    //loop over index of the child process
    for(int i=0; i< chunk_num; i++){
        /*
         * Initialize a new rec2.
         * Invert first record in each array from child process into a records in the format of rec2.
         * And assign i-th index of merge_array with the new rec2
         */
        struct rec2 new_rec2;
        new_rec2.freq = file_content[i][0].freq;
        strcpy(new_rec2.word,file_content[i][0].word);
        new_rec2.child_index = i;
        new_rec2.rec_index = 0;
        merge_array[i] = new_rec2;
    }
}

/*
 * Return the number of records each child process should read from input file.
 */
int read_rec_num(int child_index, int chunk_num, int record_num){
    int result;
    result = (int)record_num/chunk_num;
    if(child_index < record_num%chunk_num ){
        result += 1;
    }
    return result;
}

/*
 * Merge multiple sorted arrays of records different child processes read into one sorted array of
 * all records, sorted_file_content.
 */
void merge(int record_num, int chunk_num, struct rec** file_content, struct rec* sorted_file_content){
    /*
     * merge_array stores records in the format of rec2, the record with augmented info.
     * merge_array is the array of records with smallest frequency in the unsorted part
     * of each array read from child pipe.
     * Declare mere_array. Allocate memory and check error.
     */
    struct rec2* merge_array;
    merge_array = malloc(chunk_num* sizeof(struct rec2));
    if(merge_array ==NULL){
        perror("Allocating the memory of merge_array fails");
        exit(1);
    }
    //Initialize merge_array
    generate_merge_array(chunk_num, merge_array, file_content);
    //sorted_rec_number indicates the number of records that have been sorted.
    int sorted_rec_num = 0;
    //merge_array_size indicates the size of merge_size. It equals chunk_num at first.
    int merge_array_size = chunk_num;
    //loop ends until all records of all arrays read from pipes are sorted
    while(merge_array_size>0){
        //sort the merge_array
        qsort(merge_array, merge_array_size, sizeof(struct rec2), compare_freq_rec2);
        //index 0 of merge_array is the record with smallest frequency among remaining records.
        //Find its child_process index and rec_index in file_content, that is, 2 indexes of file_content[][]
        int output_child_index = merge_array[0].child_index;
        int output_rec_index = merge_array[0].rec_index;
        // rec_index_max is the number of records the child process read
        int rec_index_max = read_rec_num(output_child_index, chunk_num, record_num) -1 ;
        // let the record be the next in sorted_file_content
        sorted_file_content[sorted_rec_num] = file_content[output_child_index][output_rec_index];

        if(output_rec_index < rec_index_max){//if the record is not the last one in the array.
            // let merge_array[0] stores the info of record in next rec_index
            merge_array[0].rec_index += 1;
            struct rec next_rec = file_content[output_child_index][output_rec_index + 1];
            merge_array[0].freq = next_rec.freq;
            strcpy(merge_array[0].word, next_rec.word);
        }else{//if the record is the last one in the array.
            merge_array_size -= 1;
            merge_array[0] = merge_array[merge_array_size];
        }
        sorted_rec_num += 1;
    }
    //After sorting, free the memory of merge_array.
    free(merge_array);
}

/* free the memory of file_content and sorted_file_content*/
void deallocate(int chunk_num, struct rec** file_content, struct rec* sorted_file_content){
    for(int i = 0; i < chunk_num; i++){
        free(file_content[i]);
    }
    free(file_content);
    free(sorted_file_content);
}

/* Write sorted records to output file*/
void writing(char* outfile, struct rec* sorted_file_content, int record_num){
    // open output file and check error.
    FILE* fp = fopen(outfile, "wb");
    if(fp ==NULL){
        perror("Opening output file fails");
        exit(1);
    }
    // write data to output file.
    if (fwrite(sorted_file_content, sizeof(struct rec), record_num , fp) != record_num ) {
        perror("Writing to output file");
        exit(1);
    }
    //close the file and check error.
    if(fclose(fp)!=0){
        perror("closing output file");
        exit(1);
    }
}


