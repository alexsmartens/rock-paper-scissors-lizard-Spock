#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 60000

#define MAX_BACKLOG 5
/* MAX_BACKLOG -> listen() 
how many connections can be waiting for this particular socket at one point of time*/
#define NUM_CLIENTS 2
#define BUF_SIZE 20



struct sockname {
    int sock_fd;
    int client_sock_fd;
    char *username;
    int is_name_set;
    int client_gesture;
    char *client_gesture_buf;
    int is_gesture_recieved;
    int games_won;
};



void close_server_sockets(struct sockname *connections, int max){

    for (int index = 0; index < max; index++){
        if ( connections[index].client_sock_fd > 0 ) {
            if ( -1 == close(connections[index].client_sock_fd) ){
                perror("server-client:close");
           }
        }
        if ( connections[index].sock_fd > 0) {
            if ( -1 == close(connections[index].sock_fd) ){
                perror("server:close");
            }
        }
        free(connections[index].username);
        free(connections[index].client_gesture_buf);
    }
}



int startup_socket(int server_socket_fd, int const requested_port_no){
    
    // Set information about the port (and IP) we want to be connected to
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons (requested_port_no);
    server_address.sin_addr.s_addr = INADDR_ANY; // macros for 0.0.0.0
    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&server_address.sin_zero, 0, 8);

    // Bind socket
    if (-1 == bind( server_socket_fd, (struct sockaddr *) &server_address, sizeof (server_address) ) ){
        perror("server: bind");
        // -- exit and close performed later on -1 returned
        return -1;
    }

    // Listen: announce willingness to accept connections on this socket
    if ( -1 == listen(server_socket_fd, MAX_BACKLOG) ){
        /* MAX_BACKLOG - how many connections can be waiting for this particular socket at one point of time*/
        perror("server: listen");
        // -- exit and close performed later on -1 returned
        return -1;
    }
    return 0;
}



void add_client (int *num_avalable_ports_p, struct sockname *connections, fd_set const *listen_fds_p, fd_set *all_fds_p, int *max_fd){

    // Iterate over potential ports if at least one is available
    for (int index = 0; index < NUM_CLIENTS; index++){
        if (!connections[index].client_sock_fd) {
            // Is it the server socket? Create a new connection ...
            if (FD_ISSET(connections[index].sock_fd, listen_fds_p)) {


                // Accept conneection and add new client
                connections[index].client_sock_fd = accept(connections[index].sock_fd, NULL, NULL);
                if ( -1 == connections[index].client_sock_fd ){
                    perror("server: accept");
                    close_server_sockets(connections, NUM_CLIENTS);
                    exit(1);
                }


                // Update port data 
                *num_avalable_ports_p = *num_avalable_ports_p - 1;
                // -- add client fd to the set
                if (connections[index].client_sock_fd > *max_fd) {
                    *max_fd = connections[index].client_sock_fd;
                }
                FD_SET(connections[index].client_sock_fd, all_fds_p);
                 

                // Close current server socket and clear it from the set
                if ( -1 == close (connections[index].sock_fd) ) {
                    perror("server: close sock_fd");
                    close_server_sockets(connections, NUM_CLIENTS);
                    exit(1);
                }   
                // -- remove server fd from the set
                FD_CLR(connections[index].sock_fd, all_fds_p);
                // -- define the client socket as inactive
                connections[index].sock_fd = -1;


                /* printf("Accepted client %d connection to %d socket\n", 
                            connections[index].client_sock_fd,
                            connections[index].sock_fd ); */
            }
        }
    }

}



void set_client_name (struct sockname *connections, int index, char *buf, int *num_received_client_names_p) {
    // Error check of username lenght
    if ( strlen(connections[index].username) + strlen(buf) > BUF_SIZE + 3) {
        fprintf (stderr, "Client's name is too long. This should never happen\n");
        close_server_sockets(connections, NUM_CLIENTS);
        exit(1);
    }
    // Append username if not received in full at once
    strcat(connections[index].username, buf);
    connections[index].username[ strlen(connections[index].username) ] = '\0';
    // Check if full username received
    char *str_end = strstr(connections[index].username, "\r\n");
    if (str_end) {
        *str_end = '\0';
        connections[index].is_name_set = 1; // full username string is received

        // printf("%s connected on soc %d\n", connections[index].username, connections[index].client_sock_fd) 
        /* printf("Set name for socket %d, client %d, name %s\n", connections[index].sock_fd ,
        connections[index].client_sock_fd, connections[index].username);*/
    }
    *num_received_client_names_p = *num_received_client_names_p + 1;
}



