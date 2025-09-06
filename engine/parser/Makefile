CXX = g++
CXXFLAGS = -O3 -std=c++17 -pthread
LDFLAGS = -lcrypto
SOURCES = Parser.cpp HtmlParser.cpp HtmlTags.cpp ../lib/stemmer/stemmer.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = html_parser

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	CXXFLAGS = "-I$(brew --prefix openssl)/include $CXXFLAGS"
	LDFLAGS += "-L$(brew --prefix openssl)/lib $LDFLAGS"
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

