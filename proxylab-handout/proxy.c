/*
 * proxy.c - A simple, iterative HTTP/1.0 Web server that uses the GET method
 * to serve static and dynamic content.
 */
#define __MAC_OS_X
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#include "sbuf.h"
#include "lbuf.h"
#include <stdio.h>

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

//Struct of web response object
typedef struct {
	char uri[MAXLINE];
	char response[MAX_OBJECT_SIZE];
	int size;
	int count;
} Response;

int highestCount = 0;

//number of objects allowed in our cache
static const int NUM_OBJS = MAX_CACHE_SIZE/MAX_OBJECT_SIZE;

//Array of web response objects
Response** cache;

//reader writer cache mutex info
int readcnt;
sem_t mutex, w;

//thread queue stuff
static sbuf_t queue;
static const int nInQ = 2;

//Log queue, will have nInQ size
static lbuf_t logQueue;

//file to write to
FILE* fp;

void doit(int fd);
void read_requesthdrs(rio_t * rp, char* existingHdrs, int* hasHost,
						int* hasUA, int* hasConn, int* hasPro);
int	parseURI  (char *uri, char *host, char* port, char *path);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum,
	    	char *shortmsg, char *longmsg);

//Server thread
void* thread(void* vargp) {
	Pthread_detach(pthread_self());
	while(1) {
		//get connfd from queue
		int connfd = sbuf_remove(&queue);
		doit(connfd);
		Close(connfd);
	}
	return NULL;
}

//Logging thread
void* logThread(void* vargp) {
	Pthread_detach(pthread_self());
	while(1) {
		//get the latest message
		char *message = lbuf_remove(&logQueue);
			
		//write it to a file	
		fprintf(fp, "%s\n", message);

		//free the message you got
		Free(message);
	}
}