char *generate_welcome_message(struct sockname *connections){
    char *initial = "Game: ";
    char *between = " vs ";
    char *welcome = malloc ( sizeof(char) * ( strlen(initial) + 
                    NUM_CLIENTS * BUF_SIZE + (NUM_CLIENTS - 1) * strlen(between) + 3)  );
    welcome[0] = '\0';
    
    strcat (welcome, initial);
    // Add client names to the welcome message
    for (int index = 0; index < NUM_CLIENTS; index++){
        strcat (welcome, connections[index].username);
        // Do not add between after the last client name
        if ( (index + 1) < NUM_CLIENTS ) {
            strcat (welcome, between);
        }
    }

    // welcome[strlen(welcome) + 1] = '\n';
    strcat(welcome, "\r\n\0");

    // printf( "strlen(welcome) = %d\n", (int) strlen(welcome) );
    // printf("%s\n", welcome); 
    return welcome;
}



void send_message(struct sockname *connections, char *message){
    // each message should be less then BUF_SIZE * NUM_CLIENTS * 2 - 1 (or the change in client code is required)
    if ( strlen(message) > BUF_SIZE * NUM_CLIENTS * 2 - 1 ) {
        fprintf (stderr, "Message is too long. This should never happen\n");
        close_server_sockets(connections, NUM_CLIENTS);
        exit(1);
    } else {
        // Send the message to each client 
        for (int index = 0; index < NUM_CLIENTS; index++){
            int write_length = write( connections[index].client_sock_fd, message, strlen(message) + 1 );  
            if ( -1 ==  write_length) {
                perror("server: write");
                close_server_sockets(connections, NUM_CLIENTS);
                exit(1);
            } else if ( ( (int) strlen(message) + 1 ) !=  write_length) {
                fprintf(stderr, "server: write n\n");
                close_server_sockets(connections, NUM_CLIENTS);
                exit(1);
            }
            // printf("m> name = %s, soc= %d\n", connections[index].username, connections[index].client_sock_fd);
        } 
        // printf("sever message: %s\n", message);
    }
}

                        

int set_client_command (struct sockname *connections, int index, char *buf, int *num_received_gestures_p) {
    // Error check of username lenght
    if ( strlen(connections[index].client_gesture_buf) + strlen(buf) > 1 + 3) {
        fprintf (stderr, "Client's command is too long. This should never happen\n");
        close_server_sockets(connections, NUM_CLIENTS);
        exit(1);
    }
    // Append command if not received in full at once
    strcat(connections[index].client_gesture_buf, buf);
    connections[index].client_gesture_buf[ strlen(connections[index].client_gesture_buf) ] = '\0';
    // Check if full command received
    char *str_end = strstr(connections[index].client_gesture_buf, "\r\n");
    if (str_end) {
        *str_end = '\0';
        if ( connections[index].client_gesture_buf[0] == 'e' ){
            return 1;
        }
        connections[index].client_gesture = (int) connections[index].client_gesture_buf[0]; // full command string is received
        
        // printf(">>> %s ::: %c\n",  connections[index].username, connections[index].client_gesture);
    }
    *num_received_gestures_p = *num_received_gestures_p + 1;
    return 0;
}



  void clean_commands (struct sockname *connections, int *num_received_gestures_p){

    for (int index = 0; index < NUM_CLIENTS; index++){
        connections[index].client_gesture = 0;
        connections[index].client_gesture_buf[0] = '\0';
    }

    *num_received_gestures_p = 0;
} 



