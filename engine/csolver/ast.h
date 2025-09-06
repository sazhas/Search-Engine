#ifndef AST_H
#define AST_H

#include <cassert>
#include <string>
#include <vector>
#include <utility>

#include <sys/socket.h>   // for recv

#include "../lib/networking.h"   // for recv_looping_crashing
#include "../query/protocol_query.h"
#include "isr.h"   // for ISR and ISR_Tree

class ISR_Tree;

class Expr_AST {
private:
    enum class Operator { AND, OR, OR_SYN, NOT, WORD_START, PHRASE_START, PHRASE_END };

    static Operator ch_to_op(char ch);

    class Expr {
    public:
        virtual ISR* to_ISR(ISR_Tree* tree) const = 0;
    };

    // Base class for AST nodes.
    class Expr_Binary : public Expr {
    protected:
        Expr* left;
        Expr* right;

        std::pair<ISR*, ISR*> to_isr_pair(ISR_Tree* tree) const;

    public:
        Expr_Binary(Expr* left, Expr* right);
        virtual ~Expr_Binary();
        virtual ISR* to_ISR(ISR_Tree* tree) const = 0;

        inline Expr* get_left() const { return left; }
        inline Expr* get_right() const { return right; }
    };

    // Internal node (operators AND, OR, NOT).
    class Expr_AND : public Expr_Binary {
    public:
        Expr_AND(Expr* left, Expr* right);
        ISR* to_ISR(ISR_Tree* tree) const override;
    };

    class Expr_OR : public Expr_Binary {
    public:
        Expr_OR(Expr* left, Expr* right);
        ISR* to_ISR(ISR_Tree* tree) const override;
    };

    class Expr_OR_SYN : public Expr_Binary {
    private:
        int advance_right;
        int advance_left;   
    public:
        Expr_OR_SYN(Expr* left, Expr_OR_SYN* right, int advance_right, int advance_left);
        ISR* to_ISR(ISR_Tree* tree) const override;
    };

    class Expr_NOT : public Expr_Binary {
    public:
        Expr_NOT(Expr* expr, Expr*);
        ISR* to_ISR(ISR_Tree* tree) const override;
    };

    // Leaf node for single word searches.
    class Expr_Leaf_Word : public Expr {
    private:
        std::string term;

    public:
        Expr_Leaf_Word(std::string&& term);
        ISR* to_ISR(ISR_Tree* tree) const override;

        std::string get_term() const { return term; }
    };

    // Leaf node for phrase searches (list of words).
    class Expr_Leaf_Phrase : public Expr {
    private:
        std::vector<std::string> terms;

    public:
        Expr_Leaf_Phrase(std::vector<std::string>&& terms);
        ISR* to_ISR(ISR_Tree* tree) const override;

        std::vector<std::string> get_terms() const { return terms; }
    };

    int sock;
    Expr* root;

    uint32_t read_step() const;

    // Template function used by read_to_word_end and read_to_phrase_end.
    template <typename Aggregate, typename Ret>
    Ret read_to_cond() const;

    std::string read_to_word_end() const;
    std::vector<std::string> read_to_phrase_end() const;
    Expr* build();

public:
    explicit Expr_AST(int sock);
    // Move constructor. We simply transfer ownership of root.
    Expr_AST(Expr_AST&& other);
    ~Expr_AST();

    ISR* to_ISR(ISR_Tree* tree) const;

    // void debug_print() const;
};

#endif /* AST_H */
