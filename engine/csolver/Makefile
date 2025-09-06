# Compiler and flags
CXX      = g++
CXXFLAGS = -std=c++17 -g -Wall -Wextra -O0
LDFLAGS  = 

# Source files (add any additional .cpp files here)
SOURCES  = main.cpp ast.cpp csolver.cpp isr.cpp ../ranker/Ranker.cpp

# Object files (automatically derived from SOURCES)
OBJECTS  = $(SOURCES:.cpp=.o)

# Final executable target name
TARGET   = csolver

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean