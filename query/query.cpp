#include "query.h"

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "../lib/debug.h"
#include "../lib/networking.h"
#include "../lib/stemmer.h"
#include "synsets.h"

using namespace Query;

const char SYM_PHRASE = '$';

bool contains_phrase(const std::string& term) {
    return term[0] == SYM_PHRASE;
}


/* start Expr_Leaf */


Expr_Leaf* Expr_Leaf::generate_expr(const std::string& str) {
    if (contains_phrase(str)) {
        std::vector<std::string> terms;

        std::string term;
        for (auto it = str.begin() + 1; it != str.end(); ++it) {
            const char& c = *it;
            if (c != ' ')
                term.push_back(c);
            else {
                if (!term.empty()) {
                    terms.push_back(term);
                    term.clear();
                }
            }
        }

        if (!term.empty()) {
            terms.push_back(term);
            term.clear();
        }

        return new Expr_Leaf_Phrase { std::move(terms) };

    } else
        return new Expr_Leaf_Word { str };
}

/* end Expr_Leaf */

/**
 * @brief Prepend words with a decorator
 * @param decorator The character to prepend
 * @param term The words to prepend
 * @warning term must start with a word; words must tbe space-delimited
 */
std::string prepend_word_decorator(char decorator, const std::string& term) {
    std::string term_decorated;
    term_decorated.reserve(term.size() + 1);
    term_decorated.push_back(decorator);
    term_decorated.append(term);
    return term_decorated;
}

bool contains_space(const std::string& term) {
    return term.find(' ') != std::string::npos;
}

/* start Expr_Leaf_Word */

Expr_Leaf_Word::Expr_Leaf_Word(const std::string& t)
    : term { t }
    , stem { Stemmer::stem(term) } {}

Expr_Leaf_Word::Expr_Leaf_Word(std::string&& t)
    : term { std::move(t) }
    , stem { Stemmer::stem(term) } {}

Expr_Leaf_Word::Expr_Leaf_Word(const std::string& t, std::string&& s)
    : term { t }
    , stem { std::move(s) } {}

Expr_OR* Expr_Leaf_Word::generate_decorated() const {
    return new Expr_OR(new Expr_Leaf_Word(std::move(prepend_word_decorator('@', term))),
                       const_cast<Expr_Leaf_Word*>(this));
}

Expr_OR_SYN* Expr_Leaf_Word::helper_generate_synonyms(std::stack<Expr_Leaf_Word*>& children) {
    if (children.empty())
        return nullptr;

    else {
        auto term = children.top();
        children.pop();

        return new Expr_OR_SYN(term->generate_decorated(), helper_generate_synonyms(children));
    }
}

Expr_OR_SYN* Expr_Leaf_Word::generate_synonyms() {
    /* get all synsets associated with term */
    const auto* synsets = Synsets::get_synsets(stem);
    if (!synsets) return nullptr;

    std::stack<Expr_Leaf_Word*> children;

    for (const auto& synset : *synsets)
        for (const auto& syn : *synset) {
            /* only save synonyms with different stems - otherwise searches are the same ! side-effect of skipping past
             * self */
            auto stem_syn = Stemmer::stem(syn);
            if (stem_syn != stem) children.push(new Expr_Leaf_Word { syn, std::move(stem_syn) });
        }

    /* generate the topmost node including this term */
    Expr_OR* decorated = generate_decorated();

    Expr_OR_SYN* expr = new Expr_OR_SYN(decorated, helper_generate_synonyms(children), Protocol::STEP_TERM_ORIGINAL,
                                        Protocol::STEP_TERM_SYNONYM);

    return expr;
}

Expr* Expr_Leaf_Word::optimize() {
    if (stem.empty()) {
        delete this;
        return nullptr;
    }

    Expr_OR_SYN* replace = generate_synonyms();

    if (replace)
        return replace;

    else {
        /* no synonyms found, so we can optimize the tree */
        auto* expr = generate_decorated();
        return generate_decorated();
    }
}

void Expr_Leaf_Word::serialize_and_send(int sock) const {
    /*
        {
    */
    send_looping_crashing(sock, &Protocol::WORD_START, 1);

#ifdef TESTING_SEND
    std::cout << Protocol::WORD_START;
#endif

    /*
        {(stem)
    */
    send_looping_crashing(sock, stem.c_str(), stem.size());

#ifdef TESTING_SEND
    std::cout << stem;
#endif

    /*
        {(stem)>
    */
    send_looping_crashing(sock, &Protocol::PHRASE_END, 1);

#ifdef TESTING_SEND
    std::cout << Protocol::PHRASE_END;
#endif
}

