#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>         /* gethostname */

#define PORT 60000
#define NUM_CLIENTS 2
#define BUF_SIZE 20


// Incoming messages are printed in stdout
// Outgoing messages are sent from stdin


int initialize_socket_connect (char *hostname, int port_offset){
    static int index_client = 0;
    // Create socket
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if ( -1 == soc ){
        perror("client: socket");
        exit(1);
    }

    // Set information about the port (and IP) we want to be connected to
    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons (PORT + port_offset + index_client);
    client_address.sin_addr.s_addr = INADDR_ANY; // macros for 0.0.0.0
    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&client_address.sin_zero, 0, 8);


    // Lookup host IP address
    struct hostent *hp = gethostbyname(hostname);
    if (hp == NULL) {
        fprintf(stderr, "client: unknown host %s\n", hostname);
        exit(1);
    }

    client_address.sin_addr = *((struct in_addr *) hp->h_addr);

    // Request connection to server.
    if ( -1 == connect(soc, (struct sockaddr *) &client_address, sizeof(client_address)) ) {
        if ( -1 == close(soc) ) {
            if ( -1 == close(soc) ) {
                perror("client: close");
            }
            exit(1);
        }

        index_client++;
        if ( index_client < NUM_CLIENTS){
            return initialize_socket_connect (hostname, port_offset);
        } else {
            perror("client: connect");
            return -1;
        }
    }
    return soc;
}



char *choose_username(int soc){
    // Choose username
    char username[BUF_SIZE + 3];
    username[0] = '\0';
    char *return_username = malloc( sizeof(char) * (BUF_SIZE + 1) );
    return_username[0] = '\0';
    
    while ( strlen(username) == 0 || strlen(username) > BUF_SIZE){ 
        printf("Please type your username:\n");
        
        fgets(username, BUF_SIZE + 3, stdin);
        // '\n' is later deleted 
        char *n_ptr = strchr(username, '\n');

        // check if the string is too long
        if (  strlen(username) > BUF_SIZE) {
            // clean up the privious input if a too long string is provided
            if ( !n_ptr ) {
                char temp_buf[BUF_SIZE + 3];
                while ( !strchr( fgets(temp_buf, BUF_SIZE + 3, stdin), '\n') ) {
                    // clean up the privious input if a long string is provided
                }
            } else {
                *n_ptr = '\0'; 
            }

            printf("Chosen username is longer then %d characters, try again\n", BUF_SIZE);
        } else {
            *n_ptr = '\0';
        }
    }
    // printf("My username: %s, n = %d\n", username, (int) strlen(username) );
    
    strcpy(return_username, username);
    // Inform server about chosen username
    strcat(username, "\r\n\0");
    int write_length = write( soc, username, strlen(username) + 1 );  
    if ( -1 ==  write_length) {
        perror("client: write");
        if ( -1 == close(soc) ) {
            perror("client:close");
        }
        exit(1);
    } else if ( (int) ( strlen(username) + 1 ) !=  write_length) {
        fprintf(stderr, "client: write n\n");
        if ( -1 == close(soc) ) {
            perror("client:close");
        }
        exit(1);
    }

    return return_username;
}

int find_network_newline(const char *buf, int n) {
    // printf("- step 2\n");

    for (int i = 0; i < n -1; i++) {
        if ( buf[i] == '\r' && buf[i+1] == '\n') {
            // printf("find_network_newline returned i = %d\n", i);
            return (i + 1) + 1;
        }
    }
    // printf("find_network_newline ---- returned i = -1\n");
    return -1;

}







