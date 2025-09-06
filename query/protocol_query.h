#ifndef PROTOCOL_QUERY_H
#define PROTOCOL_QUERY_H

#include <cstdint>
namespace Query {

/*
    mappings:
    AND:    & (subexpr1) (subexpr2)
    OR:     | (subexpr1) (subexpr2)
    NOT:    - (subexpr1) >
    PHRASE: < (phrase) >
*/
namespace Protocol {

/* RPC message symbol definitions */
const char AND = '&';
const char OR = '|';
const char OR_SYN = '/';
const char NOT = '-';
const char WORD_START = '{';
const char PHRASE_START = '<';
const char PHRASE_END = '>';
const char ESCAPE = '\\';
const char STEP_DELIM = ';';
const char QUERY_END = '#';

/* defines a ratio between the number of results based upon the original term and the number of results based upon
 * synonyms */
const uint32_t STEP_TERM_ORIGINAL = 2;
const uint32_t STEP_TERM_SYNONYM = 1;

}; /* namespace Protocol */

}; /* namespace Query */

#endif /* PROTOCOL_QUERY_H */