# ======================
# Compiler & flags
# ======================
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# ======================
# Directories
# ======================
SERVER_DIR = server
USER_DIR   = user

# ======================
# Executables
# ======================
SERVER = $/ES
USER   = $/USER

# ======================
# Source files
# ======================
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

# ======================
# Object files
# ======================
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
USER_OBJ   = $(USER_SRC:.cpp=.o)

# ======================
# Default target
# ======================
all: $(SERVER_BIN) $(USER_BIN)

# ======================
# Build server
# ======================
$(SERVER_BIN): $(SERVER_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ======================
# Build user
# ======================
$(USER_BIN): $(USER_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ======================
# Create bin directory
# ======================
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ======================
# Pattern rule
# ======================
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ======================
# Convenience targets
# ======================
server: $(SERVER_BIN)

user: $(USER_BIN)

clean:
	rm -rf $(SERVER_OBJ) $(USER_OBJ) $(BIN_DIR)

.PHONY: all clean server user
