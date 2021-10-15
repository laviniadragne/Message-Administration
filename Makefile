# Protocoale de comunicatii:
# Laborator 8: Multiplexare
# Makefile

CFLAGS = -Wall -g

# Portul pe care asculta serverul (de completat)
PORT = 

# Adresa IP a serverului (de completat)
IP_SERVER = 

all: server subscriber

# Compileaza server.cpp
server: server.cpp functions.cpp

# Compileaza subscriber.cpp
subscriber: subscriber.cpp functions.cpp


.PHONY: clean run_server run_client

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul tcp
run_subscriber:
	./subscriber ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber functions
