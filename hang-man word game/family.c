#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "family.h"


/* Number of word pointers allocated for a new family.
   This is also the number of word pointers added to a family
   using realloc, when the family is full.
*/
static int family_increment = 0;


/* Set family_increment to size, and initialize random number generator.
   The random number generator is used to select a random word from a family.
   This function should be called exactly once, on startup.
*/
void init_family(int size) {
    family_increment = size;
    srand(time(0));
}


/* Given a pointer to the head of a linked list of Family nodes,
   print each family's signature and words.

   Do not modify this function. It will be used for marking.
*/
void print_families(Family* fam_list) {
    int i;
    Family *fam = fam_list;
    
    while (fam) {
        printf("***Family signature: %s Num words: %d\n",
               fam->signature, fam->num_words);
        for(i = 0; i < fam->num_words; i++) {
            printf("     %s\n", fam->word_ptrs[i]);
        }
        printf("\n");
        fam = fam->next;
    }
}


/* Return a pointer to a new family whose signature is 
   a copy of str. Initialize word_ptrs to point to 
   family_increment+1 pointers, numwords to 0, 
   maxwords to family_increment, and next to NULL.
*/
Family *new_family(char *str) {
    //Initial a new family address.
    Family *new_family;


    //Allocate the memory of new_family. And check the error of memory allocation.
    new_family = malloc(sizeof(Family));
    if(new_family  == NULL){
        perror("memory allocated failed\n");
        exit(1);
    }

    //Allocate the memory of signature of new_family. And check the error of memory allocation.
    new_family->signature = malloc(strlen(str) + 1);
    if(new_family->signature ==NULL){
        perror("memory allocated failed\n");
        exit(1);
    }
    //copy str to the signature of new_family.
    strcpy(new_family->signature, str);

    //Allocate the memory of word pointers of new_family. And check the error of memory allocation.
    new_family->word_ptrs = malloc((family_increment+1) * sizeof(char*));
    if(new_family->word_ptrs == NULL){
        perror("memory allocated failed\n");
        exit(1);
    }

    // Initialize max_words, num_words, and next  of new_family.
    new_family->max_words = family_increment;
    new_family->num_words = 0;
    new_family->next = NULL;
    return new_family;
}


/* Add word to the next free slot fam->word_ptrs.
   If fam->word_ptrs is full, first use realloc to allocate family_increment
   more pointers and then add the new pointer.
*/
void add_word_to_family(Family *fam, char *word) {
    //Only in the case where parameter fam is not NULL.
    if(fam != NULL) {
        /*
         * When the number of words in the family reaches maximum, reallocate the memory for the new word.
         * And check the error of memory reallocation.
         * Increase the max_words by 1.
         */
        if (fam->max_words == fam->num_words) {
            fam->word_ptrs = realloc(fam->word_ptrs, (fam->max_words + 2)* sizeof(char*));
            if(fam->word_ptrs == NULL){
                perror("reallocating memory of word pointers fail when adding word to family\n");
                exit(1);
            }
            fam->max_words++;
        }
        /*
         * Add the new word into the words pointers of the family. Increase the number of words by 1.
         * Assign the last index of word_ptrs with NULL.
         */
        fam->word_ptrs[fam->num_words] = word;
        fam->num_words++;
        fam->word_ptrs[fam->num_words] = NULL;
    }

}


/* Return a pointer to the family whose signature is sig;
   if there is no such family, return NULL.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_family(Family *fam_list, char *sig) {
    // Initialize a cursor in the family list
    Family *cur = fam_list;
    // loop through each node in the family until find the family with the signature to be found.
    while(cur != NULL && strcmp(cur -> signature, sig) != 0 ){
        cur = cur -> next;
    }
    return cur;
}


/* Return a pointer to the family in the list with the most words;
   if the list is empty, return NULL. If multiple families have the most words,
   return a pointer to any of them.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_biggest_family(Family *fam_list) {
    /*
     * Copy the address of the head of family list into a new variable cur.
     * Initial max_words representing the maximum number of the words among all families.
     * Initial an address of a family to return.
     */
    Family *cur = fam_list;
    int max_words = 0;
    Family *ret_family = NULL;

    // loop through each family in the list. Find the family with most words.
    while(cur != NULL){
        if(cur->max_words > max_words){
            max_words = cur->max_words;
            ret_family = cur;
        }
        cur = cur -> next;
    }
    return ret_family;
}


