#include "functions.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
	exit(0);
}

// Completeaza datele despre topic in functie
// de tipul primit
void complete_val_msg(struct msg_server_to_tcp *msg_to_client, 
					  struct msg_from_udp *msg_rec_udp) {
	memset(msg_to_client->topic, 0, sizeof(msg_to_client->topic));
	memset(msg_to_client->val_msg, 0, sizeof(msg_to_client->val_msg));
	memset(&msg_to_client->type, 0, sizeof(msg_to_client->type));

	strncpy(msg_to_client->topic, msg_rec_udp->topic, sizeof(msg_rec_udp->topic));

	char* copy_msg  = (char *) calloc(1, sizeof(msg_rec_udp->val_msg));
	memcpy(copy_msg, msg_rec_udp->val_msg, sizeof(msg_rec_udp->val_msg));

	if (msg_rec_udp->type == 0) {
		// completez type-ul, cu cel primit de la UDP (0, 1, 2 sau 3)
		msg_to_client->type = msg_rec_udp->type;
		uint32_t aux;
		// completez octetul de semn
		aux = ntohl(*(uint32_t *) (copy_msg + 1));
		if (!msg_rec_udp->val_msg[0]) {
			sprintf(msg_to_client->val_msg, "%u", aux);
		}
		else {
			sprintf(msg_to_client->val_msg, "-%u", aux);
		}
	}
	else {
		if (msg_rec_udp->type == 1) {
			msg_to_client->type = msg_rec_udp->type;
			float aux;
			aux = ntohs(*(uint16_t *) (copy_msg));
			aux = aux / 100;
			setprecision(2);
			sprintf(msg_to_client->val_msg, "%.2f", aux);
		}
		else {
			if (msg_rec_udp->type == 2) {
				msg_to_client->type = msg_rec_udp->type;
				float aux;
				aux = ntohl(*(uint32_t *) (copy_msg + 1));
				aux = aux / pow(10, (*msg_rec_udp).val_msg[5]);
				// Completez octetul de semn
				if (!msg_rec_udp->val_msg[0]) {
					sprintf(msg_to_client->val_msg, "%lf", aux);
				}
				else {
					sprintf(msg_to_client->val_msg, "-%lf", aux);
				}
			}
			else {
				if (msg_rec_udp->type == 3) {
					msg_to_client->type = msg_rec_udp->type;
					sprintf(msg_to_client->val_msg, "%s", copy_msg);
				}
				// Mesaj necunoscut
				else {
					printf("%c\n", msg_rec_udp->type);
					fprintf(stderr, "Unknown type of message from udp client\n");
				}
			}
		}
	}
	free(copy_msg);
}


// Completeaza datele despre o structura de tip sockaddr_in
// pe baza unui numar de port dat
void complete_struct_addr(int portno, struct sockaddr_in* proto_addr) {
	memset((char *) proto_addr, 0, sizeof(*proto_addr));
	proto_addr->sin_family = AF_INET;
	proto_addr->sin_port = htons(portno);
	proto_addr->sin_addr.s_addr = INADDR_ANY;
}


// Trimite mesaje empty catre clientii inca online
// pentru a-i anunta ca serverul isi inchide conexiunea
void send_exit_msg(map<string, struct client*> clients) {
	int n;
	for (auto it = clients.begin(); it != clients.end(); it++) {
		// Daca e conectat ii trimit mesaj de inchidere
		if (it->second->status == 1) {
			char *exit_msg = nullptr;
			n = send(it->second->socket, exit_msg, 0, 0);
			DIE(n < 0, "couldn't send message");
		}
	}
}


// Completeaza pe baza buffer-ului primit, campurile dintr-o structura,
// de tip mesaj de la clientul udp
void complete_msg_from_udp(struct msg_from_udp* msg_rec_udp, 
						   char buffer[BUFLEN]) {
	memcpy(msg_rec_udp->topic, buffer, sizeof(msg_rec_udp->topic));
	msg_rec_udp->type = buffer[sizeof(msg_rec_udp->topic)];
	memcpy(msg_rec_udp->val_msg, buffer + sizeof(msg_rec_udp->topic) + 1, 
		sizeof(msg_rec_udp->val_msg));
}


