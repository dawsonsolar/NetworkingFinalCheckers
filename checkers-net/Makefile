CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS  = -lpthread

SHARED_SRC = shared/CheckersLogic.cpp

SERVER_SRC = server/main.cpp \
             server/GameServer.cpp \
             server/GameRoom.cpp \
             $(SHARED_SRC)

CLIENT_SRC = client/main.cpp \
             $(SHARED_SRC)

.PHONY: all server client clean

all: server client

server: $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o checkers-server $(SERVER_SRC) $(LDFLAGS)
	@echo "Built: checkers-server"

client: $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o checkers-client $(CLIENT_SRC)
	@echo "Built: checkers-client"

clean:
	rm -f checkers-server checkers-client
