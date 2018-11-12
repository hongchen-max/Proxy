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


void doit      (int fd);
void read_requesthdrs(rio_t * rp, char* existingHdrs, int* hasHost,
						int* hasUA, int* hasConn, int* hasPro);
int	parseURI  (char *uri, char *host, int* port, char *path);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum,
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
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	
	char host[MAXLINE], path[MAXLINE];
	int port = -1;
	char existingHdrs[MAXLINE];
	char finalRequest[MAXLINE];
	int hasHost = 0;
	int hasUA = 0;
	int hasConn = 0;
	int hasPro = 0;
	
	rio_t	rio;

	/* Read request line and headers */
	Rio_readinitb(&rio, fd);
	if (!Rio_readlineb(&rio, buf, MAXLINE))
		return;
	
	if(sscanf(buf, "%s %s %s", method, uri, version) == 0) {
		clienterror(fd, "Bad request", "400", "Wrong args",
			"Three strings were not provided");
		return;
	}

	if (strcasecmp(method, "GET")) {
		clienterror(fd, method, "501", "Not Implemented",
				    "Proxy does not implement this method");
		return;
	}
	
	if(!strstr(version, "HTTP")){
		clienterror(fd, version, "501", "bad version",
					"Proxy does not implement that version");
		return;
	}

	//Parse URI from GET request, get hostname, port, and path
	if(parseURI(uri, host, &port, path)) {
		printf("\nsuccessfully parsed url\n");
		printf("host: %s\n", host);
		printf("port: %d\n", port);
		printf("path: %s\n", path);

		const char* ptr = strchr(host, '\0');
		if(ptr){
			printf("found o in host\n");
			int index = ptr - host;
			printf("index of o: %d\n", index);
		}

		const char* ptr2 = strchr(path, '\0');
		if(ptr2){
			printf("found o in path\n");
			int index = ptr2 - path;
			printf("index of o: %d\n", index);
		} else{
			printf("need to add 0 in path\n");
		}	

		//now read existing headers
		//Tell me if there was already a host, ua, conn, proxy header
		read_requesthdrs(&rio, existingHdrs, &hasHost, &hasUA, &hasConn,&hasPro);
		printf("done reading headers\n");
		
		//start building request
		//Write GET, path, and HTTP/1.0 /r/n
		char rLine[MAXLINE];
		strcat(rLine, "GET ");
		strcat(rLine, path);
		strcat(rLine, " HTTP/1.0\r\n");

		printf("break\n");
		//See if existingHdrs contains "\r\n", if so, strcat it to final
		//Append host header if none
		//Append user agent header if none
		//Append connection header if none
		//Append proxy connection header if none
		

		//call open_clientfd(hostname, port)

	} else {
		printf("bad url\n");
		clienterror(fd, "bad url", "400", "bad url", "bad url");
		return;
	}

	

}

//returns 1 if parsed OK, 0 if failed
int parseURI(char* uri, char* host, int* port, char* path){
	//try till slash with http
	if(sscanf(uri, "http://%[^/]", host) == 0){
		//try till slash no http
		if (sscanf(uri, "%[^/]", host) == 0){
			return 0;	
		}
	}	
	
	//host is everything until slash
	//has port
	if(strstr(host, ":")){
		int colonIndex = 0;
		for (int i = 0; i < strlen(host); i++){
			if (host[i] == ':'){
				colonIndex = i;
			}
		}
		
		colonIndex++;
		char portS[6];
		int digitCount = 0;
		while(host[colonIndex] != '/'){
			if(digitCount > 5){
				break;
			}
			if(colonIndex == strlen(host)){
				break;
			}
			portS[digitCount] = host[colonIndex];
			digitCount++;
			colonIndex++;
		}
		
		if(sscanf(uri, "http://%[^:]", host) == 0){
			sscanf(uri, "%[^:]", host);
		}

		*port = atoi(portS);
		if (*port == 0){
			return 0;
		}

		//path time
		int start = strlen(host) + strlen(portS) + 1;
		if(strstr(uri, "http://")){
			start = start + 7;
		}
		int j = 0;
		for (int i = start; i < strlen(uri); i++){
			path[j] = uri[i];
			j++;
		}

	} 
	//No port
	else {
		int hostLen = strlen(host);
		if(strstr(uri, "http://")){
			hostLen = hostLen + 7;
		}
		int j = 0;
		for (int i = hostLen; i < strlen(uri); i++){
			path[j] = uri[i];
			j++;
		}
	}
	
	//make host lowercase
	int l = 0;
	while( host[l] ) {
      host[l] = tolower(host[l]);
      l++;
   	}

	return 1;
}


/*
 * read_requesthdrs - read HTTP request headers
 */
void read_requesthdrs(rio_t * rp, char* existingHdrs, int* hasHost,
						int* hasUA, int* hasConn, int* hasPro)
{
	printf("\nReading headers...\n");
	char buf[MAXLINE];
	
	while (strcmp(buf, "\r\n"))
	{
		printf("in while\n");
		Rio_readlineb(rp, buf, MAXLINE);
		printf("Header: %s\n", buf);
		
		if(strstr(buf, "Host")) {
			*hasHost = 1;
			printf("had host\n");
		} else if (strstr(buf, "User-Agent")){
			*hasUA = 1;
			printf("had useragent\n");
		} else if(strstr(buf, "Connection")){
			*hasConn = 1;
			printf("had connection\n");
		} else if (strstr(buf, "Proxy-Connection")){
			*hasPro = 1;
			printf("had proxy connection\n");
		}
		
		if(strstr(buf, "\r\n") == NULL){	
			strcat(buf, "\r\n");
		}
		
		if(strcmp(buf, "\r\n")){
			strcat(existingHdrs, buf);
		}
	}
	return;
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