// Pe baza map-ului de clienti trimit mesajele primite de la clientul udp
// catre clientii tcp online abonati la acel topic, iar pentru cei offline
// retin mesajele intr-o coada, pentru fiecare client in parte
void manage_msg_from_udp(map<string, struct client*>* clients, string buf_topic,
						 struct msg_server_to_tcp *msg_to_client,
						 vector<struct client*> topic_clients) {
	int cnt, n;
	// Parcurg lista de clienti abonati la acel topic
	for (cnt = 0; cnt < topic_clients.size(); cnt++) {
		// Daca sunt online le trimit mesajul
		if (topic_clients[cnt]->status == 1) {
			n = send(topic_clients[cnt]->socket, (char*) msg_to_client, 
					sizeof(struct msg_server_to_tcp), 0);
			DIE(n < 0, "couldn't send message");
		}
		// Daca nu sunt online adaug mesajele in coada, pentru cei cu sf-ul = 1
		else {
			// Caut in lista de subscriptii a clientului daca e abonat la acel topic
			// cu sf-ul = 1 
									
			// Il caut in map-ul de clienti pe baza id-ului
			auto itr_client = clients->find(topic_clients[cnt]->id);
									
			// Daca il gasesc ii adaug in coada mesajul
			if (itr_client != clients->end()) {
				auto itr_sf = itr_client->second->subscriptions.find(buf_topic);
				// Daca are sf-ul = 1
				if (itr_sf->second == 1) {
					itr_client->second->offline_msg.push(*msg_to_client);
				}
			}
			else {
				fprintf(stderr, "Unknown client in map of clients\n");
				exit(0);
			}
		}
	}
}


// Cauta pe baza id-ului, primit in buffer, clientul abonat
// si ii trimite mesajele restante, salvate in coada
void send_msg_from_queue(map<string, struct client*>* clients, char id[BUFLEN]) {
	int n;
	auto itr = clients->find(id);
	while(!itr->second->offline_msg.empty()) {
		struct msg_server_to_tcp* restant_msg = (struct msg_server_to_tcp*) 
											malloc(sizeof(struct msg_server_to_tcp));
		(*restant_msg) = itr->second->offline_msg.front();
		n = send(itr->second->socket, (char*) restant_msg, sizeof(*restant_msg), 0);
		DIE(n < 0, "send");
		free(restant_msg);
		itr->second->offline_msg.pop();
	}
}


// Cand se primeste comanda de unsubscribe se elimina
// topicul din lista de topicuri ai acelui client
// si clientii din map-ul de topics
void erase_subscription(map<string, vector<struct client*>>* topics,
						struct client* actual_client, 
						struct msg_tcp_to_server *msg_client) {
		// Caut in vectorul de clienti ai topicului
		auto itr_topic = topics->find(msg_client->topic);

		// Am gasit clienti pentru acel topic
		if (itr_topic != topics->end()) {
			vector<struct client *> cli_topic = itr_topic->second;
			int cnt = 0;
			int ok = 0;
			while (cnt < cli_topic.size() && ok == 0) {
				if (cli_topic[cnt]->id == actual_client->id) {
					ok = 1;
				}
				else {
					cnt++;
				}
			}
			// Sterg clientul din vectorul corespunzator acelui topic
			if (ok == 1) {
				itr_topic->second.erase(cli_topic.begin() + cnt);
			}
		}
		else {
			fprintf(stderr, "Error to erase client from this topic\n");
			exit(-1);
		}
}


// Sterg datele din map-ul de clients
void free_clients(map<string, struct client*>* clients) {
	for (auto it = (*clients).begin(); it != (*clients).end(); it++) {
		if (it->second != nullptr) {
			delete it->second;
		}
	}
}

// Sterg datele din map-ul de topics
void free_topics(map<string, vector<struct client*>>* topics) {
	for (auto it = (*topics).begin(); it != (*topics).end(); it++) {
		// Sterg fiecare client din vector
		auto myIter = it->second.begin();
   		while(myIter!= it->second.end()) {
			(*myIter)->subscriptions.clear();
			queue <struct msg_server_to_tcp> empty;
			swap((*myIter)->offline_msg, empty);
			// delete(*myIter);
			++myIter;
    	}
		it->second.clear();
		// // Sterg vectorii
		// delete &(it->second);
	}
}

// Sterg datele din map-ul de socketi-clienti
void free_sockets(map<int, struct client*>* clients_sock) {
	for (auto it = (*clients_sock).begin(); it != (*clients_sock).end(); it++) {
		delete it->second;
	}
}

char *convert_data_type(uint8_t type) {
	char *convert_type;
	if (type == 0) {
		convert_type = strdup("INT\0");
	}
	else {
		if (type == 1) {
			convert_type = strdup("SHORT_REAL\0");
		}
		else {
			if (type == 2) {
				convert_type = strdup("FLOAT\0");
			}
			else {
				if (type == 3) {
					convert_type = strdup("STRING\0");
				}
				else {
					convert_type = NULL;
				}
			}
		}
	}

	return convert_type;
}

