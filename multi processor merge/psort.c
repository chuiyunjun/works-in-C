#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <getopt.h>
#include <sys/wait.h>
#include "helper.h"


int main(int argc, char *argv[]) {
    // Declare variables
    // status is the status of a process
    int status;
    // the name of input and output file
    char *infile = NULL, *outfile = NULL;
    // chunk_num indicates how many chunks the input file are divided into
    int chunk_num;
    //option indicates the option name in the command line
    int option;

    // check the number of command-line arguments
    if (argc != 7) {
        fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
        exit(1);
    }
    // check whether options -n , -f , -o are provided in the command-line argument.
    while ((option = getopt(argc, argv, "n:f:o:")) != -1) {
        switch(option) {
            case 'n':
                chunk_num = strtol(optarg, NULL, 10);
            case 'f':
                infile = optarg;
                break;
            case 'o':
                outfile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: mkwords -f <input file name> -o <output file name>\n");
                exit(1);
        }
    }
    // Declare and initialize variables.
    // Declare pipe_fd for parent process and its child processes.
    int pipe_fd[chunk_num][2];
    // read_num is the number of records that the child process in current iteration will read.
    int read_num;
    // record_num is the total number of records in the input file.
    int record_num = get_file_size(infile)/ sizeof(struct rec);
    //read_offset is the distance between SEEK_SET and the location where to begin to read in current iteration.
    int read_offset = 0;
    // the maximum of chunk_num is record_num. It is unnecessary to create more than record_num child processes.
    if(chunk_num > record_num){
        chunk_num = record_num;
    }
    // Create child processes to read records from input file respectively
    for(int i=0; i < chunk_num; i++){
        //find read_num for current iteration
        read_num = read_rec_num(i, chunk_num, record_num);
        

        //For current child pipe
        if(pipe(pipe_fd[i])==-1){
            perror("pipe");
            exit(1);
        }
        //Create a child process.
        int result = fork();
        if(result < 0){// the case where creating a child process fails
            perror("fork");
            exit(1);
        }else if(result ==0){// the case where it is in a child process.
            //Read i-th chunk of input file. Sort them and write to current child pipe.
            struct rec* read_content;
            read_content = malloc(read_num *sizeof(struct rec)); 
            if(read_content ==NULL){
               perror("Allocating memory fails");
               exit(1);
            }
            read_input_file(infile, pipe_fd, read_offset, i, read_num, read_content);
            free(read_content);
            exit(0);
        }else{// the case where it is in the parent process.
            //find the location in input file where to begin to read in next created child process.
            read_offset += read_num;
            //close writing end of pipe from parent
            if(close(pipe_fd[i][1]) == -1){
                perror("closing writing end from parent");
                exit(1);
            }
        }
    }
    /*
     * file_content[index of child process][index of records read in this child process]
     * is a 2-dimension array storing records.
     * Allocate the memory for file_content and check error.
     */
    struct rec** file_content = malloc(chunk_num * sizeof(struct rec*));
    if(file_content == NULL){
        perror("file_content memory allocating fail");
        exit(1);
    }
    // read the input file content from pipe and store it in file_content
    reading_from_pipe(record_num,  chunk_num, file_content, pipe_fd);

    //Parent waits for child processes. And check the error of child's abnormal terminating.
    for(int i =0;i < chunk_num; i++) {
        if (wait(&status) == -1) {
            fprintf(stderr, "Child terminated abnormally\n");
            exit(1);
        } else if (WEXITSTATUS(status)) {
            fprintf(stderr, "Child terminated abnormally\n");
            exit(1);
        }
    }
    /*
     * sorted_file_content is a 1-dimension array to store sorted records.
     * Allocate the memory for sorted_file_content and check error.
     */
    struct rec* sorted_file_content = malloc(record_num * sizeof(struct rec));
    if(sorted_file_content == NULL){
        perror("Allocating the memory of sorted_file_content fails");
        exit(1);
    }
    /*
     * merge multiple sorted arrays of records different child processes read into one sorted array of
     * all records, sorted_file_content.
     */
    merge(record_num, chunk_num, file_content, sorted_file_content);
    //write sorted records to output file
    writing(outfile, sorted_file_content, record_num);

    // free the allocated memory.
    deallocate(chunk_num,  file_content, sorted_file_content);
    return 0;
}
