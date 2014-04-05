
#include "strmap.h"
#include "set.h"

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "mime.c"

int server;
set_t client_set;
pthread_mutex_t client_set_lock;
char webdir[512];

void help() {
	fputs(
		"Mark's Simple Webserver\n"
		"Usage: webserver [port] [webdir]\n",
		stdout
	);
	exit(0);
}

typedef enum {
	HTTP_GET = 0,
	HTTP_HEAD,
	HTTP_POST
} http_cmd_t;

void url_decode(char *buf, const char *url) {
	char chr;
	while (*url) {
		if (*url == '%') {
			url += sscanf(++url, "%hhx", &chr);
			*buf++ = chr;
		} else *buf++ = *url++;
	}
	*buf = 0;
}

void timestamp(FILE *f) {
	char timestring[64];
	time_t rawtime;
	struct tm t;
	time(&rawtime);
	localtime_r(&rawtime, &t);
	strftime(timestring, 64, "%c", &t);
	fprintf(f, "Date: %s\r\n\r\n", timestring);
}

//We could've done a binary search here but I got lazy
const char *find_mime_type(const char *file) {
	struct mime_type *m;
	const char *ext;
	ext = strrchr(file, '.');
	if (ext == NULL) return NULL;
	else ++ext;
	for (m = mimelist; m->ext != NULL; ++m) {
		if (!strcmp(ext, m->ext)) break;	
	}
	return m->mime;
}

void http_get_request(FILE *client, strmap_t *fields) {
	unsigned char buf[4096];
	const char *mime, *query_string = strmap_find(fields, "QUERY_STRING");
	char *s;
	struct stat st;
	FILE *f;
	size_t nread;

	strcpy(buf, webdir);
	strcat(buf, query_string);
	s = strchr(buf, '?');
	if (s) *s = 0;
	if (stat(buf, &st)) {
		fputs(
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Length: 0\r\n",
			client
		);
		timestamp(client);
	}
	mime = find_mime_type(buf);
	if (mime == NULL) mime = "application/octet-stream";
	fprintf(
		client,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %ld\r\n"
		"Connection: Close\r\n",
		mime, st.st_size
	);
	timestamp(client);
	f = fopen(buf, "rb");
	while (nread = fread(buf, sizeof(unsigned char), 4096, f))
		fwrite(buf, sizeof(unsigned char), nread, client);
	fclose(f);
}

void *handle_client(void *arg) {
	char buf[4096];
	FILE *f;
	strmap_t fields;

	int i;	
	http_cmd_t cmd;
	char *token, *saveptr;
	struct sockaddr addr;
	socklen_t addrlen;
	time_t rawtime;

	f = fdopen(*(int*)arg, "r+");
	if (f == NULL) pthread_exit(NULL);

	strmap_init(&fields);
	addrlen = sizeof(addr);	

	getpeername(fileno(f), &addr, &addrlen);
	if (inet_ntop(AF_INET, &addr, buf, addrlen) != NULL) {
		fprintf(stdout, "%s: ", buf);
	}

	if (fgets(buf, 4096, f) == NULL)
		{ fputc('\n', stdout); pthread_exit(NULL); }
	else { 
		token = strpbrk(buf, "\r\n");
		if (token) *token = 0;
		fprintf(stdout, "%s\n", buf);
	}

	token = strtok_r(buf, ": ", &saveptr);
	if (!strcmp(token, "GET"))  cmd = HTTP_GET;
	else if (!strcmp(token, "HEAD")) cmd = HTTP_HEAD;
	else if (!strcmp(token, "POST")) cmd = HTTP_POST;

	switch (cmd) {
	case HTTP_GET:
	case HTTP_HEAD:
	case HTTP_POST:
		token = strtok_r(NULL, " ", &saveptr);
		url_decode(buf, token);
		strmap_insert(&fields, "QUERY_STRING", buf);
	}

	while (fgets(buf, 4096, f)) {
		if (strcmp(buf, "\r\n") == 0) break;
		token = strchr(buf, ':');
		if (token) *token++ = 0;
		if (*token == ' ') ++token;
		saveptr = strpbrk(token, "\r\n");
		if (saveptr) *saveptr = 0;	
		strmap_insert(&fields, buf, token);
	}

	switch (cmd) {
	case HTTP_GET:
		http_get_request(f, &fields);
		break;
	default:
		fputs(
			"HTTP/1.1 501 Not Implemented\r\n"
			"Content-Length: 0\r\n",
			f
		);
		timestamp(f);
	}

	i = fileno(f);
	fclose(f);
	pthread_mutex_lock(&client_set_lock);
	set_remove(&client_set, &i);
	pthread_mutex_unlock(&client_set_lock);
	strmap_destroy(&fields);
	pthread_exit(NULL);	
}

int shutdown_client(const void *arg) {
	int client = *(int*)arg;
	shutdown(client, SHUT_RD | SHUT_WR);
	close(client);
	return 0;
}

//FIXME: segfault on shutdown
void shutdown_server(int sig) {
	pthread_mutex_lock(&client_set_lock);
	set_iterate(&client_set, shutdown_client);
	set_destroy(&client_set);
	exit(0);	
}

void bind_and_listen(const char *service) {
	int i;
	struct addrinfo hints, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	if (i = getaddrinfo(NULL, service, &hints, &ai)) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(i));
		exit(1);
	}
	server = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (server == -1) { perror("socket"); exit(1); }
	if (bind(server, ai->ai_addr, ai->ai_addrlen))
		{ perror("bind"); exit(1); }
	if (listen(server, 4))
		{ perror("listen"); exit(1); }
}

int main(int argc, const char **argv) {
	int client;
	pthread_t thread;
	pthread_attr_t attr;

	if (argc != 3) help();

	signal(SIGINT, shutdown_server);
	set_init(&client_set, &int_cmp);	
	pthread_mutex_init(&client_set_lock, NULL);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	strcpy(webdir, argv[2]);
	bind_and_listen(argv[1]);

	for (;;) {
		client = accept(server, NULL, NULL);
		if (client == -1) continue;
		pthread_mutex_lock(&client_set_lock);
		pthread_create(
			&thread, &attr, handle_client,
			(void*)set_insert(&client_set, &client, sizeof(int))
		);
		pthread_mutex_unlock(&client_set_lock);
	}
}