//sigint handler
void sigint_handler(int sig) {
	//deinitializes queue
	sbuf_deinit(&queue);

	//deinitialize logging queue
	lbuf_deinit(&logQueue);

	//free the cache
	for (int i = 0; i < NUM_OBJS; i++){
		Free(cache[i]);
	}
	Free(cache);
	
	//close the file descriptor
	fclose(fp);

	fprintf(stderr, "exiting...\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int		listenfd;
	char		hostname  [MAXLINE], port[MAXLINE];
	socklen_t	clientlen;
	struct sockaddr_storage clientaddr;

	//install sigint handler
	Signal(SIGINT, sigint_handler);
	
	/* Check command line args */
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	//Initialize the thread queue
	sbuf_init(&queue, nInQ);
	
	//Make the thread pool
	pthread_t tid;
	for (int i = 0; i < nInQ; i++) {
		Pthread_create(&tid, NULL, thread, NULL);
	}

	//Initialize the logging queue
	lbuf_init(&logQueue, nInQ);

	//Make the logging thread
	Pthread_create(&tid, NULL, logThread, NULL);

	//Open the file to write to
	fp = fopen("./log.txt", "w+"); 
	setbuf(fp, NULL);

	//Make the cache array
	cache = Calloc(NUM_OBJS, sizeof(Response*));
	for (int i = 0; i < NUM_OBJS; i++) {
		cache[i] = Calloc(1, sizeof(Response));
		cache[i]->count = 0;
		cache[i]->size = 0;
		memset(cache[i]->uri, 0, MAXLINE);
		memset(cache[i]->response, 0, MAX_OBJECT_SIZE);
	}

	//initialize the cache semaphores
	Sem_init(&mutex, 0, 1);
	Sem_init(&w, 0, 1);
	readcnt = 0;

	//Opens a server on port argv[1]
	listenfd = Open_listenfd(argv[1]);
	printf("Proxy listening on port %s\n", argv[1]);

	while (1)
	{
		clientlen = sizeof(clientaddr);
		//connfd can now be used to communicate with client
		int connfd = Accept(listenfd, (SA *) & clientaddr, &clientlen);

		Getnameinfo((SA *) & clientaddr, clientlen, hostname, MAXLINE,
				    port, MAXLINE, 0);
		
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		
		//Put the connfd on the queue
		sbuf_insert(&queue, connfd);	
	}

}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	
	char host[MAXLINE], path[MAXLINE];
	char port[6];
	char existingHdrs[MAXLINE];
	char finalRequest[MAXLINE];
	int hasHost = 0;
	int hasUA = 0;
	int hasConn = 0;
	int hasPro = 0;
	
	memset(buf, 0, MAXLINE);
	memset(method, 0, MAXLINE);
	memset(uri, 0, MAXLINE);
	memset(version, 0, MAXLINE);
	memset(host, 0, MAXLINE);
	memset(path, 0, MAXLINE);
	memset(port, 0, 6);
	memset(existingHdrs, 0, MAXLINE);
	memset(finalRequest, 0, MAXLINE);
	
	strcat(port, "80");
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
	if(!parseURI(uri, host, port, path)) {
		printf("bad url\n");
		clienterror(fd, "bad url", "400", "bad url", "bad url");
		return;
	}

	//log the thread message
	char message[MAXLINE];
	memset(message, 0, MAXLINE);
	strcat(message, "Thread handled: ");
	strcat(message, host);
	strcat(message, " ");
	strcat(message, port);
	strcat(message, " ");
	strcat(message, path);
	strcat(message, "\n");
	lbuf_insert(&logQueue, message);		

	//cache semaphore handling
	P(&mutex);
	readcnt++;
	if(readcnt == 1){
		P(&w);
	}
	V(&mutex);

	//See if we have a cached version of the URI
	for (int i = 0; i < NUM_OBJS; i++) {
		if(strcmp(cache[i]->uri, uri) == 0){
			//Matching uri found, return it
			Rio_writen(fd, cache[i]->response, cache[i]->size);	
			//increment count of this object to be most recent
			highestCount++;	
			cache[i]->count = highestCount; 
			return;
		}
	}

	//end semaphore handling
	P(&mutex);
	readcnt--;
	if(readcnt == 0) {
		V(&w);
	}
	V(&mutex);

	//now read existing headers
	//Tell me if there was already a host, ua, conn, proxy header
	read_requesthdrs(&rio, existingHdrs, &hasHost, &hasUA, &hasConn,&hasPro);
	printf("done reading headers\n");
	
	//start building request
	//Write GET, path, and HTTP/1.0 /r/n
	char rLine[MAXLINE];
	memset(rLine, 0, MAXLINE);

	strcat(rLine, "GET ");
	strcat(rLine, path);
	strcat(rLine, " HTTP/1.0\r\n");
	strcat(finalRequest, rLine);
	
	//See if existingHdrs contains "\r\n", if so, strcat it to final
	if (strstr(existingHdrs, "\r\n")) {
		strcat(finalRequest, existingHdrs);	
	}
	
	//Append host header if none
	if(!hasHost){
		strcat(finalRequest, "Host: ");
		strcat(finalRequest, host);
		strcat(finalRequest, "\r\n");
	}

	//Append user agent header if none
	if(!hasUA) {
		strcat(finalRequest, user_agent_hdr);
	}

	//Append connection header if none
	if(!hasConn) {
		strcat(finalRequest, "Connection: close\r\n");
	}

	//Append proxy connection header if none
	if(!hasPro) {
		strcat(finalRequest, "Proxy-Connection: close\r\n");
	}
	
	//Append empty line
	strcat(finalRequest, "\r\n");

	//connect to server and write the bytes
	int clientfd = open_clientfd(host, port);
	if(clientfd < 0){
		clienterror(fd, "Bad request", "400", "Bad host or port",
			"You done messed up a-aron");
			return;
	}
	Rio_readinitb(&rio, clientfd);
	Rio_writen(clientfd, finalRequest, strlen(finalRequest));
	
	//receive bytes back
	char tmp[MAXLINE];
	memset(tmp, 0, MAXLINE);
	char *rsp = Calloc(MAX_OBJECT_SIZE, sizeof(char));
	memset(rsp, 0, MAX_OBJECT_SIZE);
	int rspSize = 0;
	
	int i = 0;
	while((i = Rio_readnb(&rio, tmp, MAXLINE)) > 0) {
		//concat line to cache response while tracking size
		rspSize = rspSize + i;
		if(rspSize <= MAX_OBJECT_SIZE){
			strcat(rsp, tmp);
		}
		
		//write it to client
		Rio_writen(fd, tmp, i);
	}				
		
	Close(clientfd);

	//cache semaphore handling
	P(&w);

	//is it small enough to cache?
	if(rspSize <= MAX_OBJECT_SIZE) {
		//Find lru index
		int lowestCountFound = highestCount;
		int lruIndex = 0;
		for(int i = 0; i < NUM_OBJS; i++){
			if(cache[i]->count < lowestCountFound){
				lowestCountFound = cache[i]->count;
				lruIndex = i;
			}
		}
		//cache it
		highestCount++;
		cache[lruIndex]->count = highestCount;
		strcpy(cache[lruIndex]->uri, uri);
		strcpy(cache[lruIndex]->response, rsp);
		cache[lruIndex]->size = rspSize;
	}
	
	//end cache semaphore handling
	V(&w);

	Free(rsp);
	printf("awaiting connnection...\n\n");
} 



//returns 1 if parsed OK, 0 if failed
int parseURI(char* uri, char* host, char* port, char* path){
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
		//char portS[6];
		memset(port, 0, 6*(sizeof port[0]));
		int digitCount = 0;
		while(host[colonIndex] != '/'){
			if(digitCount > 5){
				break;
			}
			if(colonIndex == strlen(host)){
				break;
			}
			port[digitCount] = host[colonIndex];
			digitCount++;
			colonIndex++;
		}
		
		if(sscanf(uri, "http://%[^:]", host) == 0){
			sscanf(uri, "%[^:]", host);
		}

		//path time
		int start = strlen(host) + strlen(port) + 1;
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
	buf[0] = '\0';
	
	while (strcmp(buf, "\r\n"))
	{
		Rio_readlineb(rp, buf, MAXLINE);
		
		if(strstr(buf, "Host")) {
			*hasHost = 1;
		} else if (strstr(buf, "User-Agent")){
			*hasUA = 1;
		} else if(strstr(buf, "Connection")){
			*hasConn = 1;
		} else if (strstr(buf, "Proxy-Connection")){
			*hasPro = 1;
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
	char buf[MAXLINE], body[MAXBUF];

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
