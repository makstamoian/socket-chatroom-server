#include <ctime>
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <mutex>

#define MAX_LEN 2048
#define NUM_COLORS 6

using namespace std;

struct terminal {
    int id;
    string name;
    int socket;
    thread th;
};

void is_socket_active(int client_socket) {
    if (client_socket < 0) {
        close(client_socket);
    }
}

vector<terminal> clients;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
int seed = 0;
mutex cout_mtx, clients_mtx;
//char sendp[] = "/private";
char change_name_command[] = "/name";

string color(int code);

void change_name(char str[], char name[], int id, int client_socket);

int is_name_placed(string name);

int is_command(char text[], char tocheck[]);

string find_name(char message[]);

void set_name(int id, char name[]);

void shared_print(string str, bool endLine);

int broadcast_message(string message, int sender_id, int status);

int broadcast_message(int num, int sender_id, int status);

void end_connection(int id);

void handle_client(int client_socket, int id);

int main() {
    int port;
    cout << "Enter the port number where your server will be located: ";
    cin >> port;

    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket: ");
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 0);

    if ((bind(server_socket, (struct sockaddr *) &server, sizeof(struct sockaddr_in))) == -1) {
        perror("bind error: ");
        exit(-1);
    }

    if ((listen(server_socket, 8)) == -1) {
        perror("listen error: ");
        exit(-1);
    }

    struct sockaddr_in client;
    int client_socket;
    unsigned int len = sizeof(sockaddr_in);

    cout << colors[NUM_COLORS - 1] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

    while (true) {
        if ((client_socket = accept(server_socket, (struct sockaddr *) &client, &len)) < 0) {
            perror("accept error: ");
            exit(-1);
        }
        seed++;
        thread t(handle_client, client_socket, seed);
        lock_guard<mutex> guard(clients_mtx);
        clients.push_back({seed, string("<unnamed>"), client_socket, (move(t))});
    }

    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    close(server_socket);
    return 0;
}

string color(int code) {
    return colors[code % NUM_COLORS];
}

// Set name of client
void set_name(int id, char name[]) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id == id) {
            clients[i].name = string(name);
        }
    }
}

// For synchronisation of cout statements
void shared_print(string str, bool endLine = true) {
    char timebuf[80];
    time_t seconds = time(NULL);
    tm *timeinfo = localtime(&seconds);
    char *format = "%A, %B %d, %Y %I:%M:%S";
    strftime(timebuf, 80, format, timeinfo);
    lock_guard<mutex> guard(cout_mtx);
    cout << "[" << timebuf << "] " << str;
    if (endLine)
        cout << endl;
}

// Broadcast message to all clients except the sender
int broadcast_message(string message, int sender_id, int is_private) {
    if (is_private == 1) {
        char messg[MAX_LEN];
        strcpy(messg, message.c_str());
        string name = find_name(messg);
        for (int i = 0; i < clients.size(); i++) {
            if (clients[i].name == name && clients[i].id != sender_id) {
                send(clients[i].socket, messg, strlen(messg), 0);
                break;
            }
        }
    } else {
        char temp[MAX_LEN];
        strcpy(temp, message.c_str());

        for (int i = 0; i < clients.size(); i++) {
            if (clients[i].id != sender_id) {
                send(clients[i].socket, temp, sizeof(temp), 0);
            }
        }
    }
}

int broadcast_message(int num, int sender_id, int is_private) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id != sender_id) {
            send(clients[i].socket, &num, sizeof(num), 0);
        }
    }
}

void end_connection(int id) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id == id) {
            lock_guard<mutex> guard(clients_mtx);
            clients[i].th.detach();
            clients.erase(clients.begin() + i);
            close(clients[i].socket);
            break;
        }
    }
}

void handle_client(int client_socket, int id) {

    is_socket_active(client_socket);
    char name[MAX_LEN], str[MAX_LEN];

    string welcome_message = string("<unnamed>") + string(" has joined");
    broadcast_message("#NULL", id, 0);
    broadcast_message(id, id, 0);
    broadcast_message(welcome_message, id, 0);
    shared_print(color(id) + welcome_message + def_col);

    set_name(id, "<unnamed>");
    strncpy(name, "<unnamed>", MAX_LEN);

    while (true) {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0)
            return;
        //if exit
        if (strcmp(str, "/exit") == 0) {
            // Display leaving message
            string message = string(name) + string(" has left");
            broadcast_message("#NULL", id, 0);
            broadcast_message(id, id, 0);
            broadcast_message(message, id, 0);
            shared_print(color(id) + message + def_col);
            end_connection(id);
            return;
        }
        //private or not private message
   //     if (is_command(str, sendp) == 1) {
   //         broadcast_message(string(name), id, 1);
   //         broadcast_message(id, id, 1);
   //         broadcast_message(string(str), id, 1);
   //     } else {
            broadcast_message(string(name), id, 0);
            broadcast_message(id, id, 0);
            broadcast_message(string(str), id, 0);
            shared_print(color(id) + name + ": " + def_col + str);
   //     }
        //change name
        if (is_command(str, change_name_command) == 1) {
            change_name(str, name, id, client_socket);
        }
    }
}

int is_command(char text[], char to_check[]) {
    for (int i = 0; i < strlen(to_check); i++) {
        if (text[i] == to_check[i]) {
            continue;
        } else {
            return 0;
        }
    }
    return 1;
}

string find_name(char message[]) {
    string name;
    for (int i = 0; i < strlen(message); i++) {
        if (message[i] == '@') {
            for (int j = i + 1; j < strlen(message); j++) {
                if (message[j] == ' ') {
                    break;
                } else {
                    name += message[j];
                }
            }
        }
    }
    return name;
}

int is_name_placed(string name) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].name == name) {
            return 1;
        }
    }
    return 0;
}

void change_name(char str[], char name[], int id, int client_socket) {
    string new_name = find_name(str);
    if (is_name_placed(new_name) != 1) {
        for(int i = 0; i < strlen(name); i++){
            name[i] = 0;
        }
        strcpy(name, new_name.c_str());
        set_name(id, name);
        shared_print("name changed");
    }
}

