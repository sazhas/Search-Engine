#include "isr.h"

#include <unordered_set>

ISR_Highlevel::ISR_Highlevel(ISR_Tree* tree_)
    : tree { tree_ } {}

// Inline definitions.
inline ISRWord* ISR_Highlevel::get_ISRWord(const std::string& str) const {
    return tree->get_ISRWord(str.c_str());
}

inline ISRDoc* ISR_Highlevel::get_ISREndDoc() const {
    return tree->get_ISREndDoc();
}

ISR_Binary::ISR_Binary(ISR_Tree* tree, ISR* isr1, ISR* isr2)
    : ISR_Highlevel { tree }
    , isr1 { isr1 }
    , isr2 { isr2 } {}

/* begin ISROr */

ISROr::ISROr(ISR_Tree* tree, ISR* isr1, ISR* isr2)
    : ISR_Binary { tree, isr1, isr2 }
    , nearestTerm(-1)
    , nearestStartLocation(0)
    , nearestEndLocation(0) {
    assert(isr1 && isr2);
}

ISROr::~ISROr() {
    delete isr1;
    delete isr2;
}

uint32_t ISROr::GetPostCount() {
    // In the case where no words were found

    // Estimate
    return isr1->GetPostCount() + isr2->GetPostCount();
}

Post* ISROr::NextInternal() {
    if (nearestTerm == -1) {
        // Initialize if this is our first call
        isr1->NextInternal();
        isr2->NextInternal();
        return FindNearest();
    }
    if (nearestTerm == 0)
        isr1->NextInternal();
    else if (nearestTerm == 1)
        isr2->NextInternal();
    return FindNearest();
}

Post* ISROr::Next() {
    if (nearestTerm == -1) {
        // Initialize if this is our first call
        isr1->Next();
        isr2->Next();
        return FindNearest();
    }
    Post* doc;
    if (nearestTerm == 0)
        doc = isr1->GetCurrentDoc();
    else if (nearestTerm == 1)
        doc = isr2->GetCurrentDoc();
    if (!doc) {
        return nullptr;
    }
    return Seek(doc->GetEndLocation() + 1);
}

Post* ISROr::Seek(Location target) {
    if (current && GetStartLocation() >= target) {
        return current;
    }
    if (isr1) isr1->Seek(target);
    if (isr2) isr2->Seek(target);
    return FindNearest();
}

Location ISROr::GetStartLocation() {
    return nearestStartLocation;
}

Location ISROr::GetEndLocation() {
    return nearestEndLocation;
}

Post* ISROr::GetCurrentPost() {
    if (nearestTerm == -1) return nullptr;
    return (nearestTerm == 0) ? isr1->GetCurrentPost() : isr2->GetCurrentPost();
}

Post* ISROr::FindNearest() {
    Post* post1 = isr1->GetCurrentPost();
    Post* post2 = isr2->GetCurrentPost();

    if (!post1 && !post2) {
        nearestTerm = -1;
        return nullptr;
    } else if (post1 && (!post2 || post1->GetStartLocation() <= post2->GetStartLocation())) {
        nearestTerm = 0;
        nearestStartLocation = post1->GetStartLocation();
        nearestEndLocation = post1->GetEndLocation();
        return post1;
    } else {
        nearestTerm = 1;
        nearestStartLocation = post2->GetStartLocation();
        nearestEndLocation = post2->GetEndLocation();
        return post2;
    }
}

void ISROr::collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms, std::unordered_set<std::string>& terms_set) {
    if (isr1) {
        if (auto word = dynamic_cast<ISRWord*>(isr1)) {
            word->collectTerms(index, terms, terms_set);
        } else if (auto highlevel = dynamic_cast<ISR_Highlevel*>(isr1)) {
            highlevel->collectTerms(index, terms, terms_set);
        }
    }
    if (isr2) {
        if (auto word = dynamic_cast<ISRWord*>(isr2)) {
            word->collectTerms(index, terms, terms_set);
        } else if (auto highlevel = dynamic_cast<ISR_Highlevel*>(isr2)) {
            highlevel->collectTerms(index, terms, terms_set);
        }
    }
}

/* end ISROr */

/* begin ISRSynOr */

