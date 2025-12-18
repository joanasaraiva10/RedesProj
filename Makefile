CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SERVER_DIR = server
USER_DIR   = user

SERVER_BIN = ES
USER_BIN   = User

SERVER_SRC = \
	$(SERVER_DIR)/main.cpp \
	$(SERVER_DIR)/events.cpp \
	$(SERVER_DIR)/parser.cpp \
	$(SERVER_DIR)/protocol.cpp \
	$(SERVER_DIR)/reservations.cpp \
	$(SERVER_DIR)/tcp_handler.cpp \
	$(SERVER_DIR)/tcp.cpp \
	$(SERVER_DIR)/udp_handler.cpp \
	$(SERVER_DIR)/udp.cpp \
	$(SERVER_DIR)/users.cpp \
	$(SERVER_DIR)/utils.cpp

USER_SRC = \
	$(USER_DIR)/main.cpp \
	$(USER_DIR)/parser.cpp \
	$(USER_DIR)/tcp_client.cpp \
	$(USER_DIR)/tcp_handler.cpp \
	$(USER_DIR)/udp_client.cpp \
	$(USER_DIR)/udp_handler.cpp \
	$(USER_DIR)/file_utils.cpp

SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
USER_OBJ   = $(USER_SRC:.cpp=.o)

all: $(SERVER_BIN) $(USER_BIN)

$(SERVER_BIN): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(USER_BIN): $(USER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

server: $(SERVER_BIN)
user: $(USER_BIN)

clean:
	rm -f $(SERVER_BIN) $(USER_BIN) $(SERVER_OBJ) $(USER_OBJ)

.PHONY: all clean server user
