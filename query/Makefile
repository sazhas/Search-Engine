CXX = g++
DEBUG_FLAGS = -g3 -DTESTING_SEND
RELEASE_FLAGS = -O2 -DNDEBUG
LDFLAGS = 

# Directories
SRC_DIR = tests
OBJ_DIR = obj
BIN_DIR = bin

# Source files
CLIENT_SRC = $(SRC_DIR)/client.cpp query.cpp synsets.cpp ../lib/stemmer/stemmer.cpp
SERVER_SRC = $(SRC_DIR)/server.cpp

# Output binaries
CLIENT_DEBUG_BIN = $(BIN_DIR)/client_debug
SERVER_DEBUG_BIN = $(BIN_DIR)/server_debug
CLIENT_RELEASE_BIN = $(BIN_DIR)/client_release
SERVER_RELEASE_BIN = $(BIN_DIR)/server_release

# Rules
all: debug release

debug: $(CLIENT_DEBUG_BIN) $(SERVER_DEBUG_BIN)

release: $(CLIENT_RELEASE_BIN)

$(CLIENT_DEBUG_BIN): $(CLIENT_SRC)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(DEBUG_FLAGS) -o $@ $^

$(SERVER_DEBUG_BIN): $(SERVER_SRC)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(DEBUG_FLAGS) -o $@ $^

$(CLIENT_RELEASE_BIN): $(CLIENT_SRC)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(RELEASE_FLAGS) -o $@ $^
	
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) *.o
