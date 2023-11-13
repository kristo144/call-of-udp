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
#define RESPOND(...) snprintf(s->outbuf, BUFSIZE, __VA_ARGS__)

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

Map_t defaultMap = {
	8, {{1, 6, 'R'}, {6, 1, 'L'}}, 
	"XXXXXXXX"
	"XSSSSSSX"
	"XSXXSXXX"
	"XSXSSSSX"
	"XSSSSXSX"
	"XXXSXXSX"
	"XSSSSSSX"
	"XXXXXXXX"
};

enum CommandType {
	CMD_ERROR,
	CMD_REQUEST,
	CMD_ACTION,
};

typedef struct {
	enum CommandType c;
	int p;  // Player
	char a; // Action
	char d; // Direction
} Cmd_t;

typedef struct {
	int sockfd;
	struct sockaddr_in srvAddr, cliAddr;
	socklen_t cliAddrLen;
	char inbuf[BUFSIZE];
	char outbuf[BUFSIZE];
} ServerData_t;

typedef struct {
	int playersConnected;
	Player_t players[MAXPLAYERS];
	int currentTurn;
	int winner;
	Map_t *activeMap;
} GameData_t;

void initGameData(GameData_t *g) {
	g->playersConnected = 0;
	g->currentTurn = -1;
	g->winner = -1;
	g->activeMap = &defaultMap;
}

int isDigit(char c) { return (c >= '0' && c <= '9'); }

int getPlayerByCookie(GameData_t *g, int c) {
	for (int i = 0; i < g->playersConnected; i++)
		if (g->players[i].cookie == c)
			return i;
	return -1;
}

void parseMsgClient(GameData_t *g, ServerData_t *s, Cmd_t *c) {
	char *msg;
	int cookie = strtol(s->inbuf, &msg, 10);
	c->p = getPlayerByCookie(g, cookie);

	c->a = msg[0];
	c->d = msg[1];
}

Cmd_t parseMsg(GameData_t *g, ServerData_t *s) {
	Cmd_t cmd = {0, -1, -1, -1};

	if (s->inbuf[0] == 'R') {
		cmd.c = CMD_REQUEST;
	}
	else if (isDigit(s->inbuf[0])) {
		cmd.c = CMD_ACTION;
		parseMsgClient(g, s, &cmd);
	}
	else {
		cmd.c = CMD_ERROR;
	}

	return cmd;
}

int initServerData(ServerData_t *s, int p) {
	s->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s->sockfd < 0) return -2;

	s->srvAddr.sin_family = AF_INET;
	s->srvAddr.sin_port = htons(p);
	s->srvAddr.sin_addr.s_addr = INADDR_ANY;

	return bind(s->sockfd, (struct sockaddr*) &s->srvAddr, sizeof(s->srvAddr));
}

void recvMsg(ServerData_t *s) {
	recvfrom(s->sockfd, s->inbuf, BUFSIZE, 0, (struct sockaddr*) &s->cliAddr, &s->cliAddrLen);
}

void sendResp(ServerData_t *s) {
	sendto(s->sockfd, s->outbuf, strlen(s->outbuf), 0, (struct sockaddr*) &s->cliAddr, sizeof(s->cliAddr));
}

void updatePlayerAddr(GameData_t *g, ServerData_t *s, int p) {
	g->players[p].addr    = s->cliAddr;
	g->players[p].addrLen = s->cliAddrLen;
}

