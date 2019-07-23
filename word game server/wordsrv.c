#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>


#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 51250
#endif
#define MAX_QUEUE 5
#define JOIN_MSG " has just joined."
#define RETYPE_MSG "Username exists or is empty.Please enter again!"

void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);

/* These are some of the function prototypes that we used in our solution 
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */
/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf);
void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);


/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;


/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }
    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

/*
 * write message to every client's fd.
 */
void broadcast(struct game_state *game, char *outbuf){
	for(struct client *every_p = game->head;
		every_p != NULL;  every_p = every_p->next){
			if(write(every_p->fd, outbuf, strlen(outbuf)) != strlen(outbuf)){
				perror("broadcast");
				exit(1);
			}
        }


}

/*
 * broadcast current state of the game to every client.
 */
void announce_game_state(struct game_state *game){
	char msg[MAX_MSG];
	broadcast(game, status_message(msg,game));
}

/*
 * announce the player that will take the next turn.
 */
void announce_turn(struct game_state *game){
	char turn_msg[MAX_MSG];
	for(struct client* every_p = game->head; every_p != NULL; every_p = every_p->next){
		if(game->has_next_turn == every_p){
			// ask the playing taking the turn for the guess
			if(write(every_p->fd, "Your guess?\n", strlen("Your guess?\n")) != strlen("Your guess?\n")){
				perror("announce turn");
				exit(1);
			}
		}else{
			// tell other players with the name of player who takes next turn
			turn_msg[0] = '\0';
			strcat(turn_msg, "It's ");
			strcat(turn_msg, game->has_next_turn->name);
			strcat(turn_msg, "'s turn.\n");
			if(write(every_p->fd, turn_msg, strlen(turn_msg)) != strlen(turn_msg)){
				perror("announce turn");
				exit(1);
			}
		}
	}
	printf("It's %s's turn.\n", game->has_next_turn->name);
}

/*
 * After someone wins the game, announce who the winner is and begin a new game.
 */
void announce_winner(struct game_state *game, struct client *winner){
	// send message to players
	char  win_msg[MAX_MSG];
	strcpy(win_msg, "The word was ");
	strcat(win_msg, game->word);
	strcat(win_msg, ".\nGame over!");
	broadcast(game, win_msg);
	for(struct client *every_p = game->head;
        every_p != NULL;  every_p = every_p->next){
		if(every_p == winner){
			strcpy(win_msg, " You win!\n\n");
		}else{
			strcpy(win_msg, winner->name);
			strcat(win_msg, " won!\n\n\n");
		}
		if(write(every_p->fd, win_msg, strlen(win_msg)) != strlen(win_msg)){
			perror("announce winner");
			exit(1);
		}
	}
	broadcast(game, "Let's start a new game\n");
	// print message in the server window.
	printf("Game over. %s won!\n", winner->name);
	printf("%s\n", "New game.");
}

/*
 * announce that game is over and begin a new game.
 */
void announce_game_over(struct game_state *game){
	broadcast(game, "No more guesses.  The word was ");
	broadcast(game, game->word);
	broadcast(game, "\n\nLet's start a new game.\n");
	printf("%s\n", "Evaluating for game_over");
	printf("%s\n", "New game.");
}

/*
 * When the guess is wrong, announce the guess is invalid
 */
void announce_guess_invalid(struct client *current_player){
	char guess_msg[MAX_MSG];
	strcpy(guess_msg, current_player->inbuf);
	strcat(guess_msg, " is not in the word\n");\
	if(write(current_player->fd, guess_msg, strlen(guess_msg)) != strlen(guess_msg)){
		perror("announce invalid guess");
		exit(1);
	}
	printf("%s",guess_msg);
}

/*
 * tell players what the guessed letter is.
 */
void announce_letter_guessed(struct game_state *game, struct client *current_player){
	char guess_msg[MAX_MSG];
	guess_msg[0] = '\0';
	strcpy(guess_msg, current_player->name);
	strcat(guess_msg, " guess: ");
	strcat(guess_msg, current_player->inbuf);
	strcat(guess_msg, "\n");
	broadcast(game, guess_msg);
}

/*
 * tell who will have the next turn
 */