/* Deallocate all memory rooted in the List pointed to by fam_list. */
void deallocate_families(Family *fam_list) {
    //Initialize an address of family.
    Family *current;
    //loop through each family in the list
    while(fam_list != NULL){
        // free signature and word_ptrs
        free(fam_list->signature);
        free(fam_list->word_ptrs);
        // record the address of current family and move to next family. Then free the current family.
        current = fam_list;
        fam_list = fam_list->next;
        free(current);
    }
}


/* Generate and return a linked list of all families using words pointed to
   by word_list, using letter to partition the words.

   Implementation tips: To decide the family in which each word belongs, you
   will need to generate the signature of each word. Create only the families
   that have at least one word from the current word_list.
*/
Family *generate_families(char **word_list, char letter) {
    //Initial the head of the family list and a cursor in the family list.
    Family *head = NULL;
    Family *cur;
    //loop through each word in the dictionary
    for(int i = 0; word_list[i] != NULL; i++){
        //Find the signature of the word
        char *curr_word = word_list[i];
        char signature[1 + strlen(curr_word)];
        for(int j = 0; j != strlen(curr_word); j++){
            if(curr_word[j] == letter){
                signature[j] = letter;
            }else{
                signature[j] = '-';
            }
        }
        signature[strlen(curr_word)] = '\0';

        //let cursor point to the family with the signature same as the word.
        cur = find_family(head, signature);
        /* if the family matching the signature can not be found, then create a new family with
         * the signature.Let the new family be the cursor and the head of the family list.
         */
        if(cur == NULL){
            cur = new_family(signature);
            cur->next = head;
            head = cur;
        }
        // add word to the family the cursor pointing to.
        add_word_to_family(cur, curr_word);
    }
    return head;
}


/* Return the signature of the family pointed to by fam. */
char *get_family_signature(Family *fam) {
    return fam->signature;
}


/* Return a pointer to word pointers, each of which
   points to a word in fam. These pointers should not be the same
   as those used by fam->word_ptrs (i.e. they should be independently malloc'd),
   because fam->word_ptrs can move during a realloc.
   As with fam->word_ptrs, the final pointer should be NULL.
*/
char **get_new_word_list(Family *fam) {
    //Initialize an address of a new word list to return.
    char **new_wordlist = NULL;
    // Only in the case where fam is not NULL.
    if(fam != NULL) {
        //Allocating memory for the new word list and check the error of memory allocation.
        new_wordlist = malloc(sizeof(char*) * (1 + fam->max_words));
        if(new_wordlist == NULL){
            perror("new word list memory allocation fails\n");
            exit(1);
        }
        // each word of the new word pointer is same as the word pointers in the family
        for(int i = 0; i < fam->num_words; i++){
            new_wordlist[i] = (fam->word_ptrs)[i];
        }
        // The final pointer of new word list is also NULL.
        new_wordlist[fam->num_words] = NULL;
    }
    return new_wordlist;
}


/* Return a pointer to a random word from fam. 
   Use rand (man 3 rand) to generate random integers.
*/
char *get_random_word_from_family(Family *fam) {
    // generate a random nature number  no smaller than 0  and no larger than the largest index of word pointers.
    int random = rand() % fam->num_words;
    // pick a random word in the word pointers based on the random nature number.
    char *ret = (fam -> word_ptrs)[random];
    return ret;
}