ISRSynOr::ISRSynOr(ISR_Tree* tree, ISR* a, ISR* b, int advanceRightSteps, int advanceLeftSteps)
    : ISR_Binary { tree, a, b }
    , nearestTerm(-1)
    , nearestStartLocation(0)
    , nearestEndLocation(0)
    , advanceRight(advanceRightSteps)
    , advanceLeft(advanceLeftSteps) {
    assert(isr1 && isr2);
}

ISRSynOr::~ISRSynOr() {
    delete isr1;
    delete isr2;
}

uint32_t ISRSynOr::GetPostCount() {
    return isr1->GetPostCount() + isr2->GetPostCount();
}

Post* ISRSynOr::NextInternal() {
    if (nearestTerm == -1) {
        isr1->NextInternal();
        isr2->NextInternal();
        return FindNearest();
    }

    if (nearestTerm == 0) {
        for (int i = 0; i < advanceLeft; ++i) isr1->NextInternal();
    } else {
        for (int i = 0; i < advanceRight; ++i) isr2->NextInternal();
    }
    return FindNearest();
}

Post* ISRSynOr::Next() {
    if (nearestTerm == -1) {
        isr1->Next();
        isr2->Next();
        return FindNearest();
    }
    Post* doc = GetCurrentDoc();
    if (!doc) return nullptr;
    Seek(doc->GetEndLocation() + 1);

    if (nearestTerm == 0) {
        for (int i = 0; i < advanceRight - 1; ++i) isr2->Next();
    } else {
        for (int i = 0; i < advanceLeft - 1; ++i) isr1->Next();
    }

    return FindNearest();
}

Post* ISRSynOr::Seek(Location target) {
    if (current && GetStartLocation() >= target) {
        return current;
    }
    if (isr1) isr1->Seek(target);
    if (isr2) isr2->Seek(target);
    return FindNearest();
}

Location ISRSynOr::GetStartLocation() {
    return nearestStartLocation;
}

Location ISRSynOr::GetEndLocation() {
    return nearestEndLocation;
}

Post* ISRSynOr::GetCurrentPost() {
    if (nearestTerm == -1) return nullptr;
    return (nearestTerm == 0) ? isr1->GetCurrentPost() : isr2->GetCurrentPost();
}

Post* ISRSynOr::GetCurrentDoc() {
    if (nearestTerm == -1) return nullptr;
    return (nearestTerm == 0) ? isr1->GetCurrentDoc() : isr2->GetCurrentDoc();
}

bool ISRSynOr::isSynonym() {
    if (nearestTerm == 0) return false;
    return true;
}

Post* ISRSynOr::FindNearest() {
    Post* post1 = isr1->GetCurrentPost();
    Post* post2 = isr2->GetCurrentPost();

    if (!post1 && !post2) {
        nearestTerm = -1;
        return nullptr;
    } else if (post1 && (!post2 || post1->GetStartLocation() <= post2->GetStartLocation())) {
        nearestTerm = 0;
        nearestStartLocation = post1->GetStartLocation();
        nearestEndLocation = post1->GetEndLocation();
        return post1;
    } else {
        nearestTerm = 1;
        nearestStartLocation = post2->GetStartLocation();
        nearestEndLocation = post2->GetEndLocation();
        return post2;
    }
}

void ISRSynOr::collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                            std::unordered_set<std::string>& terms_set) {
    if (isr1) {
        if (auto w = dynamic_cast<ISRWord*>(isr1))
            w->collectTerms(index, terms, terms_set);
        else if (auto hl = dynamic_cast<ISR_Highlevel*>(isr1))
            hl->collectTerms(index, terms, terms_set);
    }
    size_t syn_index = terms.size();
    if (isr2) {
        if (auto w = dynamic_cast<ISRWord*>(isr2))
            w->collectTerms(index, terms, terms_set);
        else if (auto hl = dynamic_cast<ISR_Highlevel*>(isr2))
            hl->collectTerms(index, terms, terms_set);
    }
    for (size_t i = syn_index; i < terms.size(); i++) {
        terms[i]->setSynonym(true);
    }
}

/* end ISRSynOr */

/* begin ISRAnd */