void advance_turn(struct game_state *game){
	if(game->head == NULL){
		game->has_next_turn = NULL;
	}else{
		if(game->has_next_turn->next){
			game->has_next_turn = game->has_next_turn->next;
		}else{
			game->has_next_turn = game->head;
		}
	}
}

/*
 * warn the player guessing a letter out of turn.
 */
void announce_not_your_turn(struct client *player){
	printf("[%d] Found newline %s\n", player->fd, player->inbuf);
	printf("Player %s tried to guess out of turn\n", player->name);
	char* message;
	message = "It is not your turn.\n";
	if(write(player->fd, message, strlen(message)) != strlen(message)){
		perror("announce out of turn");
		exit(1);
	}
}


int main(int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;
    
    if(argc != 2){
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }
    
    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);
    
    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;
    
    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;
    
    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);
    
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if(write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&(game.head), p->fd);
            };
        }
        
        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
		if(FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player
                for(p = game.head; p != NULL; p = p->next) {
                	if(cur_fd == p->fd) {
						// handle input from an active client
						if(game.has_next_turn->fd == p->fd){
							//if the play is in his/her turn
							int read_num;
							read_num = read(p->fd, p->in_ptr, MAX_BUF);
							if(read_num ==-1) {
								perror("read letter from player in turn");
								exit(1);
							}else if(read_num == 0){
								printf("Disconnect form %s\n", inet_ntoa(p->ipaddr));
								advance_turn(&game);
								if(game.has_next_turn == p){
									game.has_next_turn = p->next;
								}
								remove_player(&(game.head), p->fd);
								break;
							}else{
								printf("[%d] Read %d bytes\n", p->fd, read_num);
								p->in_ptr += read_num;
								if(*(p->in_ptr - 1)== '\n' && *(p->in_ptr - 2)== '\r'){
									// finish reading an entire line, and then remove 'enter'  ('\r\n')
									*(p->in_ptr - 2) = '\0';
									printf("[%d] Found newline %s\n", p->fd, p->inbuf);
									p->in_ptr = p -> inbuf;
									// check whether the guess is invalid
									char *guess_inv_msg;
									if(strlen(p->inbuf) != 1){
										//length is not 1.
										guess_inv_msg = "guess length should be 1.\n";
										write(p->fd, guess_inv_msg, strlen(guess_inv_msg));
									}else if((int) p->inbuf[0] < (int) 'a' || (int) p->inbuf[0] > (int) 'z'){
										//input is not a lower case letter
										guess_inv_msg = "guess should be a letter in lower case.\n";
										write(p->fd, guess_inv_msg, strlen(guess_inv_msg));
									}else if(game.letters_guessed[(int) p->inbuf[0] - (int) 'a'] == 1){
										guess_inv_msg = "This letter has been guessed.\n";
										write(p->fd, guess_inv_msg, strlen(guess_inv_msg));
									}else{
										//update the game state
										game.letters_guessed[(int) p->inbuf[0] - (int) 'a'] = 1;
										int incorrect_guess = 1;
										int win = 1;
										for(int i = 0; i< strlen(game.word); i++){
											if(game.word[i] == p->inbuf[0]){
												game.guess[i] = p->inbuf[0];
												incorrect_guess = 0;
											}
											if(game.guess[i] == '-'){
												win = 0;
											}
										}
										game.guesses_left -= incorrect_guess;
										advance_turn(&game);
										if(incorrect_guess){
											announce_guess_invalid(p);
										}

										//check whether the game ends
										if(win){
											//if someone win
											//announce it and begin a new game
											announce_winner(&game, p);
											init_game(&game, argv[1]);
										}else if(game.guesses_left <= 0){
											//if game is over and player did not find the answer
											//announce it and begin a new game
											announce_game_over(&game);
											init_game(&game, argv[1]);
										}else{
											announce_letter_guessed(&game, p);
										}
										announce_game_state(&game);
										announce_turn(&game);
									}
								}
							}
						}else{
							int read_num;
							read_num = read(p->fd, p->in_ptr, MAX_BUF);
							if(read_num == -1){
								//check the error
								perror("read letter form a player out of turn");
								exit(1);
							}else if(read_num == 0){
								//if a player out of turn disconnect.
								printf("Disconnect form %s\n", inet_ntoa(p->ipaddr));
								remove_player(&(game.head), p->fd);
								break;
							}else{
								//if a player out of turn guessed a letter, then send a warning message to the player
								printf("[%d] Read %d bytes\n", p->fd, read_num);
								p->in_ptr += read_num;
								if(*(p->in_ptr - 1)== '\n' && *(p->in_ptr - 2)== '\r'){
									// find an entire line from the player out of turn
									*(p->in_ptr - 2) = '\0';
									announce_not_your_turn(p);
									p->in_ptr = p -> inbuf;
								}
							}
						}
					}
                }

                // last_p pointers to previous player of player p.
                struct client* last_p = NULL;
                // Check if any new players are entering their names
                for(p = new_players; p != NULL; p = p->next) {
                    if(p == new_players -> next){
                         last_p = new_players;
                    }else if(p != new_players){
                         last_p = last_p->next;
                    }
                    //find active players
                    if(cur_fd == p->fd) {
                        //handle input from an new client who has
                        // not entered an acceptable name.
                        int read_num = 0;
                        int invalid = 0;
                        read_num = read(p->fd, p->in_ptr, MAX_BUF);
                        if(read_num == -1){
                        	perror("read username from new player");
                        	exit(1);
                        }else if(read_num == 0){
                        	//new player disconnected
							printf("Disconnect form %s\n", inet_ntoa(p->ipaddr));
							remove_player(&new_players, p->fd);
							break;
						}else{
							printf("[%d] Read %d bytes\n", p->fd, read_num);
		                	p->in_ptr += read_num;
		                	if(*(p->in_ptr - 1)== '\n' && *(p->in_ptr - 2)== '\r'){
		                		//find an entire line from new player and then remove '\r\n' to get the game
								*(p->in_ptr - 2) = '\0';
								strcpy(p->name, p->inbuf);
								printf("[%d] Found newline %s\n",p->fd, p->name);

								//check whether the name exists
								for(struct client *every_p = game.head;
									every_p != NULL;  every_p = every_p->next){
									if(strcmp(every_p->name, p->name) == 0){
										invalid = 1;
										break;
									}

								}
								//check whether the name is empty.
								if(strlen(p->name) == 0){
									invalid =1;
								}
								//Initialize the inbuf and in_ptr again
								p->inbuf[0]  ='\0';
								p->in_ptr = p->inbuf;

								if(invalid){
									// if the username is invalid, send a message to the new player and tell
									// the player to enter the username again.
									p->name[0] = '\0';
									char *retype = RETYPE_MSG;
									if(write(p->fd, retype, strlen(retype)) == -1) {
										fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
										remove_player(&(game.head), p->fd);
									}
								} else{
									//after entering a valid name
									char message[MAX_MSG];
									strcpy(message, p->name);
									strncat(message, " has just joined.\n", sizeof(message)-strlen(message) -1);
									//Declare a new client ,'new', and allocate memory for it
									//Let it be same as p.
									//Set it as new game.head.
									struct client* new;
									new = malloc(sizeof(struct client));
									new->fd = p->fd;
									new->ipaddr = q.sin_addr;
									new->name[0] = '\0';
									strcpy(new->name, p->name);
									new->in_ptr = new->inbuf;
									new->inbuf[0] = '\0';
									new->next = game.head;
									game.head = new;
									// After put the new client into game, tell who will be the next player.
									if(game.has_next_turn == NULL){
										game.has_next_turn= game.head;
									}
									//remove p from new_players
									if(last_p == NULL){
										new_players = p->next ;
										p->next = NULL;
									}else{
										last_p->next = p->next;
										p->next = NULL;
									}
									// tell everyone in game that a new player joined.
									broadcast(&game, message);
									// print message in the server window
									printf("%s", message);
									// tell the player who just joined the game with the current game state
									char status_msg[MAX_MSG];
									char return_msg[MAX_MSG];
									strcpy(return_msg, status_message(status_msg, &game));
									write(p->fd, return_msg, strlen(return_msg));
									// tell everyone in game who is next.
									announce_turn(&game);
									//free p
									free(p);
									break;
								}
		                	}
						}
                    }

                }
            }
        }
    }
    return 0;
}