int main (int argc, char **argv) {
    int port_offset = 0;
    if (argc == 3) {
        // Set port offset
        char * endptr;
        errno = 0;    /* To distinguish success/failure after call */
        port_offset = (int) strtol(argv[2], &endptr, 0);
        if (  *endptr  != '\0' ) {
            fprintf(stderr, "Improper port offset value, safe port range is (1024;65535)\n");
            exit(1);
        }
        if (errno) {
            perror("client: strtol");
            exit(1);
        }

        // Port number check
        if ( PORT + port_offset < 1024 || PORT + port_offset > 65535) {
            fprintf(stderr, "Chosen port %d is outside of safe ports range (1024;65535)\n", PORT + port_offset);
            exit(1);
        }
    } else if (argc != 2) {
        fprintf(stderr, "Usage: rpsls_client hostname port_offset(optional)\n");
        exit(1);
    }
    char *hostname = argv[1];
    char *username = NULL;


    
    // Declaration of Client-Server rules
    // in cass when a change to this variables is required, it is also to be changed in server code
    // -- Message for requesting custumers input 
    char *msg_rquest = "Your hand gesture:"; // followed by "\r\n" in server implementation
 
    // -- Possible commands
    int arr_game_command[128];
    // --- make all int bits to be 0
    memset(arr_game_command, 0, sizeof(int)*128 );
    arr_game_command['r'] = 1; // rock
    arr_game_command['p'] = 1; // paper
    arr_game_command['s'] = 1; // scissors
    arr_game_command['l'] = 1; // lizard
    arr_game_command['S'] = 1; // Spock
    arr_game_command['e'] = 2; // -- END



    // Iteratively try connecting to the ports available for the game
    int soc = initialize_socket_connect (hostname, port_offset);
    // If connection established
    if ( soc > 0) {


        username = choose_username(soc);


        // Play game: command client-server interactoin
        char buf[BUF_SIZE * NUM_CLIENTS * 2];
        buf[0] ='\0';
        char message[BUF_SIZE * NUM_CLIENTS * 2];
        message[0] = '\0';
        int num_read = 0;
        char command[ 1 + 3];
        command[0] = 'a';
        command[1] = '\0';
        // Prepare to read ( initialize port set data )
        fd_set all_fds; // set of file descriptors to read from
        FD_ZERO(&all_fds); // set all bits to be 0
        FD_SET(soc, &all_fds);
        FD_SET(STDIN_FILENO, &all_fds);
        int max_fd;
        if ( soc > STDIN_FILENO ) {
            max_fd = soc; 
        } else {
            max_fd = STDIN_FILENO;
        }

        
        while(1) {
            // printf ("--- client -> while\n");
                    
            int where = 0;
            int inbuf = 0;
            buf[0] ='\0';
            message[0] = '\0';
            memset(message, 0, sizeof(char)*(BUF_SIZE * NUM_CLIENTS * 2) );


            // Read server communication
            if ( -1 == (num_read = read( soc, buf, sizeof(buf) ) ) ) {
                perror("read");
                if ( -1 == close(soc) ) {
                    perror("close");
                }
                exit(1);
            }

            if ( num_read > 0) {
                buf[num_read] = '\0';
                // printf("buf = %s", buf);

                
                inbuf += num_read;

                // printf("-> inbuf = %d\n", inbuf);

                // Check if full message received
                int isWhile = 1;
                while ((where = find_network_newline(buf, inbuf)) > 0) {
                    isWhile = 0;
                    // printf("-> Analize message\n");
                    buf[where - 1] = '\0';
                    buf[where - 2] = '\0';


                    // Check message length
                    if ( strlen(message) + strlen(buf) + 3 > BUF_SIZE * NUM_CLIENTS * 2 - 1 ) {
                        fprintf (stderr, "Message is too long. This should never happen\n");
                        if ( -1 == close(soc) ) {
                            perror("close");
                        }
                        exit(1);
                    }
                    
                    // *str_end = '\0';
                    // Copy
                    strcat(message, buf);
                    message[where - 2] = '\n';


                    // When game input is requested
                    if ( strstr(message, msg_rquest) ) {

                        // Reset
                        fd_set listen_fds = all_fds;

                        
                        // Clean up current command
                        command[0] = 'a';
                        command[1] = '\0';
                        // Take comand
                        while ( arr_game_command[ (int) command[0] ] == 0 || strlen(command) > 1){ 
                            // printf("-> IIN WHILE \n");
    
                            printf("%s", message);
                            
                            
                            int nready = select( max_fd + 1, &listen_fds, NULL, NULL, NULL);
                            if ( -1 == nready ) {
                                perror("client: select");
                                if ( -1 == close(soc) ) {
                                    perror("client: close");
                                }
                                exit(1);
                            }
                            
                            // Game is done
                            if ( FD_ISSET(soc, &listen_fds) ) {
                                // printf("------------------------------------------\n");
                                goto NEXT;
                            }

                                                
                            fgets(command, 1 + 3, stdin);
                            // '\n' is later deleted 
                            char *n_ptr = strchr(command, '\n');

                            // check if the string is too long
                            if (  strlen(command) > 1 + 1 || arr_game_command[ (int) command[0] ] == 0 ) {
                                // clean up the privious input if a too long string is provided
                                if ( !n_ptr ) {
                                    char temp_buf[BUF_SIZE + 3];
                                    while ( !strchr( fgets(temp_buf, BUF_SIZE + 3, stdin), '\n') ) {
                                        // clean up the privious input if a long string is provided
                                    }
                                } else {
                                    *n_ptr = '\0'; 
                                }
                                printf("Incorrect gesture\n");
                                printf("Usage: r -rock, p -paper, s -scissors, l -lizard, S -Spock; e -END\n");

                            } else {
                                *n_ptr = '\0';
                            }
                            // printf("> %s\n", command);
                            // printf("len: %d,command: %s\n", (int) strlen(command), command);
                        }

                        
                        // Send gesture to the server
                        strcat(command, "\r\n\0");
                        int write_length = write( soc, command, strlen(command) + 1 );  
                        if ( -1 ==  write_length) {
                            perror("client:  write");
                            if ( -1 == close(soc) ) {
                                perror("client: close");
                            }
                            exit(1);
                        } else if ( ( (int) strlen(command) + 1 ) !=  write_length) {
                            fprintf(stderr, "client: write n\n");
                            if ( -1 == close(soc) ) {
                                perror("client: close");
                            }
                            exit(1);
                        }

                    } else {

                        printf("%s", message);
                        
                        if ( strstr(message, "Game end:") ) {
                            char msg_cpy[strlen(message) + 1];
                            msg_cpy[0] ='\0';
                            // Game end message 
                            char *score = strstr(message, "wins " ) + 5;
                            // printf("%p", score);
                            char *score_end = strstr(message, ", games played" );
                            // printf("%p", score_end);
                            // printf("len: %d", (int) (score_end - score) );
                            
                            strncpy(msg_cpy, score, score_end - score);
                            fprintf(stderr, "%s\n", msg_cpy);    
                        } else if (strstr(message, "Game:")) {
                            // Game introduction message 
                            // -- do nothing
                        } else {
                            // Previous game result message
                            if ( strstr(message, username) ) {
                                fprintf(stderr, "win\n"); 
                            } else {
                                fprintf(stderr, "lose\n"); 
                            }
                        }
                        
                    }


                    NEXT:
                    // Clear current message
                    message[0] = '\0';



                    strcpy(buf, buf + where + 1);
                    inbuf -= (where + 1);

                }


                // Full string is not delivered
                if (where == -1 && isWhile) {
                    // Check message length
                    if ( strlen(message) + strlen(buf) > BUF_SIZE * NUM_CLIENTS * 2 -1 ) {
                        fprintf (stderr, "Message is too long. This should never happen\n");
                        if ( -1 == close(soc) ) {
                            perror("close");
                        }
                        exit(1);
                    }
                    // Copy
                    strcat(message, buf);
                }

            } else {
                if ( -1 == close(soc) ) {
                    perror("close");
                    exit(1);
                }
                break;
            }
            
            
        }
        // sleep(10); 
    }


    if (username) free(username);
    return 0;
}