# Compiler and flags
CXX      = g++
CXXFLAGS = -g -std=c++17 -Wall -Wextra -O2
LDFLAGS  = -pthread

# Sources and target
SOURCES  = LinuxTinyServer.cpp ../query/query.cpp ../lib/stemmer/stemmer.cpp  ../query/synsets.cpp
OBJECTS  = $(SOURCES:.cpp=.o)
TARGET   = server

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean