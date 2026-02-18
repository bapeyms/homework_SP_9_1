#include <winsock2.h>
#include <iostream>
#include <vector>
#include <string>
using namespace std;

#define MAX_CLIENTS 10
#define DEFAULT_BUFLEN 4096

#pragma comment(lib, "ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // отключаем предупреждение _WINSOCK_DEPRECATED_NO_WARNINGS

SOCKET server_socket;

vector<string> history;

int main() {
	system("title Server");

	puts("Start server... DONE.");
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		printf("Failed. Error Code: %d", WSAGetLastError());
		return 1;
	}

	// create a socket
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		printf("Could not create socket: %d", WSAGetLastError());
		return 2;
	}
	// puts("Create socket... DONE.");

	// prepare the sockaddr_in structure
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8888);

	// bind socket
	if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		printf("Bind failed with error code: %d", WSAGetLastError());
		return 3;
	}

	// puts("Bind socket... DONE.");

	// слушать вход€щие соединени€
	listen(server_socket, MAX_CLIENTS);

	// прин€ть и вход€щее соединение
	puts("Server is waiting for incoming connections...\nPlease, start one or more client-side app.");

	// размер нашего приемного буфера, это длина строки
	// набор дескрипторов сокетов
	// fd means "file descriptors"
	fd_set readfds; // https://docs.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-fd_set
	SOCKET client_socket[MAX_CLIENTS] = {};

	while (true) {
		// очистить сокет fdset
		FD_ZERO(&readfds);

		// добавить главный сокет в fdset
		FD_SET(server_socket, &readfds);

		// добавить дочерние сокеты в fdset
		for (int i = 0; i < MAX_CLIENTS; i++) 
		{
			SOCKET s = client_socket[i];
			if (s > 0) {
				FD_SET(s, &readfds);
			}
		}

		// дождитесь активности на любом из сокетов, тайм-аут равен NULL, поэтому ждите бесконечно
		if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
			printf("select function call failed with error code : %d", WSAGetLastError());
			return 4;
		}

		// если что-то произошло на мастер-сокете, то это вход€щее соединение
		SOCKET new_socket; // новый клиентский сокет
		sockaddr_in address;
		int addrlen = sizeof(sockaddr_in);
		if (FD_ISSET(server_socket, &readfds)) {
			if ((new_socket = accept(server_socket, (sockaddr*)&address, &addrlen)) < 0) {
				perror("accept function error");
				return 5;
			}

			for (int i = 0; i < history.size(); i++)
			{
				cout << history[i] << "\n";
				send(new_socket, history[i].c_str(), history[i].size(), 0);
			}

			// информировать серверную сторону о номере сокета - используетс€ в командах отправки и получени€
			printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			// добавить новый сокет в массив сокетов
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (client_socket[i] == 0) {
					client_socket[i] = new_socket;
					printf("Adding to list of sockets at index %d\n", i);
					break;
				}
			}
		}

		// если какой-то из клиентских сокетов отправл€ет что-то
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			SOCKET s = client_socket[i];
			// если клиент присутствует в сокетах чтени€
			if (FD_ISSET(s, &readfds))
			{
				// получить реквизиты клиента
				getpeername(s, (sockaddr*)&address, (int*)&addrlen);

				// проверьте, было ли оно на закрытие, а также прочитайте вход€щее сообщение
				// recv не помещает нулевой терминатор в конец строки (в то врем€ как printf %s предполагает, что он есть)

				char client_message[DEFAULT_BUFLEN];

				int client_message_length = recv(s, client_message, DEFAULT_BUFLEN, 0);
				client_message[client_message_length] = '\0';

				string check_exit = client_message;
				if (check_exit == "off")
				{
					cout << "Client #" << i << " is off\n";
					closesocket(s);
					client_socket[i] = 0;
					continue;
				}
				history.push_back(client_message);
				

				for (int j = 0; j < MAX_CLIENTS; j++)
				{
					if (client_socket[j] != 0 && client_socket[j] != s)
					{
						send(client_socket[j], client_message, client_message_length, 0);
					}
				}

			}
		}
	}

	//WSACleanup();
}

/* «адание 1 
—ервер выступает в роли посредника между подключенными клиентами.
—охран€ет историю сообщении... массив клинтов и т.д.
ѕри отправке текста, тот, кто отправил (клиент), не должен повторно получить свой же текст! 

*/