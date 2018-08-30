all: rpsls_client.c rpsls_server.c
	gcc -Wall -std=gnu99 -g -o rpsls_client rpsls_client.c 
	gcc -Wall -std=gnu99 -g -o rpsls_server rpsls_server.c 

rpsls_client: rpsls_client.c
	gcc -Wall -std=gnu99 -g -o rpsls_client rpsls_client.c 

rpsls_server: rpsls_server.c
	gcc -Wall -std=gnu99 -g -o rpsls_server rpsls_server.c 
 