std::vector<std::string> generate_stems(const std::vector<std::string>& terms) {
    std::vector<std::string> stems;
    stems.reserve(terms.size());
    for (const auto& term : terms) {
        stems.push_back(Stemmer::stem(term));
    }
    return stems;
}

/* start Expr_Leaf_Phrase */

Expr_Leaf_Phrase::Expr_Leaf_Phrase(std::vector<std::string>&& _terms)
    : terms { std::move(_terms) }
    , stems { std::move(generate_stems(terms)) } {}

Expr_Leaf_Phrase::Expr_Leaf_Phrase(std::vector<std::string>&& _terms, std::vector<std::string>&& _stems)
    : terms { std::move(_terms) }
    , stems { std::move(_stems) } {}

std::pair<bool, std::vector<std::string>> prepend_words_decorator(char decorator,
                                                                  const std::vector<std::string>& terms) {
    bool at_least_one = false;
    std::vector<std::string> terms_decorated;
    terms_decorated.reserve(terms.size());
    for (const auto& term : terms) {
        if (!term.empty()) {
            at_least_one = true;
            terms_decorated.emplace_back(std::move(prepend_word_decorator(decorator, term)));
        }
    }
    return std::make_pair(at_least_one, std::move(terms_decorated));
}

Expr_OR* Expr_Leaf_Phrase::generate_decorated() const {
    auto p1 { std::move(prepend_words_decorator('@', terms)) };
    auto p2 { std::move(prepend_words_decorator('@', stems)) };

    auto& terms_decorated = p1.second;
    auto& [at_least_one, stems_decorated] = p2;

    if (!at_least_one) {
        return nullptr;
    }

    auto* expr = new Expr_OR(new Expr_Leaf_Phrase(std::move(terms_decorated), std::move(stems_decorated)),
                             const_cast<Expr_Leaf_Phrase*>(this));

    return expr;
}

Expr* Expr_Leaf_Phrase::optimize() {
    if (terms.empty()) {
        delete this;
        return nullptr;
    }

    Expr_OR* replace = generate_decorated();

    if (replace)
        return replace;

    else {
        delete this;
        return nullptr;
    }
}

void Expr_Leaf_Phrase::serialize_and_send(int sock) const {
    /*
        <
    */
    send_looping_crashing(sock, &Protocol::PHRASE_START, 1);

#ifdef TESTING_SEND
    std::cout << Protocol::PHRASE_START;
#endif

    /*
        <(stem 1) (stem 2) (...) (stem n)
    */
    for (auto it = stems.begin(); it != stems.end() - 1; ++it) {
        send_looping_crashing(sock, it->c_str(), it->size());
        send_looping_crashing(sock, " ", 1);

#ifdef TESTING_SEND
        std::cout << *it << " ";
#endif
    }
    send_looping_crashing(sock, stems.back().c_str(), stems.back().size());

#ifdef TESTING_SEND
    std::cout << stems.back();
#endif

    /*
        <(stem 1) (stem 2) (...) (stem n)>
    */
    send_looping_crashing(sock, &Protocol::PHRASE_END, 1);

#ifdef TESTING_SEND
    std::cout << Protocol::PHRASE_END;
#endif
}

/* end Expr_Leaf_Phrase */

/* start Expr_Dummy */

Expr_Dummy::Expr_Dummy(Expr* child)
    : child { child } {}

Expr* Expr_Dummy::optimize() {
    if (child) child = child->optimize();

    if (child)
        return this;
    else {
        delete this;
        return nullptr;
    }
}


void Expr_Dummy::serialize_and_send(int sock) const {
    if (child) child->serialize_and_send(sock);
}

/* end Expr_Dummy */

/* start Expr_Op */

std::string url_decode(const std::string& encoded) {
    std::ostringstream decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 < encoded.length()) {
                std::string hex = encoded.substr(i + 1, 2);
                char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
                decoded << ch;
                i += 2;
            }
        } else if (encoded[i] == '+') {
            decoded << ' ';
        } else {
            decoded << encoded[i];
        }
    }
    return decoded.str();
}

