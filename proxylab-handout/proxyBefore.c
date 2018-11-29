/*
 * proxy.c - A simple, iterative HTTP/1.0 Web server that uses the GET method
 * to serve static and dynamic content.
 */
#define __MAC_OS_X
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#include "csapp.h"
#include <stdio.h>

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


void	doit      (int fd);
void	read_requesthdrs(rio_t * rp);
int		parse_uri  (char *uri, char *filename, char *cgiargs);
void	serve_static(int fd, char *filename, int filesize);
void	get_filetype(char *filename, char *filetype);
void	serve_dynamic(int fd, char *filename, char *cgiargs);
void 	clienterror(int fd, char *cause, char *errnum,
	    	char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
	int		listenfd  , connfd;
	char		hostname  [MAXLINE], port[MAXLINE];
	socklen_t	clientlen;
	struct sockaddr_storage clientaddr;

	/* Check command line args */
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	
	//Opens a server on port argv[1]
	listenfd = Open_listenfd(argv[1]);
	printf("Proxy listening on port %s\n", argv[1]);

	while (1)
	{
		clientlen = sizeof(clientaddr);
		//connfd can now be used to communicate with client
		connfd = Accept(listenfd, (SA *) & clientaddr, &clientlen);

		Getnameinfo((SA *) & clientaddr, clientlen, hostname, MAXLINE,
				    port, MAXLINE, 0);
		
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		doit(connfd);
		Close(connfd);
	}
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
	int	is_static;
	struct stat	sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	
	char host[MAXLINE], path[MAXLINE];
	char existingHeaders[MAXLINE];
	char finalRequest[MAXLINE];
	int hasHost = 0;
	int hasUA = 0;
	int hasConn = 0;
	int hasPro = 0;
	
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t	rio;

	/* Read request line and headers */
	Rio_readinitb(&rio, fd);
	if (!Rio_readlineb(&rio, buf, MAXLINE))
		return;

	printf("This is buf: %s\n", buf);
	
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET")) {
		clienterror(fd, method, "501", "Not Implemented",
				    "Proxy does not implement this method");
		return;
	}

	/* Tell me if it's static or not */
	is_static = parse_uri(uri, filename, cgiargs);
	
	//Parse URI from GET request, get hostname, port, and path
	parseURI(is_static, uri, host, &port, path);

	//now read existing headers
	//Tell me if there was already a host, ua, conn, proxy header
	read_requesthdrs(&rio, existingHeaders, &hasHost, &hasUA, &hasConn, &hasPro);

	//start building request
	//Write GET, path, and HTTP/1.0 /r/n
	//See if there's anything in existing headers, if so, add those
		//make sure they all end in /r/n
	//Append host header if none
	//Append user agent header if none
	//Append connection header if none
	//Append proxy connection header if none
	

	//call open_clientfd(hostname, port)



	/*Serve static content */
	if (is_static) {			
		if (stat(filename, &sbuf) < 0) {
			clienterror(fd, filename, "404", "Not found",
				"Tiny couldn't find this file");
		return;
		}
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden",
				"Tiny couldn't read the file");
			return;
		} 
		serve_static(fd, filename, sbuf.st_size);
	} 
	
	/*Serve dynamic content*/
	else {			
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden",
				"Tiny couldn't run the CGI program");
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}
}

/*
 * read_requesthdrs - read HTTP request headers
 */
void read_requesthdrs(rio_t * rp)
{
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	printf("Read request headers buf: %s\n", buf);
	while (strcmp(buf, "\r\n"))
	{
		Rio_readlineb(rp, buf, MAXLINE);
		printf("line read in read request: %s\n", buf);
	}
	printf("buf after reading request headers: %s\n", buf);
	return;
}

/*
 * parse_uri - parse URI into filename and CGI args return 0 if dynamic
 * content, 1 if static
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
	char *ptr;

	/* Static content */
	if (!strstr(uri, "cgi-bin")) {		
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/')
			strcat(filename, "home.html");
		return 1;
	} 
	
	/* Dynamic content */
	else {			
		ptr = index(uri, '?');
		if (ptr){
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0';
		} else {
			strcpy(cgiargs, "");
			strcpy(filename, ".");
			strcat(filename, uri);
		}
		return 0;
	}
}

/*
 * serve_static - copy a file back to the client
 */
void serve_static(int fd, char *filename, int filesize)
{
	int		srcfd;
	char           *srcp, filetype[MAXLINE], buf[MAXBUF];

	/* Send response headers to client */
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	/* Send response body to client */
	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".png"))
		strcpy(filetype, "image/png");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[MAXLINE], *emptylist[] = {NULL};

	/* Return first part of HTTP response */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));

	/* Child */
	if (Fork() == 0) {			
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1);
		Dup2(fd, STDOUT_FILENO);	/* Redirect stdout to client */
		Execve(filename, emptylist, environ);	/* Run CGI program */
	}
	Wait(NULL);		/* Parent waits for and reaps child */
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum,
	    char *shortmsg, char *longmsg)
{
	char		buf       [MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}