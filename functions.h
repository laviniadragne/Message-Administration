#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

#define MAX_TOPIC 50
#define MAX_TYPE 12
#define MAX_SF 2
#define MAX_IP_UDP 20
#define MAX_PAYLOAD 1500
#define MAX_TOPIC_BUF 100
#define BUFLEN_CL 1574


using namespace std;

// Structura folosita pentru a retine datele despre
// un client
struct client {
    string id;
    map <string, int> subscriptions;
    queue <struct msg_server_to_tcp> offline_msg;
    int socket;
    int status;
};

// Structura folosita pentru a stoca un mesaj
// primit de la udp
struct __attribute__((packed)) msg_from_udp {
    char topic[MAX_TOPIC];
    uint8_t type;
    char val_msg[MAX_PAYLOAD + 1];
};

// Structura folosita de clientii tcp pentru a trimite mesaje,
// primite de la tastatura, catre server
struct msg_tcp_to_server {
    char type[MAX_TYPE];
    char topic[MAX_TOPIC];
    int sf;
};

// Structura folosita de server pentru a trimite
// mesaje catre clientul tcp
struct __attribute__((packed)) msg_server_to_tcp {
    char ip_client_udp[MAX_IP_UDP];
    uint16_t port_client_udp;
    char topic[MAX_TOPIC];
    uint8_t type;
    char val_msg[MAX_PAYLOAD + 1];
};

void complete_val_msg(struct msg_server_to_tcp *msg_to_client, 
					  struct msg_from_udp *msg_rec_udp);

void complete_struct_addr(int portno, struct sockaddr_in* proto_addr);

void send_exit_msg(map<string, struct client*> clients);

void complete_msg_from_udp(struct msg_from_udp* msg_rec_udp, 
						   char buffer[BUFLEN]);

void manage_msg_from_udp(map<string, struct client*>* clients, string buf_topic,
						 struct msg_server_to_tcp *msg_to_client,
						 vector<struct client*> topic_clients);

void send_msg_from_queue(map<string, struct client*>* clients, char id[BUFLEN]);

void erase_subscription(map<string, vector<struct client*>>* topics,
						struct client* actual_client, 
						struct msg_tcp_to_server *msg_client);

void free_topics(map<string, vector<struct client*>>* topics);

void free_sockets(map<int, struct client*>* clients_sock);

void free_clients(map<string, struct client*>* clients);

char *convert_data_type(uint8_t type);

void usage(char *file);