int play_game (struct sockname *connections, int const *arr_gest_idx){
    // Array received gestures
    int *arr_gest = malloc( sizeof(int) * NUM_CLIENTS );
    // Array of winners
    int *win_arr = malloc( sizeof(int) * NUM_CLIENTS );
    // --- make all int bits to be 0
    memset(arr_gest, 0, sizeof(int) * NUM_CLIENTS );
    memset(win_arr, 0, sizeof(int) * NUM_CLIENTS );
    

    // Winners count
    int win_count = NUM_CLIENTS;
    // Winner index
    int win_idx = -1;
    
    
    // Set each player gesture   
    for (int index = 0; index < NUM_CLIENTS; index++){
        arr_gest[index] = arr_gest_idx[connections[index].client_gesture];
        win_arr[index] = 1;
    }

    // Check winners
    int gest;
    for (int index = 0; index < NUM_CLIENTS; index++){
        gest = arr_gest[index];
        // Compare current gesture to all the rest
        for (int idx_check = 0; idx_check < NUM_CLIENTS; idx_check++){
            if ( (gest) == arr_gest[idx_check] ) {
                // no one wins
            } else if ( (5 + gest + 1) % 5 == arr_gest[idx_check] || 
                        (5 + gest + 3) % 5 == arr_gest[idx_check] ) {
                // gest wins
                if ( win_arr[idx_check] ) win_count--;
                win_arr[idx_check] = 0;

            } else {
                // gest loses
                if ( win_arr[index] ) win_count--;
                win_arr[index] = 0;
            }
        }
    }

    // Record winner's score
    if ( win_count == 1 ) {
        for (int index = 0; index < NUM_CLIENTS; index++){
            if ( win_arr[index] == 1 ) {
               connections[index].games_won = connections[index].games_won + 1;
               win_idx = index;
            }
        }
    } 

    free(arr_gest);
    // This array stores information about the winner or winners.
    // It might be useful in a situation when the information about multiple winners is required
    free(win_arr);

    // printf(" WIN -- index = %d", win_idx);
    return win_idx;
}



char *generate_game_status_message(struct sockname const *connections, int num_games_played, int win_idx) {
    char *message;
    
    if ( win_idx < 0) {
        message = malloc ( sizeof (char) * ( strlen("tie") + 3) );
        message[0] = '\0';

        strcat(message, "tie");

    } else {
        message = malloc ( sizeof (char) * ( strlen(connections[win_idx].username) + strlen(" wins") + 3) );
        message[0] = '\0';

        strcat(message, connections[win_idx].username );
        strcat(message, " wins" );
    }
    strcat(message, "\r\n\0");
    return message;
}



char *generate_statistics_message(struct sockname const *connections, int num_games_played){
    char *initial = "Game end: ";
    char *message = malloc ( sizeof(char) * ( strlen(initial) + BUF_SIZE + strlen(" wins ") + (num_games_played/10 + 1) *(NUM_CLIENTS + 1 ) + strlen(", games played: ") + 3)  );
    message[0] = '\0';
    strcat(message, initial);
    char buf[num_games_played/10 + 1 + 1];
    buf[0] = '\0';
    int max_idx = 0;
    int num_winners = 1;
    // Find max score
    for (int index = 0; index < NUM_CLIENTS; index++){
        if (connections[index].games_won > connections[max_idx].games_won) {
            max_idx = index;
        }
    }
    // Find if there is any other player who has the same score
    for (int index = 0; index < NUM_CLIENTS; index++){
        if ( index!= max_idx && 
            connections[index].games_won == connections[max_idx].games_won) {
                num_winners ++;
            }
    }
    // Generate the first (name) part of the message
    if ( num_winners == 1) {
        strcat(message, connections[max_idx].username);
        strcat(message, " wins ");
    } else {
        strcat(message, "tie ");
    }
    // Add client's score
    buf[0] = '\0'; 
    sprintf(buf, "%d", connections[max_idx].games_won);
    strcat(message, buf);
    strcat(message, "-");


    // Generate score statistics
    int clients_left = NUM_CLIENTS - 1; // number of scores to be printed
    for (int index = 0; index < NUM_CLIENTS; index++){
        if ( index != max_idx ) {
            // Add client's score
            buf[0] = '\0'; 
            sprintf(buf, "%d", connections[index].games_won);
            strcat(message, buf);

            clients_left--;

            // Do not add "-" after the last client name
            if ( clients_left > 0 ) {
                strcat (message, "-");
            }
        }
    }

    // Add total number of games played
    strcat(message, ", games played: ");    
    buf[0] = '\0'; 
    sprintf(buf, "%d", num_games_played);
    strcat(message, buf);

    strcat(message, "\r\n\0");
    return message;
}