Expr_Op::Expr_Op(Expr* left, Expr* right, char SYM)
    : left { left }
    , right { right }
    , SYM { SYM } {}

Expr_Op::~Expr_Op() {
    delete left;
    delete right;
}

void Expr_Op::serialize_and_send(int sock) const {
    /*
        mappings:
        AND:    & (subexpr1) (subexpr2)
        OR:     | (subexpr1) (subexpr2)
        NOT:    - (subexpr1) >
        PHRASE: < (phrase) >
    */

    /*
        (operator)
    */
    PRINTF("Send operator\n");
    send_looping_crashing(sock, &SYM, 1);

#ifdef TESTING_SEND
    std::cout << SYM;
#endif

    /*
        (operator)(subexpr1)
    */
    if (left)
        left->serialize_and_send(sock);
    else {
        send_looping_crashing(sock, &Protocol::PHRASE_END, 1);

#ifdef TESTING_SEND
        std::cout << Protocol::PHRASE_END;
#endif
    }

    /*
        (operator)(subexpr1)_subexpr2)
    */
    if (right)
        right->serialize_and_send(sock);
    else {
        send_looping_crashing(sock, &Protocol::PHRASE_END, 1);

#ifdef TESTING_SEND
        std::cout << Protocol::PHRASE_END;
#endif
    }
}

Expr* Expr_Op::optimize() {
    left = left->optimize();
    right = right->optimize();

    if (left && right)
        return this;

    else if (left) {
        auto tmp = left;
        left = nullptr;
        delete this;
        return tmp;
    }

    else if (right) {
        auto tmp = right;
        right = nullptr;
        delete this;
        return tmp;
    }

    else /* !(left || right) */ {
        delete this;
        return nullptr;
    }
}

void Expr_Op::reassign_child(Expr* child, Expr* child_new) {
    if (child == left) {
        left = child_new;
    } else {
        assert(child == right);
        right = child_new;
    }
}

/* end Expr_Op */

/* start Expr_AND */

Expr_AND::Expr_AND(Expr* left, Expr* right)
    : Expr_Op { left, right, Protocol::AND } {}

Expr* Expr_AND::optimize() {
    return Expr_Op::optimize();
}

/* end Expr_AND */

/* start Expr_OR */

Expr_OR::Expr_OR(Expr* left, Expr* right)
    : Expr_Op { left, right, Protocol::OR } {}

Expr* Expr_OR::optimize() {
    return Expr_Op::optimize();
}

/* end Expr_OR */

/* start Expr_OR_SYN */

Expr_OR_SYN::Expr_OR_SYN(Expr* left, Expr_OR_SYN* rest, uint32_t ratio_term, uint32_t ratio_rest)
    : Expr_Op { left, static_cast<Expr*>(rest), Protocol::OR_SYN }
    , size { 1 + (rest ? rest->size : 0) }
    , ratio_term { ratio_term }
    , ratio_rest { ratio_rest } {}

Expr_OR_SYN::Expr_OR_SYN(Expr* left, Expr_OR_SYN* rest)
    : Expr_OR_SYN { left, rest, 1, rest ? rest->size : 0 } {}

Expr* Expr_OR_SYN::optimize() {
    /* no need to further optimize as the structure is known and generated */
    return this;
}

void Expr_OR_SYN::serialize_and_send(int sock) const {
    Expr_Op::serialize_and_send(sock);

    /*
        (operator)(subexpr1)(subexpr2)(ratio_term);(ratio_rest);
    */

    uint32_t ratio_term_net = htonl(ratio_term);
    uint32_t ratio_rest_net = htonl(ratio_rest);

    const char* ratio_term_net$ = reinterpret_cast<const char*>(&ratio_term_net);
    const char* ratio_rest_net$ = reinterpret_cast<const char*>(&ratio_rest_net);

    send_looping_crashing(sock, ratio_term_net$, sizeof(ratio_term_net));
    send_looping_crashing(sock, ";", 1);
    send_looping_crashing(sock, ratio_rest_net$, sizeof(ratio_rest_net));
    send_looping_crashing(sock, ";", 1);

#ifdef TESTING_SEND
    std::cout << ratio_term << ";";
    std::cout << ratio_rest << ";";
#endif
}

/* end Expr_OR_SYN */

