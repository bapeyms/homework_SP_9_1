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
vector<string> nicknames(MAX_CLIENTS);

bool NicknameCheck(const string& n)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (nicknames[i] == n)
		{
			return true;
		}
	}
	return false;
}

int main() 
{
	system("title Server");

	puts("Start server... DONE");
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) 
	{
		printf("Failed. Error Code: %d", WSAGetLastError());
		return 1;
	}

	// create a socket
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) 
	{
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
	if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) 
	{
		printf("Bind failed with error code: %d", WSAGetLastError());
		return 3;
	}

	// puts("Bind socket... DONE.");

	// слушать входящие соединения
	listen(server_socket, MAX_CLIENTS);

	// принять и входящее соединение
	puts("Server is waiting for incoming connections...\nPlease, start one or more client-side app.");

	// размер нашего приемного буфера, это длина строки
	// набор дескрипторов сокетов
	// fd means "file descriptors"
	fd_set readfds; // https://docs.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-fd_set
	SOCKET client_socket[MAX_CLIENTS] = {};
	
	while (true) 
	{
		// очистить сокет fdset
		FD_ZERO(&readfds);

		// добавить главный сокет в fdset
		FD_SET(server_socket, &readfds);

		// добавить дочерние сокеты в fdset
		for (int i = 0; i < MAX_CLIENTS; i++) 
		{
			SOCKET s = client_socket[i];
			if (s > 0) 
			{
				FD_SET(s, &readfds);
			}
		}

		// дождитесь активности на любом из сокетов, тайм-аут равен NULL, поэтому ждите бесконечно
		if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) 
		{
			printf("select function call failed with error code : %d", WSAGetLastError());
			return 4;
		}

		// если что-то произошло на мастер-сокете, то это входящее соединение
		SOCKET new_socket; // новый клиентский сокет
		sockaddr_in address;
		int addrlen = sizeof(sockaddr_in);
		if (FD_ISSET(server_socket, &readfds)) 
		{
			if ((new_socket = accept(server_socket, (sockaddr*)&address, &addrlen)) < 0) 
			{
				perror("accept function error");
				return 5;
			}

			for (int i = 0; i < history.size(); i++)
			{
				cout << history[i] << "\n";
				send(new_socket, history[i].c_str(), history[i].size(), 0);
			}

			// информировать серверную сторону о номере сокета - используется в командах отправки и получения
			printf("New connection, socket fd is %d, ip is: %s, port: %d\n", 
				new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			char name[DEFAULT_BUFLEN];
			string nickname;
			while (true)
			{
				char ask_name[] = "Welcome! Please, enter your nickname: ";	
				send(new_socket, ask_name, strlen(ask_name), 0);
				int name_length = recv(new_socket, name, DEFAULT_BUFLEN, 0);
				if (name_length <= 0)
				{
					cout << "Empty nickname. Client is off" << endl;
					closesocket(new_socket);
					break;
				}
				name[name_length] = '\0';
				nickname = name;
				if (NicknameCheck(nickname))
				{
					char nickname_check_result[] = "Nickname already exists. Try another one\n";
					send(new_socket, nickname_check_result, strlen(nickname_check_result), 0);
				}
				else
				{
					char nickname_check_ok[] = "Welcome to chat!\n\n- Commands -\n/users - show online users\n/nick - change nickname\n/quit - leave chat\n";
					send(new_socket, nickname_check_ok, strlen(nickname_check_ok), 0);
					break;
				}
			}

			// добавить новый сокет в массив сокетов
			for (int i = 0; i < MAX_CLIENTS; i++) 
			{
				if (client_socket[i] == 0) 
				{
					client_socket[i] = new_socket;
					nicknames[i] = nickname;
					printf("Adding to list of sockets at index %d\n", i);
					break;
				}
			}
		}

		// если какой-то из клиентских сокетов отправляет что-то
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			SOCKET s = client_socket[i];
			// если клиент присутствует в сокетах чтения
			if (FD_ISSET(s, &readfds))
			{
				// получить реквизиты клиента
				getpeername(s, (sockaddr*)&address, (int*)&addrlen);

				// проверьте, было ли оно на закрытие, а также прочитайте входящее сообщение
				// recv не помещает нулевой терминатор в конец строки (в то время как printf %s предполагает, что он есть)

				char client_message[DEFAULT_BUFLEN];

				int client_message_length = recv(s, client_message, DEFAULT_BUFLEN, 0);
				client_message[client_message_length] = '\0';

				// quit command
				string check_exit = client_message;
				if (check_exit == "/quit")
				{
					cout << nicknames[i] << " quit\n";
					string left_msg = nicknames[i] + " left the chat";
					for (int j = 0; j < MAX_CLIENTS; j++)
					{
						if (client_socket[j] != 0 && client_socket[j] != s)
						{
							send(client_socket[j], left_msg.c_str(), left_msg.size(), 0);
						}
					}
					closesocket(s);
					client_socket[i] = 0;
					nicknames[i] = "";
					continue;
				}

				// users command
				string check_online = client_message;
				if (check_online == "/users")
				{
					string user_list = "Online users:\n";
					for (int j = 0; j < MAX_CLIENTS; j++)
					{
						if (client_socket[j] != 0)
						{
							user_list += "- " + nicknames[j] + "\n";
						}
					}
					send(s, user_list.c_str(), user_list.size(), 0);
					continue;
				}

				// nick command
				string nick_change(client_message);
				if (nick_change.rfind("/nick", 0) == 0)
				{
					string new_nickname;
					if (nick_change.size() > 6)
					{
						new_nickname = nick_change.substr(6);
					}
					if (new_nickname.empty())
					{
						string error = "ERROR! Correct example: /nick new_nickname\n";
						send(s, error.c_str(), error.size(), 0);
						continue;
					}
					if (NicknameCheck(new_nickname))
					{
						string error = "Nickname already exists. Try another one.\n";
						send(s, error.c_str(), error.size(), 0);
						continue;
					}
					string old_nick = nicknames[i];
					nicknames[i] = new_nickname;
					string notify = old_nick + " changed nickname to " + new_nickname + "\n";

					for (int j = 0; j < MAX_CLIENTS; j++)
					{
						if (client_socket[j] != 0)
						{
							send(client_socket[j], notify.c_str(), notify.size(), 0);
						}
					}

					string confirm = "Your nickname is now " + new_nickname + "\n";
					send(s, confirm.c_str(), confirm.size(), 0);
					cout << notify;
					continue;
				}

				string full_message = "[" + nicknames[i] + "]: " + client_message;
				history.push_back(full_message);
				for (int j = 0; j < MAX_CLIENTS; j++)
				{
					if (client_socket[j] != 0 && client_socket[j] != s)
					{
						send(client_socket[j], full_message.c_str(), full_message.size(), 0);
					}
				}
			}
		}
	}
}