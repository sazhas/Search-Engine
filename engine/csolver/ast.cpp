#include "ast.h"

#include <iostream>
#include <stdexcept>

#include <unistd.h>

#include <arpa/inet.h>

#include "isr.h"

// ---------- Helper: mapping operator char to enum ----------
Expr_AST::Operator Expr_AST::ch_to_op(char ch) {
    switch (ch) {
    case Query::Protocol::AND:
        return Operator::AND;
    case Query::Protocol::OR:
        return Operator::OR;
    case Query::Protocol::OR_SYN:
        return Operator::OR_SYN;
    case Query::Protocol::NOT:
        return Operator::NOT;
    case Query::Protocol::WORD_START:
        return Operator::WORD_START;
    case Query::Protocol::PHRASE_START:
        return Operator::PHRASE_START;
    case Query::Protocol::PHRASE_END:
        return Operator::PHRASE_END;
    default:
        std::cerr << "Unknown operator: " << ch << std::endl;
        throw std::runtime_error("Unknown operator");
        // assert(false);
    }
}

// ---------- Expr_Binary base class ----------
Expr_AST::Expr_Binary::Expr_Binary(Expr* left, Expr* right) : 
    left { left }, 
    right { right } 
{}

Expr_AST::Expr_Binary::~Expr_Binary() {
    if (left) delete left;
    if (right) delete right;
}

std::pair<ISR*, ISR*> Expr_AST::Expr_Binary::to_isr_pair(ISR_Tree* tree) const {
    return { 
        left ? left->to_ISR(tree) : nullptr,
        right ? right->to_ISR(tree) : nullptr 
    };
}

/*void Expr_AST::debug_print() const {
    std::function<void(Expr*, int)> print = [&](Expr* node, int depth) {
        if (!node) return;
        std::string indent(depth * 2, ' ');
        if (auto internal = dynamic_cast<Expr_Internal*>(node)) {
            std::cout << indent << "Op: " << static_cast<int>(internal->get_op()) << "\n";
            print(internal->get_left(), depth + 1);
            print(internal->get_right(), depth + 1);
        } else if (auto word = dynamic_cast<Expr_Leaf_Word*>(node)) {
            std::cout << indent << "Word: " << word->get_term() << "\n";
        } else if (auto phrase = dynamic_cast<Expr_Leaf_Phrase*>(node)) {
            std::cout << indent << "Phrase:";
            for (const auto& t : phrase->get_terms()) std::cout << " " << t;
            std::cout << "\n";
        } else {
            std::cout << indent << "Unknown node type\n";
        }
    };
    print(root, 0);
}*/

// start Expr_AND

Expr_AST::Expr_AND::Expr_AND(Expr* left, Expr* right) :
    Expr_Binary { left, right } 
{}

ISR* Expr_AST::Expr_AND::to_ISR(ISR_Tree* tree) const {
    auto [l, r] = to_isr_pair(tree);
    return new ISRAnd(tree, l, r);
}

// end Expr_AND

// start Expr_OR

Expr_AST::Expr_OR::Expr_OR(Expr* left, Expr* right) :
    Expr_Binary { left, right } 
{}

ISR* Expr_AST::Expr_OR::to_ISR(ISR_Tree* tree) const {
    auto [l, r] = to_isr_pair(tree);
    return new ISROr(tree, l, r);
}

// end Expr_OR

// start Expr_OR_SYN

Expr_AST::Expr_OR_SYN::Expr_OR_SYN(Expr* left, Expr_OR_SYN* right, int advance_right, int advance_left) :
    Expr_Binary { left, right },
    advance_right { advance_right },
    advance_left { advance_left }
{}

ISR* Expr_AST::Expr_OR_SYN::to_ISR(ISR_Tree* tree) const {
    auto [l, r] = to_isr_pair(tree);
    return new ISRSynOr(tree, l, r, advance_right, advance_left);
}

// end Expr_OR_SYN

// start Expr_NOT

Expr_AST::Expr_NOT::Expr_NOT(Expr* expr, Expr*) :
    Expr_Binary { expr, nullptr } 
{}

ISR* Expr_AST::Expr_NOT::to_ISR(ISR_Tree* tree) const {
    auto [l, r] = to_isr_pair(tree);
    // TODO: is this right?
    return new ISRContainer(tree, l, r);
}

// end Expr_NOT

// ---------- Leaf: Word ----------
Expr_AST::Expr_Leaf_Word::Expr_Leaf_Word(std::string&& term) : 
    term { std::move(term) } 
{}

ISR* Expr_AST::Expr_Leaf_Word::to_ISR(ISR_Tree* tree) const {
    return tree->get_ISRWord(term.c_str());
}

// ---------- Leaf: Phrase ----------
Expr_AST::Expr_Leaf_Phrase::Expr_Leaf_Phrase(std::vector<std::string>&& terms) :
    terms { std::move(terms) } 
{}