ISRAnd::ISRAnd(ISR_Tree* tree, ISR* isr1, ISR* isr2)
    : ISR_Binary { tree, isr1, isr2 }
    , current { nullptr } {
    assert(isr1 && isr2);
}

ISRAnd::~ISRAnd() {
    delete isr1;
    delete isr2;
}

uint32_t ISRAnd::GetPostCount() {
    if (!isr1 || !isr2) return 0;
    return isr1->GetPostCount();
}

Post* ISRAnd::Next() {
    if (!isr1 || !isr2) return nullptr;

    // If we haven't initialized yet
    if (!current) {
        isr1->Next();
        isr2->Next();
        return AdvanceToMatch();
    }

    isr1->Next();
    isr2->Next();
    return AdvanceToMatch();
}

Post* ISRAnd::NextInternal() {
    if (!isr1 || !isr2) return nullptr;

    // If we haven't initialized yet
    if (!current) {
        isr1->NextInternal();
        isr2->NextInternal();
        return AdvanceToMatch();
    }

    if (nearestTerm == 0) {
        isr1->NextInternal();
    } else {
        isr2->NextInternal();
    }
    return AdvanceToMatch();
}

Post* ISRAnd::Seek(Location target) {
    if (!isr1 || !isr2) return nullptr;
    if (current && GetStartLocation() >= target) {
        return current;
    }

    isr1->Seek(target);
    isr2->Seek(target);
    return AdvanceToMatch();
}

Location ISRAnd::GetStartLocation() {
    return current ? current->GetStartLocation() : 0;
}

Location ISRAnd::GetEndLocation() {
    return current ? current->GetEndLocation() : 0;
}

Post* ISRAnd::GetCurrentPost() {
    return current;
}

Post* ISRAnd::AdvanceToMatch() {
    while (true) {
        // One of the child ISRs are null
        if (!isr1 || !isr2) {
            nearestTerm = -1;
            return (current = nullptr);
        }

        Post* l = isr1->GetCurrentPost();
        Post* r = isr2->GetCurrentPost();

        if (!l || !r) {
            nearestTerm = -1;
            current = nullptr;
            return nullptr;
        }

        Location lStart = l->GetStartLocation();
        Location rStart = r->GetStartLocation();

        // Check if left and right are in same document
        if (lStart <= rStart) {
            auto docEnd = isr2->GetCurrentDoc();
            if (!docEnd) {
                return nullptr;   // Early return if doc is null
            }
            if (lStart >= docEnd->GetStartLocation() && rStart <= docEnd->GetEndLocation()
                && lStart <= docEnd->GetEndLocation()) {
                nearestTerm = 0;
                current = l;
                return l;
            }
            isr1->Seek(docEnd->GetStartLocation());
        } else {
            auto docEnd = isr1->GetCurrentDoc();
            if (!docEnd) {
                return nullptr;   // Early return if doc is null
            }
            if (rStart >= docEnd->GetStartLocation() && rStart <= docEnd->GetEndLocation()
                && lStart <= docEnd->GetEndLocation()) {
                nearestTerm = 1;
                current = r;
                return r;
            }
            isr2->Seek(docEnd->GetStartLocation());
        }
    }
}

void ISRAnd::collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms, std::unordered_set<std::string>& terms_set) {
    if (isr1) {
        if (auto word = dynamic_cast<ISRWord*>(isr1)) {
            word->collectTerms(index, terms, terms_set);
        } else if (auto highlevel = dynamic_cast<ISR_Highlevel*>(isr1)) {
            highlevel->collectTerms(index, terms, terms_set);
        }
    }
    if (isr2) {
        if (auto word = dynamic_cast<ISRWord*>(isr2)) {
            word->collectTerms(index, terms, terms_set);
        } else if (auto highlevel = dynamic_cast<ISR_Highlevel*>(isr2)) {
            highlevel->collectTerms(index, terms, terms_set);
        }
    }
}

/* end ISRAnd */

/* begin ISRContainer */
ISRContainer::ISRContainer(ISR_Tree* tree, ISR* included, ISR* excluded)
    : ISR_Binary(tree, included, excluded)
    , current(nullptr) {
    // Optionally, ensure that neither pointer is null.
    assert(included);
    assert(excluded);
}

