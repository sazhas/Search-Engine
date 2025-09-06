# Makefile just for csolver test file

CXX = g++
CXXFLAGS = -std=c++17 -g3 -I. -Iindexer -Iquery

SRC = \
    csolver/test_cs_on_index_chunk.cpp \
    csolver/ConstraintSolver.cpp \
    query/query.cpp \
    indexer/ast.cpp

TARGET = test_cs_on_index_chunk

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
