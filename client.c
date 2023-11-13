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

typedef struct {
	int sockfd;
	struct sockaddr_in srvAddr;
	socklen_t srvAddrLen;
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
	int cookie;
	char endgame; // Used as boolea: TRUE -> End of game || FALSE -> Still playing
} ClientData_t;

void recvMsg(ClientData_t *c) {
	recvfrom(c->sockfd, c->inbuf, BUFSIZE, 0, (struct sockaddr*) &c->srvAddr, &c->srvAddrLen);
}

void sendMsg(ClientData_t *c) {
	sendto(c->sockfd, c->outbuf, strlen(c->outbuf), 0, (struct sockaddr*) &c->srvAddr, sizeof(c->srvAddr));
}

int initConectionData(ClientData_t *c, int p) {
	c->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (c->sockfd < 0) return -2;

	c->srvAddr.sin_family = AF_INET;
	c->srvAddr.sin_port = htons(p);
	c->srvAddr.sin_addr.s_addr = INADDR_ANY;
	sprintf(c->outbuf, "%c\n\0", 'R');

	return 0;
}

int showMap(ClientData_t *c) {
	int dim;
	char response, map[MAXMAP];

	sprintf(c->outbuf, "%d%c\n", c->cookie, 'V');

	sendMsg(c);
	recvMsg(c);
	sscanf(c->inbuf, "%c:%d:%s\n\0", &response, &dim, map);
	
	if(response == 'W') return -6;

	for(short i = 0; i < dim; i++) {
		for(short j = 0; j < dim; j++) {
			printf("%c", (map[i*dim+j] == 'S') ? ' ' : map[i*dim+j]);
		}
		printf("\n");
	}

	return 0;
}

int startGame(ClientData_t *c) {
	char response;
	int aux;

	sendMsg(c);
	recvMsg(c);
	sscanf(c->inbuf, "%c\n\0", &response);

	if (response == 'F') return -3;
	else if (response == 'E') return -4;

	sscanf(c->inbuf, "%d\n\0", &aux);
	c->cookie = aux;

	c->endgame = 0;

	recvMsg(c);
	sscanf(c->inbuf, "%c\n\0", &response);
	printf("Your turn!\n");

	showMap(c);

	return 0;
}

int action(ClientData_t *c, char act, char dir) {
	char response;

	sprintf(c->outbuf, "%d%c%c\n\0", c->cookie, act, dir);
	sendMsg(c);
	recvMsg(c);

	sscanf(c->inbuf, "%c\n\0", &response);

	if (response == 'I') return -5;
	else if (response == 'E') return -4;
	else if (response == 'U') return -6;
	else return 0;
}

int main (int argc, char **argv) {
	char resp[2];
	int port, e;
	ClientData_t c;

	if (argc < 2) ABORT("Has d'especificar el port.", e);
	port = atoi(argv[1]);

	e = initConectionData(&c, port);
	if (e == -2) ABORT("Error al crear el socket.", e);

	e = startGame(&c);
	if (e == -3) ABORT("Impossible connectar, joc ple (proba a un altre lobby)", e);
	else if (e == -4) ABORT("Hi ha hagut un error inesperat en iniciar la connexio.", e);

	while (e != -9) {
		printf("\nEnter play:\n([Move/Turn/Shoot] [Up/Down/Left/Right]])\n");

		do {
			resp[0] = getchar();
			resp[1] = getchar();
		} while (!((resp[0] == 'M' || resp[0] == 'T' || resp[0] == 'S') && (resp[1] == 'U' || resp[1] == 'D' || resp[1] == 'L' || resp[1] == 'R')));
		getchar();

		e = action(&c, resp[0], resp[1]);
		switch (e) {
			case -4: printf("Hi ha hagut un error inesperat (Error code: %d)", e); break;
			case -5: printf("El moviment es invalid! (Error code: %d)", e); break;
			case -6: printf("Error en enviar la cookie (Error code: %d)", e); break;
			case 0:
				if(showMap(&c) == 0) {
					recvMsg(&c);
					sscanf(c.inbuf, "%c\n\0", &resp[0]);
				} else resp[0] = 'W';
				switch (resp[0]) {
					case 'T': printf("\n\nYour turn!\n"); break;
					case 'W':
						printf("\n\n############ HAS GUANYAT! ############");
						e = -9;
						break;
					case 'L':
						printf("\n\n############ HAS PERDUT :( ############");
						e = -9;
						break;
					default:
						printf("Error, recived %c while expecting 'T/W/L'\n", resp[0]);
						e = -9;
						break;
				}
				break;
			default: break;
		}
	}

	return 0;
}