ISRContainer::~ISRContainer() {
    delete isr1;
    delete isr2;
}

uint32_t ISRContainer::GetPostCount() {
    // For a NOT operator, you might decide to return 0 or delegate to isr1.
    return isr1 ? isr1->GetPostCount() : 0;
}

Post* ISRContainer::AdvanceToMatch() {
    while (true) {
        Post* includedPost = isr1->GetCurrentPost();
        if (!includedPost) {
            current = nullptr;
            return nullptr;
        }
        // Seek the excluded ISR to the same target as the included post.
        Post* docEnd = isr1->GetCurrentDoc();
        if (!docEnd) {
            return nullptr;
        }
        isr2->Seek(docEnd->GetStartLocation());
        Post* excludedPost = isr2->GetCurrentPost();
        // If excluded post exists and its start location falls inside the document of the included post,
        // then skip the included post.
        if (excludedPost && excludedPost->GetStartLocation() >= docEnd->GetStartLocation()
            && excludedPost->GetStartLocation() < docEnd->GetEndLocation()) {
            isr1->Next();   // Skip current included post.
            continue;
        }
        // Otherwise, current is a valid match.
        current = includedPost;
        return current;
    }
}

Post* ISRContainer::Next() {
    if (!isr1 || !isr2) return nullptr;
    if (!current) {
        isr1->Next();
        isr2->Next();
        return AdvanceToMatch();
    }
    // Advance the included stream.
    isr1->Next();
    return AdvanceToMatch();
}

Post* ISRContainer::NextInternal() {
    if (!isr1 || !isr2) return nullptr;
    if (!current) {
        isr1->NextInternal();
        isr2->NextInternal();
        return AdvanceToMatch();
    }
    isr1->NextInternal();
    return AdvanceToMatch();
}

Post* ISRContainer::Seek(Location target) {
    if (current && GetStartLocation() >= target) {
        return current;
    }
    if (!isr1 || !isr2) return nullptr;
    isr1->Seek(target);
    return AdvanceToMatch();
}

Location ISRContainer::GetStartLocation() {
    return current ? current->GetStartLocation() : 0;
}

Location ISRContainer::GetEndLocation() {
    return current ? current->GetEndLocation() : 0;
}

Post* ISRContainer::GetCurrentPost() {
    return current;
}

Post* ISRContainer::GetCurrentDoc() {
    return isr1 ? isr1->GetCurrentDoc() : nullptr;
}

void ISRContainer::collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                                std::unordered_set<std::string>& terms_set) {
    // Only collect from included terms (isr1)
    if (isr1) {
        if (auto word = dynamic_cast<ISRWord*>(isr1)) {
            word->collectTerms(index, terms, terms_set);
        } else if (auto highlevel = dynamic_cast<ISR_Highlevel*>(isr1)) {
            highlevel->collectTerms(index, terms, terms_set);
        }
    }
}

/* end ISRContainer */

/* begin ISRPhrase */

ISRPhrase::ISRPhrase(ISR_Tree* tree, const std::vector<std::string>& terms_)
    : ISR_Highlevel { tree }
    , terms { strings_to_isrs(terms_) }
    , current { nullptr } {}

Post* ISRPhrase::NextInternal() {
    if (terms.empty()) return nullptr;

    // If we haven't initialized yet
    if (!current) {
        for (auto& term : terms) {
            term->NextInternal();
        }
        return AdvanceToMatch();
    }

    return Seek(current->GetStartLocation() + 1);
}

Post* ISRPhrase::Next() {
    if (terms.empty()) return nullptr;

    // If we haven't initialized yet
    if (!current) {
        for (auto& term : terms) {
            term->Next();
        }
        return AdvanceToMatch();
    }
    auto doc = terms[0]->GetCurrentDoc();
    if (!doc) {
        return nullptr;
    }
    return Seek(doc->GetEndLocation() + 1);
}

Post* ISRPhrase::GetCurrentDoc() {
    if (terms.empty()) return nullptr;
    if (current) {
        return terms[0]->GetCurrentDoc();
    }
    return nullptr;
}