int main (int argc, char **argv) {
    int port_offset = 0;
    if (argc == 2) {
        // Set port offset
        char * endptr;
        errno = 0;    /* To distinguish success/failure after call */
        port_offset = (int) strtol(argv[1], &endptr, 0);
        if (  *endptr  != '\0' ) {
            fprintf(stderr, "Improper port offset value, safe port range is (1024;65535)\n");
            exit(1);
        }
        if (errno) {
            perror("server: strtol");
            exit(1);
        }

        // Port number check
        if ( PORT + port_offset < 1024 || PORT + port_offset > 65535) {
            fprintf(stderr, "Chosen port %d is outside of safe ports range (1024;65535)\n", PORT + port_offset);
            exit(1);
        }
    } else if (argc > 2) {
        fprintf(stderr, "Usage: rpsls_server port_offset(optional)\n");
        exit(1); 
    }


    // Declaration of Server-Client rules
    // in cass when a change to this variables is required, it is also to be changed in client code
    // -- Message for requesting custumers input 
    char *msg_rquest = "Your hand gesture:\r\n"; // without "\r\n" in client implementation

    // -- Commands indexes
    int arr_gest_idx[128];
    // --- make all int bits to be 0
    memset(arr_gest_idx, 0, sizeof(int) * 128 );
    arr_gest_idx['s'] = 0; // scissors
    arr_gest_idx['p'] = 1; // paper
    arr_gest_idx['r'] = 2; // rock
    arr_gest_idx['l'] = 3; // lizard
    arr_gest_idx['S'] = 4; // Spock
    // arr_gest_idx['e'] = 2; // -- END


    // Initialize a set of server/client sockets
    int max_fd = 0;
    struct sockname connections[NUM_CLIENTS];
    for (int index = 0; index < NUM_CLIENTS; index++){
        // Create socket
        connections[index].sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if ( -1 == connections[index].sock_fd ){
            perror("server: socket");
            exit(1);
        }

        connections[index].username = malloc(sizeof(char) *(BUF_SIZE + 3) ); 
        connections[index].username[0] = '\0';
        connections[index].is_name_set = 0; // = 1 when full username string is received
        connections[index].client_sock_fd = 0; // actual file descriptor is a non-negative integer
        connections[index].client_gesture_buf = malloc(sizeof(char) *(1 + 3) ); 
        connections[index].client_gesture_buf[0] = '\0';
        connections[index].client_gesture = 0; // non-negative when a full string with the gesture is received
        connections[index].games_won = 0;


        // Bind and listen from the socket
        if ( -1 == startup_socket(connections[index].sock_fd, PORT + port_offset + index) ){
            close_server_sockets(connections, index + 1);
            exit(1);
        }
        
        // Updated max socket fd
        if ( connections[index].sock_fd > max_fd ) {
            max_fd = connections[index].sock_fd;
        }

        /*printf("Created socket %d, client %d, name %s\n", connections[index].sock_fd ,
        connections[index].client_sock_fd, connections[index].username); */
    }


    // Prepare to read ( initialize port set data )
    fd_set all_fds; // set of file descriptors to read from
    FD_ZERO(&all_fds); // set all bits to be 0
    for ( int index = 0; index < NUM_CLIENTS; index++){
        // Add each fd to the set
        FD_SET(connections[index].sock_fd, &all_fds);
    }


    // Process control variables
    int num_avalable_ports = NUM_CLIENTS;
    int num_received_client_names = 0;
    int num_received_gestures = 0;
    int num_games_played = 0;


    // Read
    while (1){
        // printf ("--- server -> while\n");
        

        // Update the set of descriptors
        // -- select updates the fd_set it receives, so we always use a copy and retain the original.
        fd_set listen_fds = all_fds;
        int nready = select ( max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if ( -1 == nready ) {
            perror("server: select");
            close_server_sockets(connections, NUM_CLIENTS);
            exit(1);
        }


        // Accept conneection and add new CLIENTS 
        // if at least one port is available
        if ( num_avalable_ports > 0) {
            add_client ( &num_avalable_ports, connections, &listen_fds, &all_fds, &max_fd);
        } 


        // Read from clients
        for (int index = 0; index < NUM_CLIENTS; index++){
            if ( connections[index].client_sock_fd > 0 && FD_ISSET(connections[index].client_sock_fd, &listen_fds) ) {
                
                // Initialize name / game gesture buffer 
                char buf[BUF_SIZE + 3];
                buf[0] = '\0';
                
                int num_read = read (connections[index].client_sock_fd, &buf, BUF_SIZE + 3);
                if ( -1 == num_read ) {
                    perror("server: read");
                    close_server_sockets(connections, NUM_CLIENTS);
                    exit(1);
                }
                

                if ( num_read > 0 ) {
                    buf[num_read] = '\0';
                
                    if ( !connections[index].is_name_set ) {
                        // Set up client name
                        set_client_name (connections, index, buf, &num_received_client_names);
                        
                        // send welcome message to the clients
                        if ( num_received_client_names == NUM_CLIENTS ) {
                            char *welcome = generate_welcome_message(connections);
                            
                            // char *message1 = malloc ( sizeof(char) * (strlen (msg_rquest) + strlen (welcome) + 1) );
                            // message1[0] = '\0';

                            // printf("dsfdsfffffffffffffffffffffffff\n");
                            // strcat(message1, welcome);
                            // strcat(message1, msg_rquest);
                            // send_message(connections, message1);


                            send_message(connections, welcome);
                            free(welcome);
                            send_message(connections, msg_rquest);
                            // free(message1);
                        } else if ( num_received_client_names > NUM_CLIENTS ) {
                            fprintf (stderr, "Too many clients. This should never happen\n");
                            close_server_sockets(connections, NUM_CLIENTS);
                            exit(1);
                        }

                    } else {
                        // Game communication
                        if (!connections[index].client_gesture) {
                            // set_client_command (connections, index, buf, &num_received_gestures);  
                            // if (connections[index].client_gesture == 'e') {

                            if ( set_client_command (connections, index, buf, &num_received_gestures) ) {
                                // on e (EXIT) received
                                
                                // Send the game info message
                                char *msg_stat = generate_statistics_message(connections, num_games_played);
                                send_message(connections, msg_stat);
                                free(msg_stat);
                                close_server_sockets(connections, NUM_CLIENTS);
                                return 0;
                            }

                        } else {
                            fprintf (stderr, "A command received second time. This should never happen\n");
                            close_server_sockets(connections, NUM_CLIENTS);
                            exit(1);
                        }
                    }


                } else {
                    // Client exits unexpectedly 

                    // End communication 
                    close_server_sockets(connections, NUM_CLIENTS);
                    exit(1);

                    // // Remove client fd from the set
                    // FD_CLR(connections[index].client_sock_fd, &all_fds);
                    // // Close current client socket
                    // if ( -1 == close (connections[index].client_sock_fd) ) {
                    //     perror("server: close client sock_fd");
                    //     close_server_sockets(connections, NUM_CLIENTS);
                    //     exit(1);
                    // }
                    // // Define the client socket as inactive
                    // connections[index].client_sock_fd = -1;
                }
            }
        }



        // Play game (gesture communication with the clients)
        if (num_received_gestures == NUM_CLIENTS) {

            // Find out the results
            int win_idx = play_game(connections, arr_gest_idx);
            num_games_played++;
            
            // Send the game info message
            char *msg_game = generate_game_status_message(connections, num_games_played, win_idx);
            send_message(connections, msg_game);
            free(msg_game);

            // Clean game data (individual clients)
            clean_commands (connections, &num_received_gestures);
            // Send message inviting to play one more game
            send_message(connections, msg_rquest); 

        } else if ( num_received_gestures > NUM_CLIENTS ){
            fprintf (stderr, "Too many commands received. This should never happen\n");
            close_server_sockets(connections, NUM_CLIENTS);
            exit(1);
        }

    }


    fprintf (stderr, "return 0. This should never happen\n");            
    return 1;
}