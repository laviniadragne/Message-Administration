#include "functions.h"

using namespace std;


int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN_CL];
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	// Numarul de argumente introduse de la tastatura este gresit
	if (argc != 4) {
		usage(argv[0]);
	}

	char *client_id;
	char *ip_server;
	char *port_server;

	// Memorez id-ul clientului
	client_id = strdup(argv[1]);
	if (client_id == NULL) {
		fprintf(stderr, "Can't alloc memory for client_id\n");
		free(client_id);
		exit(0);
	}

	// In cazul in care id-ul este mai mare de 10 caractere
	DIE(strlen(client_id) > 10, "CLIENT_ID must be a maximum of 10 characters!\n");

	// Memorez ip-ul serverului
	ip_server = strdup(argv[2]);
	if (ip_server == NULL) {
		fprintf(stderr, "Can't alloc memory for server_addr\n");
		free(client_id);
		free(ip_server);
		exit(0);
	}

	// Memorez portul serverului
	port_server = strdup(argv[3]);
	if (port_server == NULL) {
		fprintf(stderr, "Can't alloc memory for server_port\n");
		free(client_id);
		free(ip_server);
		free(port_server);
		exit(0);
	}

	// Creeaza un nou socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// Completare structura sockaddr_in
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(port_server));
	ret = inet_aton(ip_server, &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// Ma conectez la portul si ip-ul serverului primite ca si parametru
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// Ii trimit serverului id-ul clientului
	ret = send(sockfd, client_id, strlen(client_id) + 1, 0);
	DIE(ret < 0, "send");	

	// Dezactivez alg lui Nagle
	int opt_val = 1;
	DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
		(char *)&opt_val, sizeof(int)) == -1, "setsockopt");

	// Sterg toti descriptorii existenti
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Adaug socket-ul curent in multimea de read_fds
	FD_SET(sockfd, &read_fds);
	// Adaug STDIN-ul in multimea de read_fds
	FD_SET(STDIN_FILENO, &read_fds);

	fdmax = sockfd;

	while (1) {
		
		// Initializez si aloc o noua structura de tip msg pentru 
		// a trimit mesaje la server
		struct msg_tcp_to_server msg_tcp_to_server;
		memset(&msg_tcp_to_server, 0, sizeof(msg_tcp_to_server));

		// Copiez multimea de read_fds, pentru ca
		// la fiecare apel de select aceasta va fi distrusa
		tmp_fds = read_fds; 

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// Daca trebuie sa citesc date de la tastatura
		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// se citeste de la tastatura mesajul
			memset(buffer, 0, BUFLEN_CL);
			DIE((fgets(buffer, BUFLEN_CL - 1, stdin) == NULL), "fgets"); 

			// Am primit exit de la tastatura
			if (strncmp(buffer, "exit", 4) == 0) {
				// Trimit mesaj de exit serverului
				char *exit_msg = nullptr;
				n = send(sockfd, exit_msg, 0, 0);
				DIE(n < 0, "send");	
				break;
			}

			char buf1[MAX_TYPE], buf2[MAX_TOPIC_BUF], buf3[MAX_SF];
			ret = sscanf(buffer, "%s %s %s\n", buf1, buf2, buf3);
			// Daca nu s-au citit parametrii bine
			DIE(ret < 2, "Wrong input from stdin for tcp client.");

			// Comanda subscribe trebuie sa aiba 3 parametri
			if ((strncmp(buf1, "subscribe", 9) == 0) && ret != 3){
				fprintf(stderr, "Subscribe command must have 3 parameters!\n");
				exit(0);
			}

			// Comanda nu este de niciun tip
			if ((strncmp(buf1, "subscribe", 9) != 0) 
				&& (strncmp(buf1, "unsubscribe", 11) != 0)) {
				fprintf(stderr, "Unknown command from stdin to tcp client!\n");
				exit(0);
			}

			// Topic-ul are mai mult de 50 caractere
			if (strlen(buf2) > 50) {
				fprintf(stderr, "Topic must be a maximum of 50 characters!\n");
				exit(0);
			}
		
			strcpy(msg_tcp_to_server.type, buf1);
			strcpy(msg_tcp_to_server.topic, buf2);
			msg_tcp_to_server.sf = atoi(buf3);

			// Se trimite mesajul la server
			n = send(sockfd, (char *) &msg_tcp_to_server, sizeof(msg_tcp_to_server), 0);
			DIE(n < 0, "send");		

			// Se afiseaza, in functie de tipul mesajului primit de la stdin, pe ecran
			if (strncmp(msg_tcp_to_server.type, "subscribe", 9) == 0) {
				printf("Subscribed to topic.\n");
			}
			else {
				if (strncmp(msg_tcp_to_server.type, "unsubscribe", 11) == 0) {
					printf("Unsubscribed from topic.\n");
				}
			}

		// Daca primesc date pe acel socket
		} else if (FD_ISSET(sockfd, &tmp_fds)) {

			memset(buffer, 0, BUFLEN_CL);
			ret = recv(sockfd, buffer, BUFLEN_CL, 0);
			DIE(ret == -1, "recv");

			// Am primit exit
			if (ret == 0) {
				// conexiunea s-a inchis
				close(sockfd);
				// se scoate din multimea de citire socketul inchis 
				FD_CLR(sockfd, &read_fds);
				free(client_id);
				free(ip_server);
				free(port_server);
				return 0;
			} else {
				// Declar o structura de tip msg_server_to_tcp
				struct msg_server_to_tcp* msg_server_to_tcp = (struct msg_server_to_tcp*) 
																calloc(1, sizeof(struct msg_server_to_tcp));

				// Memorez mesajul primit de la server
				memcpy(msg_server_to_tcp, buffer, sizeof(*msg_server_to_tcp));

				char *convert_type = convert_data_type(msg_server_to_tcp->type);
				if (convert_type == NULL) {
					free(msg_server_to_tcp);
					fprintf(stderr, "Doesn't exist this type of data.\n");
					break;
				}
				
				// Afisez mesajul corespunzator
				if (msg_server_to_tcp->val_msg[strlen (msg_server_to_tcp->val_msg) - 1] == '0' 
					&& msg_server_to_tcp->val_msg[strlen (msg_server_to_tcp->val_msg) - 2] == '0') {
							msg_server_to_tcp->val_msg[strlen (msg_server_to_tcp->val_msg) - 3] = '\0';
				}

				printf("%s - %s - %s\n", msg_server_to_tcp->topic, 
					  convert_type, msg_server_to_tcp->val_msg);
				free(convert_type);
				free(msg_server_to_tcp);
			}
		}
	}

	free(client_id);
	free(ip_server);
	free(port_server);
	close(sockfd);
	return 0;
}