// break into ISR word
// do scanner protocol:
// where Li is location of ISWi
// advance iSW1 to L1
// advance ISW2 to L2; if L2 =/ L1 + 1 then restart
// advance ISW i to Li; if Li =/ Li - 1 then restart
// advance ISWn to Ln; if Ln =/ Ln-1 then restart
// if complete, then return L1

Post* ISRPhrase::AdvanceToMatch() {
    while (true) {
        Post* first = terms[0]->GetCurrentPost();
        if (!first) return current = nullptr;

        Location base = first->GetStartLocation();
        bool match = true;

        // try aligning each term at consecutive positions
        for (size_t i = 1; i < terms.size(); ++i) {
            Location expected = base + i;
            terms[i]->Seek(expected);

            Post* post = terms[i]->GetCurrentPost();
            if (!post || post->GetStartLocation() != expected) {
                // restart search from next position of ISR1
                terms[0]->Seek(base + 1);
                match = false;
                break;
            }
        }

        if (match) {
            current = terms[0]->GetCurrentPost();
            return current;
        }
    }
}

Post* ISRPhrase::Seek(Location target) {
    if (current && GetStartLocation() >= target) {
        return current;
    }
    if (terms.empty()) return current = nullptr;

    for (ISR* t : terms) {
        if (!t) return current = nullptr;
    }

    // start scanning from the first ISR
    terms[0]->Seek(target);

    return AdvanceToMatch();
}

Location ISRPhrase::GetStartLocation() {
    return current ? current->GetStartLocation() : 0;
}

Location ISRPhrase::GetEndLocation() {
    return current ? current->GetEndLocation() : 0;
}

Post* ISRPhrase::GetCurrentPost() {
    return current;
}

uint32_t ISRPhrase::GetPostCount() {
    return terms.empty() ? 0 : terms.front()->GetPostCount();
}

std::vector<ISR*> ISRPhrase::strings_to_isrs(const std::vector<std::string>& terms) const {
    std::vector<ISR*> isrs;
    isrs.reserve(terms.size());

    for (const auto& term : terms) isrs.push_back(get_ISRWord(term));

    return isrs;
}

void ISRPhrase::collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                             std::unordered_set<std::string>& terms_set) {
    for (auto term : this->terms) {
        if (term) {
            if (auto word = dynamic_cast<ISRWord*>(term)) {
                word->collectTerms(index, terms, terms_set);
            } else if (auto highlevel = dynamic_cast<ISR_Highlevel*>(term)) {
                highlevel->collectTerms(index, terms, terms_set);
            }
        }
    }
}

/* end ISRPhrase */

/* begin ISR_Tree */

ISR_Tree::ISR_Tree(IndexBlob* blob, Expr_AST* root)
    : blob { blob }
    , root { root->to_ISR(this) }
    , current { nullptr } {}

ISR_Tree::~ISR_Tree() {
    delete root;
}

std::vector<ISRWord*> ISR_Tree::getFlattenedTerms() const {
    std::vector<ISRWord*> terms;
    std::unordered_set<std::string> terms_set;
    if (root) {
        root->collectTerms(blob, terms, terms_set);
    }
    return terms;
}

/*std::vector<Post*> ISR_Tree::get_top_N_pages(size_t N, double (*Compute_Score)(Post*)) {
    // Define the RankedPost struct locally if not declared globally.
    struct RankedPost {
        Post* post;
        double score;
    };

    // Define a custom comparator.
    struct RankedPostComparator {
        bool operator()(const RankedPost& lhs, const RankedPost& rhs) const {
            // For a min-heap: if lhs.score is greater, then lhs should come after rhs.
            return lhs.score > rhs.score;
        }
    };

    std::priority_queue<RankedPost, std::vector<RankedPost>, RankedPostComparator> top_matches;

    Post* p = root->Seek(0);
    while (p) {
        double score = Compute_Score(p);
        if (top_matches.size() < N) {
            top_matches.push({ p, score });
        } else if (score > top_matches.top().score) {
            top_matches.pop();
            top_matches.push({ p, score });
        }
        p = root->Next();
    }

    std::vector<Post*> result;
    result.reserve(top_matches.size());
    while (!top_matches.empty()) {
        result.push_back(top_matches.top().post);
        top_matches.pop();
    }
    return result;
}*/
/* end ISR_Tree */