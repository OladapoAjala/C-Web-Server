/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490"  // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 65536;
    char response[max_response_size];
    
    time_t rawtime;
    struct tm *time_date;
    char *date;

    time( &rawtime );

    time_date = localtime( &rawtime );
    date = asctime(time_date);
    
    // Build HTTP response and store it in response
    sprintf(response, "%s\nContent-Type: %s\nDate: %d\n  %s\n", header, content_type, date, body);
    
    int response_length = strlen(response);

    // Send it all!
    int rv = send(fd, response, content_length, 0);

    if (rv < 0) {
        perror("send");
    }

    return rv;
}


/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    time_t t;
    /*Initialize random number generator*/
    srand((unsigned) time(&t));

    /*Generate a random number between 1 and 20 inclusive*/
    unsigned int r = rand() % 20;
    printf("The random number generated is: %d", r);
    
    char s [2048] = "<!DOCTYPE HTML>\r\n\n<html>\r\n<head><title>get_d20()</title></head><body><h1>Random Number Generator</h1>"; 
    char ss [512] = {};
    snprintf(ss, 1024, "<p>The random number generated is: %d</p></body></html>", r); 
    strcat(s, ss);

    // Use send_response() to send it back as text/plain data
    send_response(fd, "HTTP/1.1 200 OK", "text/html", s, 2048);
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[4096];
    struct file_data *filedata; 
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);

    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

    file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path)
{ 
    struct file_data *filedata; 
    char *mime_type;
    printf("Path: %s\n", request_path);

    struct cache_entry *ce;
    ce = cache_get(cache, request_path);

    if(ce == NULL){
        printf("%s", "File not found in the cache\n");
        filedata = file_load(request_path);
    
        if (filedata == NULL) {
            // TODO: make this non-fatal
            fprintf(stderr, "cannot find system file\n");
            exit(3);
        }

        mime_type = mime_type_get(request_path);

        send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);
        cache_put(cache, request_path, mime_type, filedata->data, filedata->size);

        file_free(filedata);
    }
    else {
        printf("%s", "data served from cache");
        mime_type = ce->content_type;

        send_response(fd, "HTTP/1.1 200 OK", mime_type, ce->content, ce->content_length);

    }
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header)
{
    ///////////////////
    // IMPLEMENT ME! // (Stretch)
    ///////////////////
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(int fd, struct cache *cache)
{
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }
    puts(request);

    // Read the three components of the first request line
    char s1[512], s2[512], s3[512];
    sscanf(request, "%s %s %s", s1, s2, s3);
    
    // If GET, handle the get endpoints
    if(!strcmp(s1, "GET")) {
        if(!strcmp(s2, "/")){
            printf("%s", "get the home page\n");
             
             // Fetch the index.html file
            char filepath[4096];
            snprintf(filepath, sizeof filepath, "%s/index.html", SERVER_ROOT);
            get_file(fd, cache, filepath);
        }
        else if(!strcmp(s2, "/d20")) {
            printf("%s", "get_d20\n");
            get_d20(fd);
        } 

        else {
            resp_404(fd);
            printf("%s", "resp_404()\n");
        }
    }

    // (Stretch) If POST, handle the post request
}

/**
 * Main
 */
int main(void)
{
    int newfd;  // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 0);

    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // forks a handler process to take care of it. The main parent
    // process then goes back to waiting for new connections.
    
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.

        handle_http_request(newfd, cache);

        close(newfd);
    }

    // Unreachable code

    return 0;
}