/* start Expr_NOT */

Expr_NOT::Expr_NOT(Expr* term)
    : Expr_Op { term, nullptr, Protocol::NOT } {}

Expr* Expr_NOT::optimize() {
    left = left->optimize();

    if (!left) {
        delete this;
        return nullptr;
    } else
        return this;
}

/* start Query_Compiler */

std::vector<std::string> tokenize(const std::string& query) {
    enum class Mode { DEFAULT, ESCAPED, PHRASE };

    Mode mode = Mode::DEFAULT;


    std::vector<std::string> tokens;

    std::string curr;
    curr.reserve(512);

    for (char c : query) {
        if (mode == Mode::PHRASE) {
            switch (c) {
            /* case for terminal characters */
            case '"':
                tokens.push_back(curr);
                mode = Mode::DEFAULT;
                curr.clear();
                break;
            /* cases for non-terminal characters */
            case '>':  /* phrase ends */
            case '\\': /* backslashes */
                curr.push_back('\\');
            /* cases for non delimiting characters */
            default:
                curr.push_back(c);
                break;
            }

        }

        else if (mode == Mode::ESCAPED) {
            curr.push_back(c);
            mode = Mode::DEFAULT;
        }

        else /* mode == Mode::DEFAULT */ {
            switch (c) {
            case '"':

                curr.push_back(SYM_PHRASE);
                mode = Mode::PHRASE;
                break;

            case '(':
            case ')':
            case '&':
            case '|':
            case '-':

                if (!curr.empty()) {
                    tokens.push_back(curr);
                    curr.clear();
                }
                tokens.push_back(std::string(1, c));
                break;

            case ' ':

                if (!curr.empty()) {
                    tokens.push_back(curr);
                    curr.clear();
                }
                break;

            case '\\': /* backslashes */
                mode = Mode::ESCAPED;
                break;

            /* cases for non-operator characters */
            case '>':        /* phrase ends */
            case SYM_PHRASE: /* phrase starts */
                curr.push_back('\\');
            default:
                curr.push_back(c);
                break;
            }
        }
    }

    if (!curr.empty()) tokens.push_back(curr);

    return tokens;
}

// **Parses individual terms: words, NOT, or parentheses**
Expr* Query_Compiler::parse_term(std::vector<std::string>& tokens, int& index) {
    if (index >= tokens.size()) return nullptr;

    std::string token = tokens[index];

    if (token == "-") {   // NOT operator
        index++;
        if (index >= tokens.size()) return nullptr;   // prevents invalid - usage

        Expr* right = parse_term(tokens, index);
        if (!right) return nullptr;   // Prevents NOT from not having an operand

        return new Expr_NOT { right };
    } else if (token == "(") {
        index++;
        Expr* expr = parse_expression(tokens, index);

        // Ensure that closing ')' exists
        if (index >= tokens.size() || tokens[index] != ")") {
            return nullptr;   // if invalid expression (missing `)`)
        }

        index++;
        return expr;
    } else {   // A search term or phrase
        index++;
        return Expr_Leaf::generate_expr(token);
    }
}
// I dont know if this is right sob

// **Parses expressions w/ operator precedence**
Expr* Query_Compiler::parse_expression(std::vector<std::string>& tokens, int& index) {
    Expr* left = parse_term(tokens, index);

    while (index < tokens.size()) {
        std::string token = tokens[index];

        const char SYM = token[0];

        if (SYM == Protocol::AND) {
            ++index;
            Expr* right = parse_term(tokens, index);
            left = new Expr_AND { left, right };
        }

        else if (SYM == Protocol::OR) {
            ++index;
            Expr* right = parse_term(tokens, index);
            left = new Expr_OR { left, right };
        }

        else if (token != ")") {
            // Implicit AND
            Expr* right = parse_term(tokens, index);
            left = new Expr_AND { left, right };
        }

        else
            break;
    }

    return left;
}

Expr_Dummy* Query_Compiler::build_expr(const std::string& query) {
    std::string decoded_query = url_decode(query);

    // TODO: multithreading
    // TODO: not pass index by ref
    std::vector<std::string> tokens = tokenize(decoded_query);
    int index = 0;   // Track pos in tokens
    Expr* child = parse_expression(tokens, index);
    return new Expr_Dummy(child);
}

