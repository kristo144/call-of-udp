#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define MAXMAP 512
#define BUFSIZE 1024

#define ABORT(msg, retval) do {fprintf(stderr, "%s\n", msg); return retval;} while (0)

enum CommandType {
	CMD_ERROR,
	CMD_REQUEST,
	CMD_ACTION,
};

typedef struct {
	int sockfd;
	struct sockaddr_in srvAddr;
	socklen_t srvAddrLen;
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
	char endgame; // Used as boolea: TRUE -> End of game || FALSE -> Still playing
} ClientData_t;

void recvMsg(ClientData_t *c) {
	recvfrom(c->sockfd, c->inbuf, BUFSIZE, 0, (struct sockaddr*) &c->srvAddr, &c->srvAddrLen);
}

void sendMsg(ClientData_t *c) {
	sendto(c->sockfd, c->outbuf, strlen(c->outbuf), 0, (struct sockaddr*) &c->srvAddr, sizeof(c->srvAddr));
}

int initClientData(ClientData_t *c, int p) {
	c->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (c->sockfd < 0) return -1;

	c->srvAddr.sin_family = AF_INET;
	c->srvAddr.sin_port = htons(p);
	c->srvAddr.sin_addr.s_addr = INADDR_ANY;
	sprintf(c->outbuf, "%c\n\0", 'R');
	c->endgame = 0;

	return 0;
}

void showMap(ClientData_t *c) {
	char response, dim, map[MAXMAP];
	sscanf(c->inbuf, "%c:%d:%s\n\0", &response, &dim, &map);
	printf("%s\n\n\n", map);

	//for(short i = 0; i < dim; i++) {
	//	for(short j = 0; j < dim; j++) {
	//		printf("%c", map[i*dim+j]);
	//	}
	//	printf("\n");
	//}
}

int startGame(ClientData_t *c, int *cookie) {
	char response;

	sendMsg(c);
	recvMsg(c);
	sscanf(c->inbuf, "%c\n\0", &response);
	if (response == 'F') ABORT("Impossible connectar, joc ple (proba a un altre lobby)", 3);
	else if (response == 'E') ABORT("Hi ha hagut un error inesperat en iniciar la connexio.", 4);
	else sscanf(c->inbuf, "%d\n\0", cookie);

	sprintf(c->outbuf, "%d%c\n", *cookie, 'V');
	sendMsg(c);
	recvMsg(c);
	showMap(c);

	return 0;
}

int main (int argc, char **argv) {
	int port, e, cookie = 0;
	ClientData_t *c;

	if (argc < 2) ABORT("Has d'especificar el port.", 1);

	port = atoi(argv[1]);
	e = initClientData(c, port);
	if (e == -1) ABORT("Error al crear el socket.", 2);

	srand(time(NULL));

	startGame(c, &cookie);

	return 0;
}