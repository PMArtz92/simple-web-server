#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

//send files through socket
void send_new(int fd, char *msg) {
 int len = strlen(msg);
 if (send(fd, msg, len, 0) == -1) {
  printf("Error in send\n");
 }
}

//get file size
int get_file_size(int fd) {
 struct stat stat_struct;
 if (fstat(fd, &stat_struct) == -1)
  return (1);
 return (int) stat_struct.st_size;
}

//php function to serve php pages
void php_cgi(char* script_path, int fd) {
 send_new(fd, "HTTP/1.1 200 OK\n Server: Web Server in C\n Connection: close\n");
 dup2(fd, STDOUT_FILENO);
 char script[500];
 strcpy(script, "SCRIPT_FILENAME=");
 strcat(script, script_path);
 putenv("GATEWAY_INTERFACE=CGI/1.1");
 putenv(script);
 putenv("QUERY_STRING=");
 putenv("REQUEST_METHOD=GET");
 putenv("REDIRECT_STATUS=true");
 putenv("SERVER_PROTOCOL=HTTP/1.1");
 putenv("REMOTE_HOST=127.0.0.1");
 execl("/usr/bin/php-cgi", "php-cgi", NULL);
}

int main(int argc, char *argv[]){
  struct sockaddr_in server_addr, client_addr;
  socklen_t sin_len = sizeof(client_addr);
  int fd_server, fd_client;
  char buf[2048], *ptr, temp[500]; //use buffer to keep request, use temp for keep temporary contents
  int fd1,length;
  int on = 1;

  fd_server = socket(AF_INET, SOCK_STREAM, 0);
  if(fd_server < 0){
    perror("socket");
    exit(1);
  }

  setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(8000); //port that runs the server

  if(bind(fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){
    perror("bind");
    close(fd_server);
    exit(1);
  }
  if(listen(fd_server,10) == -1){
    perror("listen");
    close(fd_server);
    exit(1);
  }
  printf("%s\n","socket created..!" );

  while(1){
    fd_client = accept(fd_server, (struct sockaddr *) &client_addr, &sin_len);
    if(fd_client == -1){
      perror("connection failed");
      continue;
    }
    printf("%s\n","Client connection success..!" );
    printf("%s\n","Server runs on localhost:8000" );

    //use fork to handle continuos requests
    if(!fork()){
      close(fd_server);
      memset(buf, 0, 2048);
      read(fd_client, buf,2047);
      printf("%s\n",buf);
      ptr = strstr(buf, " HTTP/");
      //check request is a HTTP request
      if (ptr == NULL) {
       printf("NOT HTTP\n");
      }
      else{
        *ptr = 0;
        ptr = NULL;
        if (strncmp(buf, "GET ", 4) == 0) {
         ptr = buf + 4;
        }
        if (ptr == NULL) {
         printf("Unknown Request ! \n");
        }
        else{
          if (ptr[strlen(ptr) - 1] == '/') {
           strcat(ptr, "index.html"); //serve index file when there is no other page request
          }
          strcpy(temp, ".");
          strcat(temp, ptr);
          char* s = strchr(ptr, '.');

          fd1 = open(temp, O_RDONLY, 0);
          printf("Opening \"%s\"\n", temp);

          if (fd1 == -1) { //send 404 for files that not in server
            printf("404 File not found Error\n");
            send_new(fd_client, "HTTP/1.1 404 Not Found\r\n");
            send_new(fd_client, "Server : Web Server in C\r\n\r\n");
            send_new(fd_client, "<html><head><title>404 Not Found</head></title>");
            send_new(fd_client, "<body><p>404 Not Found: The requested resource could not be found!</p></body></html>");
            //Handling php requests
          }

          if(strcmp(s+1,"php") == 0){
            printf("%s\n", "php send");
            php_cgi(temp,fd_client);
            sleep(1);
            close(fd_client);
            exit(1);
          }
          else{
            send_new(fd_client, "HTTP/1.1 200 OK\r\n");
            send_new(fd_client, "Server : Web Server in C\r\n\r\n");
            if (ptr == buf + 4) // if it is a GET request
              {
             if ((length = get_file_size(fd1)) == -1)
              printf("Error in getting size !\n");
             size_t total_bytes_sent = 0;
             ssize_t bytes_sent;
             //this will make sure to send complete file
             while (total_bytes_sent < length) {
              if ((bytes_sent = sendfile(fd_client, fd1, 0,
                length - total_bytes_sent)) <= 0) {
               if (errno == EINTR || errno == EAGAIN) {
                continue;
               }
               perror("sendfile");
               return -1;
              }
              total_bytes_sent += bytes_sent;
             }
             printf("%s\n", "file send");
            }
          }
        }
      }
      close(fd_client);
      printf("%s\n","closing....!" );
      exit(0);
    }
    close(fd_client);
  }
  return 0;
}
