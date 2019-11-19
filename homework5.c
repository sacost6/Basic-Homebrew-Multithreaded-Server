#include <fnmatch.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include<sys/wait.h> 
#define BACKLOG (10)

/* This part of the code is setting up for multithreading 
 *
 * Setting up arguments and function for threads
 */ 

// Use this to pass client file descriptor for server to send responses
struct thread_arg {
	int clientSocket; 
}; 

// global query  variable
char * QUERY[]  ={ "QUERY_STRING"};  
ssize_t rio_writen(int, void *, size_t);
void create_error_page(void);
void serve_request(int);
void create_formatted_string(int, char*);
void create_directory_index(int, char*);
int numClients = 0; 
// function that will be passed to client threads to 
// perform the server request 
void * listener(void * arg) {
  struct thread_arg *my_argument = (struct thread_arg *) arg;
  int client_sock = my_argument->clientSocket;
  free(arg);
  serve_request(client_sock); 
  close(client_sock); 
  return NULL;
}

/*
 * Variables used to check type of file requested
 *
 */ 
const char * GIF = ".gif";
const char * PNG = ".png"; 
const char * JPEG = ".jpeg";
const char * JPG = ".jpg";
const char * CGI = "cgi";
const char * PDF = ".pdf";
const char * HTML = ".html";
/*
 * Directory for server to pull resources
 */ 
static char root[1000];

//////////////////////////////////////////////
void serve_request(int);

char * pdf_request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: application/pdf; charset=UTF-8\r\n\r\n";

char * gif_request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/gif; charset=UTF-8\r\n\r\n";

char * png_request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/png; charset=UTF-8\r\n\r\n";

char * jpeg_request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: image/jpeg; charset=UTF-8\r\n\r\n";

char * request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * error_request_str =  "HTTP/1.0 404 NOT FOUND\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * cgi_request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/plain; charset=UTF-8\r\n\r\n";

char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>\n"
        "<title>Directory listing for %s</title>\n"
"<body>\n"
"<h2>Directory listing for %s</h2><hr><ul>\n";

// snprintf(output_buffer,4096,index_hdr,filename,filename);


char * index_body = "<li><a href=\"%s\">%s</a>\n";

char * index_ftr = "</ul><hr></body></html>\n";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char* parseRequest(char* request) {
  //assume file paths are no more than 256 bytes + 1 for null. 
  char *buffer = malloc(sizeof(char)*257);
  memset(buffer, 0, 257);
  
  if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0; 
  sscanf(request, "GET %s HTTP/1.", buffer);
  return buffer; 
}

int get_index_file(char * targetDir) {
	
	DIR * path = opendir(targetDir);
	struct stat file_stat; 
	struct dirent* underlying_file = NULL;
	if(path != NULL) {
		while((underlying_file = readdir(path)) != NULL) {
			if(strstr(underlying_file->d_name, "index.html")) {return 1;}
		}
		closedir(path);
	}
	return 0;
}

char * parse_directory(char * requested_filename) {
	char filename[4096]; 
	char *result = "sike"; 
	 
	strcpy(filename, requested_filename); 
	result = strtok(filename, "/");	

	if(result == NULL) {
		result = "NONE"; 
		return result;
	}

	return result; 
}

void serve_request(int client_fd) {
  
  int read_fd;
  int bytes_read;
  int file_offset = 0;
  char client_buf[4096];
  char send_buf[4096];
  char filename[4096];
  char * requested_file;
  char *directory;
  memset(client_buf,0,4096);
  memset(filename,0,4096);

  while(1){
   
    file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
    if(strstr(client_buf,"\r\n\r\n"))
      break;
  }

  requested_file = parseRequest(client_buf); 
  int index = 0;
  filename[0] = '.';
  strcpy(&filename[1],requested_file); 
  struct stat file_stat;
   
    // take requested_file, add a . to beginning, open that file
  if(strstr(filename, root)) {
	char* temp;

	temp = strtok(filename, "/");
	temp = strtok(NULL, "/");
	while(!strstr(temp, root)) {
		temp = strtok(NULL, "/");
	}
	filename[0] = '.';
	filename[1] = '/';
	strcpy(&filename[2], temp);
  }
  if(strstr(requested_file, "form")) {
	send(client_fd, cgi_request_str, strlen(cgi_request_str), 0); 
	create_formatted_string(client_fd, filename); 
  }
  else if((stat(filename, &file_stat) != 0) && (!strstr(filename, root))) {
	send(client_fd, error_request_str, strlen(error_request_str), 0);	
  	char * src = "./error.html";
	strcpy(filename,src);
  }

  else if ((stat(filename, &file_stat) == 0)) {
    
  if(strstr(requested_file, GIF)) {
          send(client_fd, gif_request_str, strlen(gif_request_str), 0); 
    } else if(strstr(requested_file, PNG)) {
	  send(client_fd, png_request_str, strlen(png_request_str), 0); 
    } else if(strstr(requested_file, JPEG) || strstr(requested_file, JPG)) {
  	  send(client_fd, jpeg_request_str, strlen(jpeg_request_str), 0); 
    } else if(strstr(requested_file, PDF)) {
	  send(client_fd, pdf_request_str, strlen(pdf_request_str), 0); 
    } else if(strstr(requested_file, HTML)) {
	   send(client_fd, request_str, strlen(request_str), 0);  
    } else if(strstr(requested_file, ".ico")) {
	 
    } else if(strstr(requested_file, "cgi")) {
    } 
    else {
	directory = S_ISDIR(file_stat.st_mode);
	index = get_index_file(filename); 
	if(directory && index) {
		send(client_fd, request_str, strlen(request_str),0);
		char * src = "/index.html";
		strcat(filename, src);

	 } else if(directory && !index) {
		send(client_fd, request_str, strlen(request_str),0);
	 	create_directory_index(client_fd, filename);
	 }	 
    }
  }
  read_fd = open(filename,0,0);
  while(bytes_read != -1){
    bytes_read = read(read_fd,send_buf,4096);
    if(bytes_read == 0)
      break;
    send(client_fd,send_buf,bytes_read,0);
  }
  close(read_fd);
  --numClients;
  close(client_fd);
  requested_file = NULL;
  return;
  }

