#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define BUFSIZE 1024
#define MAXPLAYERS 2
#define MAXMAP 512

#define ABORT(msg, retval) do {fprintf(stderr, "%s\n", msg); return retval;} while (0)
#define RESPOND(...) snprintf(outbuf, BUFSIZE, __VA_ARGS__)

typedef struct {
	int x;
	int y;
	char ori;
} Position_t;

typedef struct {
	Position_t pos;
	uint16_t cookie;
	struct sockaddr_in addr;
	socklen_t addrLen;
} Player_t;

typedef struct {
	unsigned int columns;
	Position_t startPos[MAXPLAYERS];
	char data[MAXMAP];
} Map_t;

static Map_t defaultMap = {
	8, {{1, 6, 'R'}, {6, 1, 'L'}}, 
	"XXXXXXXX"
	"X      X"
	"X XX XXX"
	"X X    X"
	"X    X X"
	"XXX XX X"
	"X      X"
	"XXXXXXXX"
};

// Global vars

static struct {
	int playersConnected;
	Player_t players[MAXPLAYERS];
	int currentTurn;
	int winner;
	Map_t *activeMap;
} gameData = {
	.currentTurn = -1,
	.winner = -1,
	.activeMap = &defaultMap,
};

char inbuf[BUFSIZE];
char outbuf[BUFSIZE];
struct sockaddr_in clientAddr;
socklen_t addrLen;

int prepareSock(int sock, struct sockaddr_in *server_addr, int port) {
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(port);
	server_addr->sin_addr.s_addr = INADDR_ANY;
	return bind(sock, (struct sockaddr*) server_addr, sizeof(*server_addr));
}

int addPlayer() {
	int newPlayer = gameData.playersConnected;
	uint16_t cookie;

	if (newPlayer >= MAXPLAYERS)
		return -1;

	cookie = rand();
	gameData.players[newPlayer].cookie = cookie;
	gameData.players[newPlayer].pos = gameData.activeMap->startPos[newPlayer];
	gameData.players[newPlayer].addr = clientAddr;
	gameData.playersConnected++;
	printf("Players connected: %d\n", gameData.playersConnected);

	if (gameData.playersConnected == MAXPLAYERS)
		gameData.currentTurn = 0;

	return cookie;
}

int isDigit(char c) {
	return (c >= '0' && c <= '9');
}

int getPlayer(int cookie) {
	for (int i = 0; i <= gameData.playersConnected; i++)
		if (cookie == gameData.players[i].cookie)
			return i;
	return -1;
}

char drawDirection(char d) {
	switch (d) {
		case 'U': return '^';
		case 'D': return 'v';
		case 'L': return '<';
		case 'R': return '>';
		default:  return '-';
	}
}

void sendMap(int player) {
	char map[MAXMAP];
	Position_t *pos = &(gameData.players[player].pos);
	strncpy(map, gameData.activeMap->data, MAXMAP);
	map[gameData.activeMap->columns * pos->y + pos->x] = drawDirection(pos->ori);
	RESPOND("M:%d:%s\n", gameData.activeMap->columns, map);
}

void playerTurn(int player, char direction) {
	gameData.players[player].pos.ori = direction;
}

int playerMove(int player, int direction) {
	player = direction;
	direction = player;
	return 0;
}

void playerShoot(int player, int direction) {
	player = direction;
	direction = player;
}

int playerAction(int player, char action, char direction) {
	int res = 1;

	switch(action){
		case 'T':
			playerTurn(player, direction);
			break;
		case 'M':
			res = playerMove(player, direction);
			break;
		case 'S':
			playerShoot(player, direction);
			break;
	}

	if (res) gameData.currentTurn ^= 1;

	return res;
}

void parseCmd(char *cmd, int player){
	switch(cmd[0]) {
		case 'V':
			sendMap(player);
			break;
		case 'M':
		case 'T':
		case 'S':
			if (gameData.currentTurn != player)
				RESPOND("N\n");
			else if (playerAction(player, cmd[0], cmd[1]))
				RESPOND("K\n");
			else RESPOND("I\n");
			break;
		default:
			RESPOND("E\n");
	}
}

void parseMsg() {
	int cookie;
	int player;
	char *cmd;

	if (inbuf[0] == 'R') {
		cookie = addPlayer();
		if (cookie < 0) RESPOND("F\n");
		else RESPOND("%d\n", cookie);
	}
	else if (isDigit(inbuf[0])) {
		cookie = strtol(inbuf, &cmd, 10);
		player = getPlayer(cookie);
		if (player >= 0) parseCmd(cmd, player);
		else RESPOND("U\n");
	}
	else RESPOND("E\n");
}

void notifyTurn(int sock) {
	RESPOND("T\n");
	sendto(sock, outbuf, strlen(outbuf), 0, (struct sockaddr*) &gameData.players[gameData.currentTurn].addr, addrLen);
}

void handleGameLogic(int sock) {
	static int oldTurn = -1;

	if (oldTurn != gameData.currentTurn) {
		notifyTurn(sock);
	}

	oldTurn = gameData.currentTurn;
}

int main (int argc, char **argv) {
	int sock, port;
	struct sockaddr_in server_addr;

	if (argc < 2) ABORT("Has d'especificar el port.", 1);

	port = atoi(argv[1]);
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) ABORT("Error al crear el socket.", 2);

	if (prepareSock(sock, &server_addr, port) < 0)
		ABORT("Error al reservar el socket.", 3);

	srand(time(NULL));

	for(;;) {
		recvfrom(sock, inbuf, BUFSIZE, 0, (struct sockaddr*) &clientAddr, &addrLen);
		parseMsg();
		sendto(sock, outbuf, strlen(outbuf), 0, (struct sockaddr*) &clientAddr, addrLen);
		handleGameLogic(sock);
	}

	close(sock);
	return 0;
}
