// Ranker.hpp

#ifndef RANKER_HPP
#define RANKER_HPP

// #include <atomic>
#include <cstdint>
#include <string>   // TODO: add custom string class
#include <vector>   // TODO: add custom vector class

#include "../csolver/isr.h"
#include "../indexer/Indexer.hpp"
#include "../lib/algorithm.h"
class ISR_Tree;

struct RankingResult {
    const char* url;
    const char* title;
    double score;
};

namespace Ranker {

struct ThreadArgs {
    ISR_Tree* tree;
    ISR* root;
    IndexBlob* index;
    uint32_t processedDocs;
    size_t maxResults;
    pthread_mutex_t* queueMutex;
    pthread_mutex_t* resultsMutex;
    std::vector<RankingResult>* results;
};

struct Span {
    size_t termCount;
    bool isExactPhrase;
    bool isOrdered;
    bool isClose;
    bool isBoldHeading;
};

struct StaticFeatures {
    size_t wordCount;
    size_t urlLength;
    TLD tld;
    uint32_t titleLength;
    bool english;
    bool isUtilityPage;
};

struct DynamicFeatures {
    std::vector<Span> spans;
    size_t exactPhraseCount;
    size_t orderedCount;
    size_t closeCount;
    size_t doubleCount;
    size_t tripleCount;
    size_t boldHeadingCount;
    std::vector<size_t> termFrequencies;
    size_t topPositionSpans;
    size_t frequentTermCount;
    Location firstSpanPosition;
    bool hasUrlMatch;
};

struct QueryIntent {
    bool isUtilityQuery;
    std::string mainTerm;
    std::vector<std::string> modifiers;
};

class Ranker {
public:
    Ranker(IndexBlob* index, size_t maxResults = 10);
    ~Ranker();

    std::vector<RankingResult> RankResults(ISR_Tree* tree);

private:
    IndexBlob* index;
    size_t maxResults;
    static constexpr size_t CLOSE_THRESHOLD = 10;
    static constexpr size_t TOP_POSITION_THRESHOLD = 100;
    static constexpr double MOST_WORDS_RATIO = 0.7;
    static constexpr double SHORT_SPAN_WEIGHT = 0.04;
    static constexpr double SHORTEST_SPAN_WEIGHT = 0.08;
    static constexpr double CLOSE_SPAN_WEIGHT = 0.23;
    static constexpr double ORDERED_SPAN_WEIGHT = 0.10;
    static constexpr double EXACT_PHRASE_WEIGHT = 0.55;
    static constexpr double TOP_POSITION_WEIGHT = 1.0;
    static constexpr double ALL_FREQUENT_WEIGHT = 0.57;
    static constexpr double MOST_FREQUENT_WEIGHT = 0.29;
    static constexpr double SOME_FREQUENT_WEIGHT = 0.14;
    static constexpr double NON_ENGLISH_WEIGHT = 0.14;
    static constexpr double TITLE_WEIGHT = 0.7;
    static constexpr double BODY_WEIGHT = 0.3;
    static constexpr uint32_t MAX_DOCS = 100;
    static constexpr double SCORE_THRESHOLD = .5;
    static constexpr double STATIC_THRESHOLD = 0.25;
    static constexpr double DYNAMIC_THRESHOLD = 0.1;
    static constexpr double OPTIMAL_TITLE_LENGTH = 10.0;
    static constexpr double TITLE_LENGTH_WEIGHT = 0.15;

    static constexpr double UTILITY_PAGE_PENALTY = 0.15;
    static constexpr double URL_LENGTH_WEIGHT = 0.35;
    static constexpr double TLD_WEIGHT = 0.35;
    static constexpr double DOC_LENGTH_WEIGHT = 0.15;
    static constexpr double MAIN_CONTENT_WEIGHT = 0.10;
    static constexpr double SYN_WEIGHT = 0.4;
    static constexpr double ORIGIN_WEIGHT = 0.6;

    static constexpr double URL_TERM_MATCH_BOOST = 1.2;
    static constexpr double FREQUENT_THRESHOLD = .01;

    // Feature extraction
    StaticFeatures ExtractStaticFeatures(Location start, Location end, const DocumentAttributes* attr);
    DynamicFeatures ExtractDynamicFeatures(Location start, Location end, const std::vector<ISRWord*>& queryTerms,
                                           const char* url = nullptr);
    void SeekToDocStart(std::vector<ISRWord*>& terms, Location docStart);
    double GetTLDScore(const TLD tld);
    ISRWord* FindRarestTerm(const std::vector<ISRWord*>& terms, ISRDoc* doc);
    Span FindBestSpan(ISRWord* rarestTerm, const std::vector<ISRWord*>& otherTerms, Location targetPos,
                      const std::vector<Location>& expectedPositions);
    void ClassifySpan(Span& span);
    QueryIntent AnalyzeQueryIntent(const std::vector<ISRWord*>& queryTerms);
    bool IsUtilityPage(const char* url);

    // Scoring
    double CalculateStaticScore(const StaticFeatures& features, const std::vector<ISRWord*>& queryTerms);
    double CalculateDynamicScore(const DynamicFeatures& features, bool isTitle, uint32_t docLength);
    void InsertResult(std::vector<RankingResult>& results, RankingResult& newResult);
    double GetTLDScore(const std::string& domain);
    static void* WorkerThread(void* arg);
};

}   // namespace Ranker

#endif   // RANKER_HPP
