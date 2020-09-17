//-----------------------Defines--------------------------
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

//-----------------------Includes-------------------------
#include <iostream>
#include <winsock2.h>
#include <string.h>
#include <time.h>
#pragma comment(lib, "Ws2_32.lib")

//-----------------------Enums And Structs----------------
enum State {
	EMPTY_STATE,
	IDLE,
	SEND
};
enum SocketType {
	EMPTY_SOCKET,
	LISTEN_SOCKET,
	MESSAGE_SOCKET
};
enum RequestType {
	EMPTY_REQUEST,
	SEND_TIME,
	SEND_SECONDS
};
struct SocketState
{
	SOCKET id;					// Socket handle
	SocketType socketType;
	State state;
	RequestType sendSubType;	// Sending sub-type
	char buffer[128];
	int len;
};

//-----------------------Globals--------------------------
const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;





//----------------Function Declarations-------------------
void handleWaitRecvSockets(fd_set* waitRecv, int* nfd);
void handleWaitSendSockets(fd_set* waitSend, int* nfd);
fd_set getWaitRecvSockets();
fd_set getWaitSendSockets();
int findEmptySocketIndex();
bool addListenSocket();
bool hasNewConnection(fd_set waitRecv);
bool addSocket(SOCKET id, SocketType socketType);
void removeSocket(int index);
void acceptConnection();
void receiveMessage(int index);
void sendMessage(int index);
int filterIdleSockets(fd_set* waitRecv, fd_set* waitSend);

using namespace std;

int main()
{
	if (addListenSocket() == false) {
		return -1;
	}

	while (true)
	{
		fd_set waitRecv = getWaitRecvSockets();
		fd_set waitSend = getWaitSendSockets();

		int nfd = filterIdleSockets(&waitRecv, &waitSend);
		if (nfd == -1)
		{
			return -1;					// Error at select function
		}

		if (hasNewConnection(waitRecv)) {
			acceptConnection();
			cout << "except connection\n";
			nfd--;
		}

		handleWaitRecvSockets(&waitRecv, &nfd);
		handleWaitSendSockets(&waitSend, &nfd);

	}
}

void handleWaitRecvSockets(fd_set* waitRecv, int* nfd)
{
	for (int i = 1; i < MAX_SOCKETS && *nfd > 0; i++)
	{
		if (FD_ISSET(sockets[i].id, waitRecv))
		{
			(*nfd)--;
			switch (sockets[i].socketType)
			{

			case MESSAGE_SOCKET:
				receiveMessage(i);
				break;
			}
		}
	}
}

void handleWaitSendSockets(fd_set* waitSend, int* nfd)
{
	for (int i = 0; i < MAX_SOCKETS && *nfd > 0; i++)
	{
		if (FD_ISSET(sockets[i].id, waitSend))
		{
			(*nfd)--;
			switch (sockets[i].state)
			{
			case SEND:
				sendMessage(i);
				break;
			}
		}
	}
}

fd_set getWaitRecvSockets()
{
	fd_set waitRecv;
	FD_ZERO(&waitRecv);
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if ((sockets[i].socketType == LISTEN_SOCKET) || (sockets[i].socketType == MESSAGE_SOCKET))
			FD_SET(sockets[i].id, &waitRecv);
	}

	return waitRecv;
}

fd_set getWaitSendSockets()
{
	fd_set waitSend;
	FD_ZERO(&waitSend);
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].state == SEND)
			FD_SET(sockets[i].id, &waitSend);
	}

	return waitSend;
}

int findEmptySocketIndex()
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].socketType == EMPTY_SOCKET)
			return i;
	}

	return -1;
}

bool addListenSocket()
{

	WSAData wsaData;

	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return false;
	}


	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();

		return false;
	}

	sockaddr_in serverService;
	serverService.sin_family = AF_INET;
	serverService.sin_addr.s_addr = INADDR_ANY;
	serverService.sin_port = htons(TIME_PORT);

	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();

		return false;
	}

	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();

		return false;
	}

	return addSocket(listenSocket, LISTEN_SOCKET);
}

bool hasNewConnection(fd_set waitRecv)
{
	return FD_ISSET(sockets[0].id, &waitRecv);
}

bool addSocket(SOCKET id, SocketType socketType)
{
	bool succeed = false;
	int socketIndex = findEmptySocketIndex();
	if (socketIndex != -1) {

		sockets[socketIndex].id = id;
		sockets[socketIndex].socketType = socketType;
		sockets[socketIndex].state = IDLE;
		sockets[socketIndex].len = 0;

		socketsCount++;
		succeed = true;
	}

	return  succeed;
}

void removeSocket(int index)
{
	sockets[index].socketType = EMPTY_SOCKET;
	sockets[index].state = EMPTY_STATE;
	socketsCount--;
}

void acceptConnection()
{
	SOCKET id = sockets[0].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	///
	/// Set the socket to be in non-blocking mode.
	///
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, MESSAGE_SOCKET) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}

}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Time Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			if (strncmp(sockets[index].buffer, "TimeString", 10) == 0)
			{
				sockets[index].state = SEND;
				sockets[index].sendSubType = SEND_TIME;
				memcpy(sockets[index].buffer, &sockets[index].buffer[10], sockets[index].len - 10);
				sockets[index].len -= 10;
				return;
			}
			else if (strncmp(sockets[index].buffer, "SecondsSince1970", 16) == 0)
			{
				sockets[index].state = SEND;
				sockets[index].sendSubType = SEND_SECONDS;
				memcpy(sockets[index].buffer, &sockets[index].buffer[16], sockets[index].len - 16);
				sockets[index].len -= 16;
				return;
			}
			else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
			{
				closesocket(msgSocket);
				removeSocket(index);
				return;
			}
		}
	}

}

void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[255] = "";

	SOCKET msgSocket = sockets[index].id;
	if (sockets[index].sendSubType == SEND_TIME)
	{
		// Answer client's request by the current time string.

		// Get the current time.
		time_t timer;
		time(&timer);
		// Parse the current time to printable string.
		strcpy(sendBuff, ctime(&timer));
		sendBuff[strlen(sendBuff) - 1] = 0; //to remove the new-line from the created string
	}
	else if (sockets[index].sendSubType == SEND_SECONDS)
	{
		// Answer client's request by the current time in seconds.

		// Get the current time.
		time_t timer;
		time(&timer);
		// Convert the number to string.
		_itoa((int)timer, sendBuff, 10);
	}

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Time Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].state = IDLE;

}

int filterIdleSockets(fd_set* waitRecv, fd_set* waitSend)
{
	int nfd = select(0, waitRecv, waitSend, NULL, NULL);
	if (nfd == SOCKET_ERROR)
	{
		cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
		WSACleanup();
		return -1;
	}
}