ISR* Expr_AST::Expr_Leaf_Phrase::to_ISR(ISR_Tree* tree) const {
    return new ISRPhrase(tree, terms);
}

// ---------- Template helper function ----------
template <typename Aggregate, typename Ret>
Ret Expr_AST::read_to_cond() const {
    Aggregate aggr;
    char window[2] = { 0, 0 };
    recv_looping_crashing(sock, window, 1);

    bool exit = false;
    while (!exit) {
        // Peek the next character without consuming it
        recv(sock, window + 1, 1, MSG_PEEK);
        switch (window[0]) {
        case '\\':
            recv_looping_crashing(sock, window + 1, 1);
            aggr.push_back(window[1]);
            recv_looping_crashing(sock, window, 1);
            break;
        case Query::Protocol::PHRASE_END:
            exit = true;
            break;
        default:
            recv_looping_crashing(sock, window + 1, 1);
            aggr.push_back(window[0]);
            window[0] = window[1];
            break;
        }
    }
    return aggr.ret();
}

// Explicit instantiations if necessary (or place the definitions in the header)

uint32_t Expr_AST::read_step() const {
    uint32_t step;
    char* window = (char*)&step;

    recv_looping_crashing(sock, window, sizeof(step));

    char delim;

    recv_looping_crashing(sock, &delim, 1);

    if (delim != Query::Protocol::STEP_DELIM) {
        std::cerr << "Error: expected QUERY_DELIM marker ('|'), got '" << delim << "' instead.\n";
        throw std::runtime_error("Invalid delimiter");
    }

    return ntohl(step);
}

// ---------- read_to_word_end and read_to_phrase_end ----------
std::string Expr_AST::read_to_word_end() const {
    struct Aggregate_Word {
        std::string term;
        Aggregate_Word() { term.reserve(512); }
        void push_back(char ch) { term.push_back(ch); }
        std::string ret() { return std::move(term); }
    };

    return read_to_cond<Aggregate_Word, std::string>();
}

std::vector<std::string> Expr_AST::read_to_phrase_end() const {
    struct Aggregate_Phrase {
        std::vector<std::string> terms;
        Aggregate_Phrase()
            : terms(1, std::string {}) {
            terms.back().reserve(512);
        }
        void push_back(char ch) {
            if (ch == ' ') {
                if (!terms.back().empty()) terms.push_back(std::string {});
            } else {
                terms.back().push_back(ch);
            }
        }
        std::vector<std::string> ret() { return std::move(terms); }
    };

    return read_to_cond<Aggregate_Phrase, std::vector<std::string>>();
}

// ---------- Build the AST recursively ----------
Expr_AST::Expr* Expr_AST::build() {
    char char_op;
    recv_looping_crashing(sock, &char_op, 1);
    Operator op = ch_to_op(char_op);
    Expr* ret = nullptr;

    switch (op) {
    case Operator::AND:
        {
            Expr* left = build();
            Expr* right = build();
            ret = new Expr_AND(left, right);
        }
        break;
    case Operator::OR:
    {
        Expr* left = build();
        Expr* right = build();
        ret = new Expr_OR(left, right);
    }
        break;
    case Operator::OR_SYN:
        {
            Expr* left = build();
            Expr_OR_SYN* right = static_cast<Expr_OR_SYN*>(build());
            uint32_t advance_right = read_step();
            uint32_t advance_left = read_step();

            if(!(left || right))
                throw std::runtime_error("Invalid OR_SYN expression: both left and right are null");
            else if(!left)
                ret = right;
            else if(!right)
                ret = left;
            else {
                ret = new Expr_OR_SYN(left, right, advance_right, advance_left);
            }
        }
        break;
    case Operator::NOT:
    {
        Expr* left = build();
        Expr* discard = build();
        ret = new Expr_NOT(left, discard);
    }
        break;
    case Operator::WORD_START:
        ret = new Expr_Leaf_Word(read_to_word_end());
        break;
    case Operator::PHRASE_START:
        ret = new Expr_Leaf_Phrase(read_to_phrase_end());
        break;
    default /* Operator::PHRASE_END */:
        ret = nullptr;
        break;
    }
    return ret;
}

// ---------- Expr_AST public methods ----------
Expr_AST::Expr_AST(int sock)
    : sock(sock)
    , root(nullptr) {
    root = build();

    char end_marker;
    recv_looping_crashing(sock, &end_marker, 1);
    if (end_marker != Query::Protocol::QUERY_END) {
        std::cerr << "Error: expected QUERY_END marker ('#'), got '" << end_marker << "' instead.\n";
        throw std::runtime_error("Unterminated query");
    }
}

Expr_AST::Expr_AST(Expr_AST&& other)
    : sock(other.sock)
    , root(other.root) {
    other.root = nullptr;
}

Expr_AST::~Expr_AST() {
    if (root) delete root;
}

ISR* Expr_AST::to_ISR(ISR_Tree* tree) const {
    return (root) ? root->to_ISR(tree) : nullptr;
}
