#include <iostream>
#include "../query.h"

using namespace std;
using namespace Query;

int main(int argc, char* argv[]) {
    bool demo = false;
    if (argc > 1) {
        demo = (argv[1][0] == 'd');
    }

    // Initialize the Query_Compiler
    Query_Compiler::init_instance(std::vector{CSolverInfo("127.0.0.1", 9000)}, "/home/alexycn/Downloads/synsets.txt");
    Query_Compiler* compiler = Query_Compiler::get_instance();

    if(demo) {
        // Test cases
        vector<string> tests = {
            // Simple queries
            "a",                          // Single term
            "-a",                         // NOT operator
            "a & b",                      // AND operator
            "a | b",                      // OR operator

            // Nested expressions
            "a & (b | c)",                // AND with nested OR
            "(a & b) | c",                // OR with nested AND
            "-(a & b)",                   // NOT with nested AND
            "a & (b | (c & d))",          // Multiple levels of nesting
            "\"lmao lol\" & lmao",
            // Complex expressions
            "(a | b) & (c | d)",          // Multiple ORs and ANDs
            "-(a | (b & c))",             // NOT with nested OR and AND
            "a & b & c & d",              // Multiple ANDs
            "a | b | c | d",              // Multiple ORs


            // // Edge cases
            // "",                           // Empty query
            // "()",                         // Empty parentheses
            // "-()",                        // NOT with empty parentheses
            // "a &",                        // Trailing operator
            // "& a",                        // Leading operator
            // "(a & b",                     // Missing closing parenthesis
            // "a & b)",                     // Missing opening parenthesis
            // "a & (b | -)",                // Invalid NOT usage
            // "a & (b | (c & ))",           // Missing term in nested expression
        };

        for (const auto& query : tests) {
            cout << "Query: " << query << endl;

            compiler->send_query(query);
        }
    }

    else {
        std::string query;
        std::cout << "Enter your query: ";
        while(std::getline(std::cin, query)) {
            // Send the query to the server
            compiler->send_query(query);
            std::cout << "Enter your query: ";
        }
    }

    return 0;
}