void create_formatted_string(int fd, char * request) {
  char * temp, *string1, *string2;
  if(request == NULL){ return; }
  temp = strtok(request, "?"); 
  temp = strtok(NULL, "?");
  string2 = strtok(temp, "&");
  string1 = string2;
  string2 =  strtok(NULL, "&");
  char * MAIN_ARGS[]  = {"format_string",  string1,string2, NULL};
  if(fork() == 0) {
	dup2(fd, STDOUT_FILENO); 
	execvp("../format_string", MAIN_ARGS);
  } 
  wait(NULL);

}

int server_sock;

void signal_handler(int sig) {
	close(server_sock);
} 

void create_error_page(){ 
  FILE * buf = NULL;
  buf = fopen("./error.html", "w");
  fprintf(buf, "<!DOCTYPE html\r\n");
  fprintf(buf, "<html>\r\n");
  fprintf(buf, "<body>\r\n");
  fprintf(buf, "<head>\r\n"
	"<style type=");
  fprintf(buf, "\"text/css\"");
  fprintf(buf, ">\r\n"
	"h2 {\r\n"
	"border-top: 20px solid black;\r\n"
	"}\r\n"
	"</style>\r\n"
	"</head>\r\n");
  fprintf(buf, "<h2>ERROR 404 - !</h2>\r\n"); 
  fprintf(buf, "<p>Error, this page was not found.</p>\r\n");
  fprintf(buf, "</body>\r\n");
  fprintf(buf, "</html>\r\n\r\n");
  fclose(buf);
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}

void create_directory_index(int fd, char *dir) {
	DIR * path = opendir(dir); 
	struct dirent * underlying_file = NULL; 
	char buf[4096], *emptylist[] = { NULL };
	char file[4096];
	memset(file, 0, 4096); 
	if(path == NULL) {
		return;
	}
	sprintf(buf, index_hdr, dir, dir);  
        rio_writen(fd, buf, strlen(buf));
	char temp[4096]; 
	memset(temp, 0 , 4096); 
	strcpy(temp, "");
	strcat(temp, dir);
        strcat(temp, "/");	
	while((underlying_file = readdir(path)) != NULL) {
		char *name = underlying_file->d_name;
		strcpy(file, "");
		strcat(file,temp);
		strcat(file, name);
		sprintf(buf, index_body, file, underlying_file->d_name);
		rio_writen(fd, buf, strlen(buf));
	}
	sprintf(buf, "<li><a href=\"filename.spam\"</a>\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, index_ftr);
	rio_writen(fd, buf, strlen(buf));
	if (fork() == 0) { /* Child */
        /* Real server would set all CGI vars here */
        	close(0);
		dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
        	execve(file,emptylist, QUERY);  /* Run CGI program */
	}
	close(fd);
}
/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
    /* For checking return values. */
    int retval;
    pthread_t clientThread;
    struct thread_arg* argument; 
    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);
    signal(SIGTSTP, signal_handler); 
    /* Create a socket to which clients will connect. */
    server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }
    strcpy(root, argv[2]);
    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }
   
    if(chdir(root) != 0) { 
        printf("Unavailable root folder\n"); 
   }
    char cwd[4096];
    getcwd(cwd, sizeof(cwd));
    printf("%ss\n", cwd);
    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
    
    create_error_page();
    struct sockaddr_in6 addr;   // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces

    
    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if(retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }
    int msec = 0, trigger = 5000; /* 10ms */
    clock_t before = clock();

    struct sockaddr_in remote_addr;
    unsigned int socklen = sizeof(remote_addr); 
    int sock;
    int MAX_THREADS = 1024; 
    pthread_t *threads = malloc(sizeof(pthread_t) * MAX_THREADS);
    int counter = 0; 
    while(sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen)) {
        /* Declare a socket for the client connection. */
        argument = malloc(sizeof(struct thread_arg));
                                                                 
	numClients++; 
        /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */

	
        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        if(sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

	argument->clientSocket = sock;
	
	if((pthread_create(&threads[counter], NULL, listener, (void *) argument) != 0)) {
	    perror("error creating thread\n");
	}
	counter++;
	clock_t difference = clock() - before;
  	msec = difference * 1000 / CLOCKS_PER_SEC;
	if(msec <= 5000) {
	}
        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */

        /* Tell the OS to clean up the resources associated with that client
         * connection, now that we're done with it. */
    }
    for (int i = 0; i < counter; i++) {
        int retval = pthread_join(threads[i], NULL);
        if (retval) {
            printf("pthread_join() failed\n");
            exit(1);
        }
    }
    free(argument);
    free(threads); 
    close(server_sock);
}
