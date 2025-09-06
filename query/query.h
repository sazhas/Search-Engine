#ifndef QUERY_H
#define QUERY_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <stack>
#include <string>
#include <vector>

#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "../lib/thread.h"
#include "protocol_query.h"

namespace Query {


struct SearchResult {
    std::string url;
    std::string title;
    double score;
};

struct CSolverInfo {
    std::string ip;
    uint16_t port;

    CSolverInfo() = default;                       // keep default
    CSolverInfo(std::string&& ip_, uint16_t port_)   // new ctor
        : ip(std::move(ip_))
        , port(port_) {}
};


class Expr_Parent;


class Expr {
public:
    virtual Expr* optimize() = 0;
    virtual void serialize_and_send(int sock) const = 0;
};


class Expr_Parent : public Expr {
public:
};


class Expr_OR;
class Expr_OR_SYN;


class Expr_Leaf : public Expr {
protected:

    virtual Expr_OR* generate_decorated() const = 0;

public:

    static Expr_Leaf* generate_expr(const std::string& str);

}; /* class Expr_Leaf */


class Expr_Leaf_Word : public Expr_Leaf {

    friend class Expr_Leaf;

private:

    std::string term;
    std::string stem;

    Expr_Leaf_Word(const std::string& term);
    Expr_Leaf_Word(std::string&& term);

    /* for use in synonym generation, where  the stem is already known */
    Expr_Leaf_Word(const std::string& term, std::string&& stem);

    Expr_OR* generate_decorated() const override;

    static Expr_OR_SYN* helper_generate_synonyms(std::stack<Expr_Leaf_Word*>& children);
    /**
     * @brief generates a synonym tree that incorporates the original term in memory
     */
    Expr_OR_SYN* generate_synonyms();

public:

    Expr* optimize() override;

    void serialize_and_send(int sock) const override;

};


class Expr_Leaf_Phrase : public Expr_Leaf {

    friend class Expr_Leaf;

private:

    std::vector<std::string> terms;
    std::vector<std::string> stems;

    Expr_Leaf_Phrase(std::vector<std::string>&& _terms);

    /* for use in decoration, where the stems are already known */
    Expr_Leaf_Phrase(std::vector<std::string>&& _terms, std::vector<std::string>&& _stems);

    Expr_OR* generate_decorated() const override;

public:

    Expr* optimize() override;

    void serialize_and_send(int sock) const override;

};


class Query_Compiler;

class Expr_Dummy : public Expr_Parent {
    friend class Query_Compiler;

private:
    Expr* child;

    Expr_Dummy(Expr* child);

public:

    Expr* optimize() override;

    void serialize_and_send(int sock) const override;

}; /* class Expr_Dummy */


class Expr_Op : public Expr_Parent {
protected:
    Expr* left;
    Expr* right;

    const char SYM;

public:
    Expr_Op(Expr* left, Expr* right, char SYM);
    virtual ~Expr_Op();

    /**
     * @brief serializes the expression
     * @cond sock must be initialized
     */
    virtual void serialize_and_send(int sock) const override;

    virtual Expr* optimize() override;

    virtual void reassign_child(Expr* child, Expr* sub);

}; /* class Expr */


class Expr_AND : public Expr_Op {
public:
    Expr_AND(Expr* left, Expr* right);

    Expr* optimize() override;
};


class Expr_OR : public Expr_Op {
public:
    Expr_OR(Expr* left, Expr* right);

    Expr* optimize() override;
};


class Expr_OR_SYN : public Expr_Op {
private:
    uint32_t size;
    uint32_t ratio_term, ratio_rest;

public:
    Expr_OR_SYN(Expr* term, Expr_OR_SYN* rest, uint32_t ratio_term, uint32_t ratio_rest);

    Expr_OR_SYN(Expr* term, Expr_OR_SYN* rest);

    Expr* optimize() override;

    void serialize_and_send(int sock) const override;
};


class Expr_NOT : public Expr_Op {
public:
    Expr_NOT(Expr* term);

    Expr* optimize() override;
};


class Query_Compiler {
private:
    std::vector<Query::CSolverInfo> endpoints;
    /**
     * @brief parses an individual term (word, NOT, or parentheses)
     */
    Expr* parse_term(std::vector<std::string>& tokens, int& index);

    /**
     * @brief recursively parses expressions based on operator precedence??/ unsure
     */
    Expr* parse_expression(std::vector<std::string>& tokens, int& index);

    /**
     * @brief builds syntax tree
     */
    Expr_Dummy* build_expr(const std::string& query);

    Query_Compiler(const std::vector<Query::CSolverInfo>& endpoints);

    inline static Query_Compiler* instance = nullptr;

public:
    ~Query_Compiler();

    static void init_instance(const std::vector<CSolverInfo>& endpoints, const std::string& file_synsets);

    /**
     * @brief singleton instance
     */
    static Query_Compiler* get_instance();

    std::vector<SearchResult> send_query(const std::string& query);

}; /* class Query_Compiler */


}; /* namespace Query */

#endif /* QUERY_H */
