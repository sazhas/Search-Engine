#ifndef ISR_H
#define ISR_H

#include <string>
#include <vector>

#include "../indexer/Indexer.hpp"
#include "../indexer/Posts.hpp"
#include "../query/query.h"
#include "../ranker/Ranker.hpp"
#include "csolver.h"

// Forward declarations
class ISR_Tree;
class Expr_AST;

class ISR_Highlevel : public ISR {
protected:
    ISR_Tree* tree;

    ISR_Highlevel(ISR_Tree* tree_);

    ISRWord* get_ISRWord(const std::string& str) const;
    ISRDoc* get_ISREndDoc() const;

public:
};

class ISR_Binary : public ISR_Highlevel {
protected:
    ISR* isr1;
    ISR* isr2;

    ISR_Binary(ISR_Tree* tree, ISR* isr1, ISR* isr2);

public:
};

class ISROr : public ISR_Binary {
public:
    ISROr(ISR_Tree* tree, ISR* isr1, ISR* isr2);
    ~ISROr() override;

    uint32_t GetPostCount() override;
    Post* Next() override;
    Post* NextInternal() override;
    Post* Seek(Location target) override;
    Location GetStartLocation() override;
    Location GetEndLocation() override;
    Post* GetCurrentPost() override;
    Post* GetCurrentDoc() override {
        if (nearestTerm == -1) return nullptr;
        return (nearestTerm == 0) ? isr1->GetCurrentDoc() : isr2->GetCurrentDoc();
    }

    void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                      std::unordered_set<std::string>& terms_set) override;

private:
    int nearestTerm;
    Location nearestStartLocation;
    Location nearestEndLocation;

    Post* FindNearest();
};

class ISRSynOr : public ISR_Binary {
public:
    // advanceRightSteps = how many times to call Next()/NextInternal() on the right when left wins
    // advanceLeftSteps  = how many times to call Next()/NextInternal() on the left when right wins
    ISRSynOr(ISR_Tree* tree, ISR* isr1, ISR* isr2, int advanceRightSteps, int advanceLeftSteps);
    ~ISRSynOr() override;

    uint32_t GetPostCount() override;
    Post* Next() override;
    Post* NextInternal() override;
    Post* Seek(Location target) override;
    Location GetStartLocation() override;
    Location GetEndLocation() override;
    Post* GetCurrentPost() override;
    Post* GetCurrentDoc() override;
    bool isSynonym() override;

    void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                      std::unordered_set<std::string>& terms_set) override;

private:
    int nearestTerm;
    Location nearestStartLocation;
    Location nearestEndLocation;

    int advanceRight;
    int advanceLeft;

    Post* FindNearest();
};


class ISRAnd : public ISR_Binary {
public:
    ISRAnd(ISR_Tree* tree, ISR* isr1, ISR* isr2);
    ~ISRAnd() override;

    uint32_t GetPostCount() override;
    Post* Next() override;
    Post* NextInternal() override;
    Post* Seek(Location target) override;
    Location GetStartLocation() override;
    Location GetEndLocation() override;
    Post* GetCurrentPost() override;
    Post* GetCurrentDoc() override {
        if (current) {
            return isr1->GetCurrentDoc();
        }
        return nullptr;
    }

    void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                      std::unordered_set<std::string>& terms_set) override;

private:
    Post* current;
    int nearestTerm = -1;
    Post* AdvanceToMatch();
};

class ISRContainer : public ISR_Binary {
public:
    ISRContainer(ISR_Tree* tree, ISR* included, ISR* excluded);
    ~ISRContainer() override;
    uint32_t GetPostCount() override;
    Post* Next() override;
    Post* NextInternal() override;
    Post* Seek(Location target) override;
    Location GetStartLocation() override;
    Location GetEndLocation() override;
    Post* GetCurrentPost() override;
    Post* GetCurrentDoc() override;

    void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                      std::unordered_set<std::string>& terms_set) override;

private:
    Post* current;
    Post* AdvanceToMatch();
};

class ISRPhrase : public ISR_Highlevel {
public:
    ISRPhrase(ISR_Tree* tree, const std::vector<std::string>& terms);
    ~ISRPhrase() override {}

    Post* Next() override;
    Post* NextInternal() override;
    Post* GetCurrentDoc() override;
    Post* Seek(Location target) override;
    Location GetStartLocation() override;
    Location GetEndLocation() override;
    Post* GetCurrentPost() override;
    uint32_t GetPostCount() override;

    void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                      std::unordered_set<std::string>& terms_set) override;

private:
    std::vector<ISR*> terms;
    Post* current;

    std::vector<ISR*> strings_to_isrs(const std::vector<std::string>& terms) const;
    Post* AdvanceToMatch();
};

class ISR_Tree {
private:
    IndexBlob* blob;
    ISR* root;
    Post* current;

public:
    ISR_Tree(IndexBlob* blob, Expr_AST* root);
    ~ISR_Tree();

    inline ISRWord* get_ISRWord(const char* str) const { return blob->OpenISRWord(str); }
    inline ISRDoc* get_ISREndDoc() const { return blob->OpenISREndDoc(); }

    inline ISR* get_root() const { return root; }

    std::vector<ISRWord*> getFlattenedTerms() const;
};

#endif /* ISR_H */