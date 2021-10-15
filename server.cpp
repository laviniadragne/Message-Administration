#include "functions.h"

using namespace std;


int main(int argc, char *argv[])
{
	int sockfd_tcp, newsockfd_tcp, portno, sockfd_udp;
	char buffer[BUFLEN];
	struct sockaddr_in tcp_addr, udp_addr, newtcp_addr;
	int n, i, ret;
	socklen_t tcplen, udplen;

	// Map-ul de clienti abonati
	// cu key id-ul clientului si
	// value clientul in sine
	map<string, struct client*> clients;

	// Map cu key valoarea topicului
	// si valoare lista de clienti abonati la acel topic
	map<string, vector<struct client*>> topics;

	// Map cu sockets
	map<int, struct client*> clients_sock;
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc != 2) {
		usage(argv[0]);
	}

	// Se goleste multimea de descriptori de citire (read_fds) 
	// si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Deschid socket-ul pentru comunicatiile tcp
	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd_tcp < 0, "socket");

	// Deschid socketul pentru comunicatiile udp
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockfd_udp < 0, "socket");

	int opt_val = 1;
	if (setsockopt(sockfd_tcp, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int)) < 0)
    	fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// Completez datele in structura sockaddr_in
	// pentru comunicarea cu clientul tcp si udp
	complete_struct_addr(portno, &tcp_addr);
	complete_struct_addr(portno, &udp_addr);

	memset((char *) &newtcp_addr, 0, sizeof(newtcp_addr));

	ret = bind(sockfd_tcp, (struct sockaddr *) &tcp_addr,
			   sizeof(struct sockaddr_in));
	DIE(ret < 0, "bind");

	ret = bind(sockfd_udp, (struct sockaddr *) &udp_addr,
			   sizeof(struct sockaddr_in));
	DIE(ret < 0, "bind");

	ret = listen(sockfd_tcp, INT_MAX);
	DIE(ret < 0, "listen");

	// Se adauga noul file descriptor (socketul pe care se
	// asculta conexiuni) in multimea read_fds
	FD_SET(sockfd_tcp, &read_fds);
	FD_SET(sockfd_udp, &read_fds);
	// Se adauga si STDIN-ul pentru a asculta comenzile de
	// la tastatura
	FD_SET(STDIN_FILENO, &read_fds);

	fdmax = max(sockfd_tcp, sockfd_udp);

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				
				// mesaj de la tastatura
				if (i == STDIN_FILENO) {
					DIE((fgets(buffer, BUFLEN - 1, stdin) == NULL), "fgets");
					if (strcmp(buffer, "exit\n") == 0) {
						// trimit mesaj de notificare catre toti clientii tcp
						// conectati
						send_exit_msg(clients);

						free_topics(&topics);
						// free_clients(&clients);
						// free_sockets(&clients_sock);
						clients.clear();
						topics.clear();
						clients_sock.clear();
						close(sockfd_tcp);
						close(sockfd_udp);
						close(newsockfd_tcp);
						return 0;
					}
					else {
						fprintf(stderr, "Unknown command from stdin to server\n");
						exit(0);
					}
				}

				else {
					// mesaj de la clientul udp
					if (i == sockfd_udp) {
						// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
						// pe care serverul o accepta
						memset(buffer, 0, BUFLEN);

						udplen = sizeof(udp_addr);
						n = recvfrom(sockfd_udp, buffer, BUFLEN, 0, (sockaddr*) &udp_addr, &udplen);
						DIE(n < 0, "recv");

						// Aloc o structura de tip mesaj udp si retin datele in ea primite
						struct msg_from_udp *msg_rec_udp_copy = (struct msg_from_udp*) 
																calloc(1, sizeof(struct msg_from_udp));
						struct msg_from_udp msg_rec_udp = *(msg_rec_udp_copy);

						complete_msg_from_udp(&msg_rec_udp, buffer);

						// Pregatesc mesajul cu informatiile despre topic pentru clientul TCP
						struct msg_server_to_tcp *msg_to_client = (struct msg_server_to_tcp*)
																 	malloc(sizeof(struct msg_server_to_tcp));
						memset(msg_to_client->ip_client_udp, 0, sizeof(msg_to_client->ip_client_udp));

						// Completez ip ul si port-ul corespunzatoare
						strncpy((*msg_to_client).ip_client_udp, inet_ntoa(udp_addr.sin_addr), 
								sizeof(inet_ntoa(udp_addr.sin_addr)));
						(*msg_to_client).port_client_udp = ntohs(udp_addr.sin_port);

						// Completez camp cu camp mesajul pentru clientul tcp din
						// mesajul primit de la clientul udp
						complete_val_msg(msg_to_client, &msg_rec_udp);

						string buf_topic(msg_rec_udp.topic);

						// Caut toti clientii tcp pe care ii am dupa topic
						// si pentru cei care au statusul online trimit mesajul;
						// pentru cei cu status offline, retin mesajele in coada
						auto itr_topic_clients = topics.find(buf_topic);

						// Nu exista acest topic in map, il adaug cu o lista de clienti goala
						if (itr_topic_clients == topics.end()) {
							// Creez un vector gol cu constructor default
							vector<struct client *> *empty_clients = new vector<struct client*>();
							topics[msg_rec_udp.topic] = *empty_clients;
						}
						else {
							// Trimit mesajele catre clientii conectati, iar pentru fiecare
							// client deconectat retin mesajele intr-o coada
							manage_msg_from_udp(&clients, buf_topic, msg_to_client,
												itr_topic_clients->second);
						}

						free(msg_rec_udp_copy);
					}
					else 
					{
						// mesaj de la clientul tcp
						if (i == sockfd_tcp) {
							// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
							// pe care serverul o accepta
							tcplen = sizeof(sockaddr);
							newsockfd_tcp = accept(sockfd_tcp, (struct sockaddr *) &newtcp_addr, &tcplen);
							DIE(newsockfd_tcp < 0, "accept");
							
							// Dezactivez alg lui Nagle
							int opval = 1;
							DIE (setsockopt(newsockfd_tcp, IPPROTO_TCP, TCP_NODELAY, 
								(char *)&opval, sizeof(int)) == -1, "setsockopt");

							// se adauga noul socket intors de accept() la multimea
							// descriptorilor de citire
							FD_SET(newsockfd_tcp, &read_fds);

							// s-au primit date pe unul din socketii de client,
							// asa ca serverul trebuie sa le receptioneze
							// receptioneaza id-ul clientului
							memset(buffer, 0, BUFLEN);
							n = recv(newsockfd_tcp, buffer, sizeof(buffer), 0);
							DIE(n < 0, "wrong status recv");

							// Caut id-ul clientului in map-ul de clients
							auto itr = clients.find(buffer);

							// L-am gasit in map-ul de clienti
							if (itr != clients.end()) {

								struct client* new_client = itr->second;

								// Este deja conectat (este deja online)
								if (new_client->status == 1) {
									cout<<"Client "<<new_client->id<<" already connected.\n";
									close(newsockfd_tcp);
								}

								// Devine online
								else {
									if (fdmax < newsockfd_tcp) { 
										fdmax = newsockfd_tcp;
									}
									// Schimba statusul si socket-ul
									itr->second->status = 1;
									itr->second->socket = newsockfd_tcp;

									// Ii actualizez socket-ul in map-ul de socketi
									struct client *new_client = new client();

									(*new_client).socket = newsockfd_tcp;
									(*new_client).status = 1;
									string new_id(buffer);
									(*new_client).id = new_id;
									map<string, int>* new_sub = new map<string, int>;
									new_client->subscriptions = *new_sub;
									queue<struct msg_server_to_tcp> *new_queue = 
																		new queue<struct msg_server_to_tcp>;
									new_client->offline_msg = *new_queue;

									clients_sock.insert(pair<int, struct client*> (new_client->socket, new_client)); 

									// Cat timp coada clientului tcp nu e goala trimite mesajele
									// restante
									send_msg_from_queue(&clients, buffer);
									
									printf("New client %s connected from %s:%d.\n", buffer,
									inet_ntoa(newtcp_addr.sin_addr), ntohs(newtcp_addr.sin_port));
								}
							}

							// Nu l-am gasit in map-ul de clienti, trebuie sa-l adaug
							else {
								if (fdmax < newsockfd_tcp) { 
									fdmax = newsockfd_tcp;
								}
							
								struct client *new_client = new client();
								(*new_client).socket = newsockfd_tcp;
								(*new_client).status = 1;
								string new_id(buffer);
								(*new_client).id = new_id;
								map<string, int>* new_sub = new map<string, int>;
								new_client->subscriptions = *new_sub;
								queue<struct msg_server_to_tcp> *new_queue =
																	new queue<struct msg_server_to_tcp>;
								new_client->offline_msg = *new_queue;

								// Il adaug in map-ul de clienti
								clients.insert(pair<string, struct client*> (new_client->id, new_client));
								// Il adaug in map-ul de socketi
								clients_sock.insert(pair<int, struct client*> (new_client->socket, new_client)); 

								printf("New client %s connected from %s:%d.\n", buffer,
								inet_ntoa(newtcp_addr.sin_addr), ntohs(newtcp_addr.sin_port));
							}

						} 
						else {
							// s-au primit date pe unul din socketii de client,
							// asa ca serverul trebuie sa le receptioneze
							memset(buffer, 0, BUFLEN);
							n = recv(i, buffer, sizeof(buffer), 0);
							DIE(n < 0, "recv");

							// Caut pe baza socketului
							auto itr_sock = clients_sock.find(i);

							// Am gasit clientul
							if (itr_sock != clients_sock.end()) {
								struct client* actual_client = itr_sock->second;

								// Se inchide conexiunea
								if (n == 0) {
									// Conexiunea s-a inchis
									itr_sock->second->status = 0;
									cout<<"Client "<<actual_client->id<<" disconnected.\n";
									close(i);
									
									// Se scoate din multimea de citire socketul inchis 
									FD_CLR(i, &read_fds);
								} 

								// Se aboneaza/dezaboneaza la un topic
								else {
									// Memorez mesajul primit de la clientul TCP
									struct msg_tcp_to_server *msg_client = (struct msg_tcp_to_server*) 
																			malloc(sizeof(struct msg_tcp_to_server));
									memcpy(msg_client, buffer, sizeof(struct msg_tcp_to_server));

									// Daca se aboneaza 
									if (strncmp ((*msg_client).type, "subscribe", 9) == 0) {
										// Il fac online
										itr_sock->second->status = 1;
										// Ii adaug noul topic, daca nu exista deja

										string copy_topic(msg_client->topic);
										auto itr_subscript = itr_sock->second->subscriptions.find(copy_topic);

										// Topicul nu exista
										if (itr_subscript == itr_sock->second->subscriptions.end()) {
											// il adaugam in map-ul de subscription din fiecare client
											// cu key topic-ul si value sf-ul
											string copy_topic(msg_client->topic);
											itr_sock->second->subscriptions.insert
												(pair<string, int> (copy_topic, msg_client->sf));

											// Il adaugam in map-ul de topicuri
											// il caut in map-ul de topicuri 
											auto itr_topic = topics.find(copy_topic);

											// Daca nu l-am gasit, creez un vector nou in care adaug si clientul
											// actual
											vector<struct client*> new_topic = *(new vector<struct client*>);

											if (itr_topic == topics.end()) {
												new_topic.push_back(actual_client);
												string copy_topic(msg_client->topic);
												topics.insert(pair<string, vector <struct client *>> (copy_topic, new_topic));
											}
											// Daca l-am gasit, doar adaug noul client
											else {
												// Daca era cineva abonat
												if (itr_topic->second.size() != 0) {
													itr_topic->second.push_back(actual_client);
												}
												// Inca nu e nimeni abonat
												else {
													new_topic.push_back(actual_client);
													itr_topic->second = new_topic;
												}
											}
										}
										// Topicul exista si trebuie sa-i actualizam sf-ul
										else {
											itr_subscript->second = (*msg_client).sf;
										}
									}

									else {
										// Clientul se dezaboneaza
										if (strncmp ((*msg_client).type, "unsubscribe", 11) == 0) {
											// Caut topicul in lista clientului
											string copy_topic((*msg_client).topic);
											auto itr_client_topic = itr_sock->second->subscriptions.find(copy_topic);
											// Nu era abonat la acel topic clientul
											if (itr_client_topic == itr_sock->second->subscriptions.end()) {
												fprintf(stderr, "Unknown topic to unsuscribe for this client tcp\n");
												exit(-1);
											}
											// Era abonat, sterg topicul din map-ul de subscriptions
											// si clientul din map-ul de topics
											else {
												itr_client_topic = 
														itr_sock->second->subscriptions.erase(itr_client_topic);

												// Sterge topicul din lista de abonari, ale clientului actual,
												// si clientul din map-ul de topicuri
												erase_subscription(&topics, actual_client, msg_client);

											}
										}
										// Comanda necunoscuta de la clientul tcp
										else {
											fprintf(stderr, "Unknown command from client tcp\n");
											exit(-1);
										}
									}

									free(msg_client);
								}
							}
							// Nu am am gasit in map clientul
							else {
								fprintf(stderr, "Unknown client socket\n");
								exit(-1);
							}
						}
					}
				}
			}
		}
	}


	// free_clients(&clients);
	free_topics(&topics);
	// free_sockets(&clients_sock);
	clients.clear();
	topics.clear();
	clients_sock.clear();
	// free(&clients);
	// free(&topics);
	// free(&clients_sock);
	close(sockfd_tcp);
	close(sockfd_udp);
	close(newsockfd_tcp);

	return 0;
}
