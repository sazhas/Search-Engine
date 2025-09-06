CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I.. -I../..
LDFLAGS = -pthread

# Targets
TARGETS = test test2 test3 test4

# Sources and object files for each test
SRCS_test = test.cpp ../../parser/HtmlParser.cpp ../../parser/HtmlTags.cpp
OBJS_test = $(SRCS_test:.cpp=.o)

SRCS_test2 = test2.cpp ../../parser/HtmlParser.cpp ../../parser/HtmlTags.cpp
OBJS_test2 = $(SRCS_test2:.cpp=.o)

SRCS_test3 = test3.cpp ../../parser/HtmlParser.cpp ../../parser/HtmlTags.cpp
OBJS_test3 = $(SRCS_test3:.cpp=.o)

SRCS_test4 = test4.cpp ../../parser/HtmlParser.cpp ../../parser/HtmlTags.cpp
OBJS_test4 = $(SRCS_test4:.cpp=.o)

.PHONY: all clean

all: $(TARGETS)

test: $(OBJS_test)
	$(CXX) $(OBJS_test) -o $@ $(LDFLAGS)

test2: $(OBJS_test2)
	$(CXX) $(OBJS_test2) -o $@ $(LDFLAGS)

test3: $(OBJS_test3)
	$(CXX) $(OBJS_test3) -o $@ $(LDFLAGS)

test4: $(OBJS_test4)
	$(CXX) $(OBJS_test4) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_test) $(OBJS_test2) $(OBJS_test3) $(OBJS_test4) $(TARGETS)


