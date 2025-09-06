#pragma once

#include <cstddef>

// Thread Count Constants
constexpr const int NUM_CRAWL_THREADS = 256;
constexpr const int NUM_FRONTIER_TALK_THREADS = 64;
constexpr const int NUM_PARSER_TALK_THREADS = 64;
constexpr const int NUM_PARSE_THREADS = 256;
constexpr const int NUM_SEND_THREADS = 64;
constexpr const int NUM_INDEX_SAVE_THREADS = 4;

// Save Times
constexpr const int CRAWLER_SAVE_TIME = 60;
constexpr const int PARSER_SAVE_TIME = 60;

// Parser Constants
constexpr const int PARSER_PORT = 1024;
constexpr const int BLOOM_FRONTIER_SIZE = 200000000;
constexpr const double FRONTIER_FP_RATE = 0.02;

constexpr const char* INDEX_CHUNK_NAME = "index_chunk";
constexpr const char* PARSER_FILTER_FILE = "parser_filter.bin";
constexpr const char* PARSER_PEERS_FILE = "parser_peers.txt";

constexpr const int MAX_FRONTIER_SIZE = 500000;
constexpr const int MIN_PAGES_PER_CHUNK = 5000;

// Frontier Constants
constexpr const int FRONTIER_N = 50000;
constexpr const int FRONTIER_K = 25000;

constexpr const char* FRONTIER_FILE = "frontier.txt";
constexpr const char* FRONTIER_FILE_COPY = "frontier.old.txt";
constexpr const char* FRONTIER_FILTER_FILE = "frontier_filter.bin";

constexpr const int FRONTIER_PORT = 1025;

// Crawler Constants
constexpr const char* CRAWLER_PEERS_FILE = "crawler_peers.txt";

// IOStream Constants
constexpr const size_t COUT_BUFFER_SIZE = 2048;

// HTTP Constants
constexpr const size_t HTTP_BUFFER_SIZE = 1048576;
