#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

const short unsigned int portnum = 1234;

void complain(int sock) {
    char buf[1000];
    strcpy(buf, "500 Not logged in\n");
    send(sock, buf, strlen(buf), 0);
}

void do_yo(int sock) {
    const int NOT_AUTH = 0, LOGGED_IN = 1;

    char buf[1000], username[50];

    int state;

    // Open connection to database
    sqlite3 *db;
    char* dbname = "yo.db";
    struct stat buffer;
    if (stat(dbname,&buffer) < 0 || sqlite3_open(dbname, &db) < 0) {
        printf("Can't open database\n");
	exit(1);
    }

    // Send greeting
    strcpy(buf, "250 yo, hello\n");
    send(sock, buf, strlen(buf), 0);

    state = NOT_AUTH;

    while (1) {

        // Wait for input from user

        int recvd = recv(sock, buf, 1000, 0);
        buf[recvd] = '\0';

        if (!strncmp("NOYO", buf, 4)) {

	    close(sock);
            sqlite3_close(db);
            printf("Session closed\n");
	    exit(0);

	} else if (!strncmp("MEIS", buf, 4)) {

            // Log user in and INSERT into database

            if (sscanf(buf, "MEIS %s ", username) != 1) {
                strcpy(buf, "500 No user specified\n");
                send(sock, buf, strlen(buf), 0);
            }
            
            printf("User is %s\n", username);
            
            // Enter username into database
            char query[1000];
            sprintf(query, "INSERT INTO users (uname) VALUES ('%s'); COMMIT;", username);
            sqlite3_exec(db, query, NULL, NULL, NULL);

            // Send back confirmation
            strcpy(buf, "200 Logged in\n");
            send(sock, buf, strlen(buf), 0);
            state = LOGGED_IN;

	} else if (!strncmp("YOSR", buf, 4)) {

            // List logged in users

	    if (state == NOT_AUTH) {
                complain(sock);
                continue;
            }
            
	    strcpy(buf, "300 All users\n");
	    send(sock, buf, strlen(buf), 0);
            printf("%s listing users\n", username);

	    // Query database for all users
	    sqlite3_stmt* stmt;
	    char* query = "SELECT uname FROM users ORDER BY uname";
	    sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);

	    // Display list to client
	    while (sqlite3_step(stmt) == SQLITE_ROW) {
	        sprintf(buf, "%s\n", (char*)sqlite3_column_text(stmt,0));
		send(sock, buf, strlen(buf), 0);
	    }
            sqlite3_finalize(stmt);
	    // Send "."
	    strcpy(buf, ".\n");
	    send(sock, buf, strlen(buf), 0);

	} else if (!strncmp("YOYO", buf, 4)) {

            // Message other users

	    if (!state) {
                complain(sock);
                continue;
            }
            
            // Queue yo for someone
	    char query[1000], to[995];
        
            if (sscanf(buf, "YOYO %s ", to) != 1) { // count
	        strcpy(buf, "500 No user specified");
                send(sock, buf, strlen(buf), 0);
		continue;
	    }

	    printf("%s sending yo to %s\n", username, to);

	    // Check if user exists
	    sprintf(query, "SELECT COUNT(*) FROM USERS WHERE uname = '%s'", username);
            sqlite3_stmt* stmt;
	    sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
	    sqlite3_step(stmt);
	    if (sqlite3_column_int(stmt,0) == 0) { // user count
	        strcpy(buf, "404 User does not exist\n");
		send(sock, buf, strlen(buf), 0);
		continue;
	    }
            sqlite3_finalize(stmt);

	    sprintf(query, "INSERT INTO yos (yofrom, yoto) VALUES ('%s', '%s'); COMMIT;", username, to);
            sqlite3_exec(db, query, NULL, NULL, NULL);

    	    strcpy(buf, "200 Yo queued\n"); 
	    send(sock, buf, strlen(buf), 0);

	} else if (!strncmp("YOLO", buf, 4)) {

            // Read new messages

	    if (state == NOT_AUTH) {
                complain(sock);
                continue;
            }
            
	    strcpy(buf, "300 Here are your yos\n");
	    send(sock, buf, strlen(buf), 0);
    
            printf("%s getting yos\n", username);

	    sqlite3_stmt* stmt;
            char query[1000];

            // Display list to client
            sprintf(query, "SELECT yofrom FROM yos WHERE yoto = '%s' ORDER BY id", username);
	    sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
	    while (sqlite3_step(stmt) == SQLITE_ROW) {
	        sprintf(buf, "%s\n", (char*)sqlite3_column_text(stmt, 0));
		send(sock, buf, strlen(buf), 0);
	    }
            sqlite3_finalize(stmt);
            // Send "."
	    strcpy(buf, ".\n");
	    send(sock, buf, strlen(buf), 0);

	    // Delete the yos
	    sprintf(query, "DELETE FROM yos WHERE yoto = '%s'; COMMIT;", username);
	    sqlite3_exec(db, query, NULL, NULL, NULL);

	} else {

            // Send error message
	    strcpy(buf, "500 Command not recognized\n");
	    send(sock, buf, strlen(buf), 0);

	}
    }
    sqlite3_close(db);
}

int main(int argc, char* argv[]) {

    int sockfd, newsockfd; // socket file descriptor

    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // create socket and set address

    if (sockfd < 0) {
	printf("Can't open socket.\n");
	exit(1);
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portnum);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int res = bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); // bind socket

    if (res < 0) {
        perror("Can't bind socket");
        exit(1);
    }

    listen(sockfd, 5); // listen for connection

    struct sockaddr_in cli_addr;
    
    int cli_len = sizeof(cli_addr);

    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, (socklen_t*)&cli_len); // accept connection

        int pid = fork();

        if (pid < 0) {
            perror("Could not fork");
            exit(1);
        } else if (pid == 0) { // child process
            close(sockfd); // close parent socket
            do_yo(newsockfd); // hand off to function
        } else { // parent process
            close(newsockfd);
        }
    }
}