Query_Compiler::Query_Compiler(const std::vector<Query::CSolverInfo>& eps)
    : endpoints(eps) {}

Query_Compiler::~Query_Compiler() {
    delete instance;
    instance = nullptr;
}

void Query_Compiler::init_instance(const std::vector<CSolverInfo>& endpoints, const std::string& file_synsets) {
    assert(instance == nullptr);
    Synsets::init(file_synsets);
    instance = new Query_Compiler(endpoints);
}

Query_Compiler* Query_Compiler::get_instance() {
    assert(instance != nullptr);
    return instance;
}

static int connect_to_csolver(const CSolverInfo& ep) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ep.port);
    if (inet_pton(AF_INET, ep.ip.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(s);
        return -1;
    }
    if (connect(s, (sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("connect");
        close(s);
        return -1;
    }
    return s;
}

static void read_results_from_socket(int s, std::vector<SearchResult>& out) {
    uint32_t cnt_net = 0;
    recv_looping_crashing(s, reinterpret_cast<char*>(&cnt_net), sizeof(cnt_net));
    uint32_t cnt = ntohl(cnt_net);

    std::string cur;
    SearchResult tmp;
    int line = 0;
    char ch;
    const int lines_needed = cnt * 2;

    while (line < lines_needed && recv(s, &ch, 1, 0) > 0) {
        if (ch == '\n') {
            if (line % 2 == 0)
                tmp.url = cur;
            else
                tmp.title = cur;
            cur.clear();
            ++line;
            if (line % 2 == 0) { /* completed pair, now read 8‑byte score */
                uint64_t bits_net;
                recv_looping_crashing(s, reinterpret_cast<char*>(&bits_net), 8);
                uint64_t bits = ntohll(bits_net);
                std::memcpy(&tmp.score, &bits, sizeof(bits));
                out.push_back(tmp);
            }
        } else
            cur.push_back(ch);
    }
}
std::vector<SearchResult> mergeSortedArrays(const std::vector<std::vector<SearchResult>>& arrays) {
    int k = arrays.size();
    std::vector<int> indices(k, 0);   // track current position in each array
    std::vector<SearchResult> result;
    std::unordered_set<std::string> urls;

    while (true) {
        double minVal = std::numeric_limits<double>::max();
        int minArray = -1;

        // Find the smallest element among current heads
        for (int i = 0; i < k; ++i) {
            if (indices[i] < arrays[i].size()) {
                if (arrays[i][indices[i]].score < minVal) {
                    minVal = arrays[i][indices[i]].score;
                    minArray = i;
                }
            }
        }

        if (minArray == -1) break;   // all arrays are exhausted

        auto& search_result = arrays[minArray][indices[minArray]];

        if (urls.find(search_result.url) == urls.end()) {
            result.push_back(search_result);
            urls.insert(search_result.url);
        }
        indices[minArray]++;   // move forward in the chosen array
    }

    return result;
}

/* =============================================================
 *                     send_query – main API
 * ===========================================================*/
std::vector<SearchResult> Query_Compiler::send_query(const std::string& query) {
    Expr* root = build_expr(query);
    root = root->optimize();

    if (!root) {
        std::cerr << "Invalid query: " << query << std::endl;
        return std::vector<SearchResult> {};
    }

    // 1. broadcast query
    std::vector<int> socks;
    socks.reserve(endpoints.size());
    for (const auto& ep : endpoints) {
        int s = connect_to_csolver(ep);
        if (s != -1) {
            socks.push_back(s);
            root->serialize_and_send(s);
            send_looping_crashing(s, &Protocol::QUERY_END, 1);

#ifdef TESTING_SEND
            std::cout << Protocol::QUERY_END << std::endl;
#endif
        }
    }

#ifndef TESTING_SEND
    // 2. gather each socket's *already‑sorted* list
    std::vector<std::vector<SearchResult>> perSocket;
    perSocket.resize(socks.size());

    for (size_t i = 0; i < socks.size(); ++i) read_results_from_socket(socks[i], perSocket[i]);

    for (auto& i : socks) {
        close(i);
    }
#endif

    delete root;

#ifndef TESTING_SEND
    // 3. k‑way merge without std::sort
    return mergeSortedArrays(perSocket);
#endif

#ifdef TESTING_SEND
    return std::vector<SearchResult> {};
#endif
}


/* end Query_Compiler */