int addPlayer(GameData_t *g, ServerData_t *s) {
	int newPlayer = g->playersConnected;
	uint16_t cookie;

	if (newPlayer >= MAXPLAYERS)
		return -1;

	cookie = rand();
	g->players[newPlayer].cookie = cookie;
	g->players[newPlayer].pos = g->activeMap->startPos[newPlayer];
	updatePlayerAddr(g, s, newPlayer);
	g->playersConnected++;

	if (g->playersConnected == MAXPLAYERS)
		g->currentTurn = 0;

	return newPlayer;
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

int isSeeable(char c) {
	if (strchr(" ", c)) return 1;
	else return 0;
}

int isMovable(char c) {
	if (strchr(" ", c)) return 1;
	else return 0;
}

int isShootable(char c) {
	if (strchr(" ", c)) return 1;
	else return 0;
}

void showMap(GameData_t *g, ServerData_t *s, int p) {
	char map[MAXMAP];
	Position_t *pos = &(g->players[p].pos);

	int cols = g->activeMap->columns;
	int rows = strlen(g->activeMap->data) / cols;

	strncpy(map, g->activeMap->data, MAXMAP);
	// Mostrar jugadores en mapa
	for (int i = 0; i < g->playersConnected; i++) {
		pos = &(g->players[i].pos);
		map[g->activeMap->columns * pos->y + pos->x] = drawDirection(pos->ori);
	}
	pos = &(g->players[p].pos);

	// Calculo rectangulo vision
	int x0, xf, y0, yf;
	switch (pos->ori) {
		case 'U':
			x0 = pos->x - 1;
			xf = pos->x + 1;
			yf = pos->y + 1;
			for (y0 = pos->y - 1; isSeeable(g->activeMap->data[y0 * cols + pos->x]); y0--);
			break;
		case 'D':
			x0 = pos->x - 1;
			xf = pos->x + 1;
			y0 = pos->y - 1;
			for (yf = pos->y + 1; isSeeable(g->activeMap->data[yf * cols + pos->x]); yf++);
			break;
		case 'L':
			y0 = pos->y - 1;
			yf = pos->y + 1;
			xf = pos->x + 1;
			for (x0 = pos->x - 1; isSeeable(g->activeMap->data[pos->y * cols + x0]); x0--);
			break;
		case 'R':
			y0 = pos->y - 1;
			yf = pos->y + 1;
			x0 = pos->x - 1;
			for (xf = pos->x + 1; isSeeable(g->activeMap->data[pos->y * cols + xf]); xf++);
			break;
	}

	// Esconder
	for (int i = 0; i < cols; i++)
		for (int j = 0; j < rows; j++)
			if (i < x0 || i > xf || j < y0 || j > yf)
				map[j * cols + i] = '.';

	RESPOND("M:%d:%s\n", g->activeMap->columns, map);
}

void turnPlayer(GameData_t *g, ServerData_t *s, Cmd_t c) {
	g->players[c.p].pos.ori = c.d;
	g->currentTurn = (g->currentTurn + 1) % g->playersConnected;
	RESPOND("K\n");
}

void movePlayer(GameData_t *g, ServerData_t *s, Cmd_t c) {
	int posX = g->players[c.p].pos.x;
	int posY = g->players[c.p].pos.y;
	int newX = posX + (c.d == 'R') - (c.d == 'L');
	int newY = posY + (c.d == 'D') - (c.d == 'U');

	int columns = g->activeMap->columns;

	if (isMovable(g->activeMap->data[newY * columns + newX])) {
		g->players[c.p].pos.x = newX;
		g->players[c.p].pos.y = newY;
		turnPlayer(g, s, c);
	}
	else RESPOND("I\n");
}

void shootPlayer(GameData_t *g, ServerData_t *s, Cmd_t c) {
	int columns = g->activeMap->columns;
	Position_t *myPos = &g->players[c.p].pos;
	Position_t *enemyPos = &g->players[!c.p].pos;

	int posIni = columns * myPos->y + myPos->x;
	int posNmy = columns * enemyPos->y + enemyPos->x;
	int shootOffset = columns * ((c.d == 'D') - (c.d == 'U')) + ((c.d == 'R') - (c.d == 'L'));

	int bulletPos = posIni + shootOffset;

	while (isShootable(g->activeMap->data[bulletPos])) {
		if (bulletPos == posNmy) {
			g->winner = c.p;
		}
		bulletPos += shootOffset;
	}

	turnPlayer(g, s, c);
}

void doAction(GameData_t *g, ServerData_t *s, Cmd_t c) {
	int p;

	switch (c.c) {
		default:
		case CMD_ERROR:
			RESPOND("E\n");
			break;
		case CMD_REQUEST:
			p = addPlayer(g, s);
			if (p >= 0) RESPOND("%d\n", g->players[p].cookie);
			else RESPOND("F\n");
			break;
		case CMD_ACTION:
			if (c.p < 0) {
				RESPOND("U\n");
			}
			else {
				updatePlayerAddr(g, s, c.p);
				if (c.a == 'V') showMap(g, s, c.p);
				else {
					if (c.p != g->currentTurn)
						RESPOND("N\n");
					else if (!strchr("UDLR", c.d))
						RESPOND("E\n");
					else switch (c.a) {
						case 'M': movePlayer(g, s, c); break;
						case 'T': turnPlayer(g, s, c); break;
						case 'S': shootPlayer(g, s, c); break;
						default: RESPOND("E\n");
					}
				}
			}
			break;
	}
}

void informTurn(GameData_t *g, ServerData_t *s) {
	static int oldTurn = -1;
	if (g->winner != -1 ) {
		sendto(s->sockfd, "W\n", 3, 0, (struct sockaddr*) &g->players[g->winner].addr, sizeof(struct sockaddr_in));
		sendto(s->sockfd, "L\n", 3, 0, (struct sockaddr*) &g->players[!(g->winner)].addr, sizeof(struct sockaddr_in));
		initGameData(g);
		oldTurn = -1;
	}
	else if (oldTurn != g->currentTurn) {
		sendto(s->sockfd, "T\n", 3, 0, (struct sockaddr*) &g->players[g->currentTurn].addr, sizeof(struct sockaddr_in));
		oldTurn = g->currentTurn;
	}
}

int main (int argc, char **argv) {
	GameData_t g;
	ServerData_t s;
	int port;
	int e;
	Cmd_t cmd;

	if (argc < 2) ABORT("Has d'especificar el port.", 1);

	port = atoi(argv[1]);
	e = initServerData(&s, port);
	if (e == -2) ABORT("Error al crear el socket.", 2);
	else if (e == -1) ABORT("Error al reservar el socket.", 3);

	initGameData(&g);
	srand(time(NULL));

	for(;;) {
		recvMsg(&s);
		cmd = parseMsg(&g, &s);
		doAction(&g, &s, cmd);
		sendResp(&s);
		informTurn(&g, &s);
	}

	return 0;
}
