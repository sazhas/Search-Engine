#include "Ranker.hpp"

namespace Ranker {

Ranker::Ranker(IndexBlob* index, size_t maxResults)
    : index(index)
    , maxResults(maxResults) {}

Ranker::~Ranker() {}

void Ranker::SeekToDocStart(std::vector<ISRWord*>& terms, Location docStart) {
    for (auto term : terms) {
        term->Seek(docStart);
    }
}

Span Ranker::FindBestSpan(ISRWord* rarestTerm, const std::vector<ISRWord*>& otherTerms, Location targetPos,
                          const std::vector<Location>& expectedPositions) {
    Span span;
    span.termCount = 1;

    ISRDoc* docIsr = index->OpenISREndDoc();
    docIsr->Seek(targetPos);
    Location docEnd = docIsr->GetEndLocation();
    delete docIsr;

    span.isExactPhrase = true;
    span.isOrdered = true;
    span.isClose = true;
    span.isBoldHeading = false;

    Location prevLocation = 0;

    // Try to find each other term close to its expected position
    for (size_t i = 0; i < otherTerms.size(); i++) {
        if (expectedPositions[i] == 1) {
            prevLocation = targetPos;
        }
        auto term = otherTerms[i];
        Location expected = targetPos + expectedPositions[i];

        // Single stage of lookahead - look for closest position within threshold
        Post* post = term->Seek(expected - CLOSE_THRESHOLD);

        span.isExactPhrase = false;
        span.isOrdered = false;
        span.isClose = false;
        // Find first occurrence within threshold
        while (post && post->GetStartLocation() <= expected + CLOSE_THRESHOLD && post->GetStartLocation() <= docEnd) {
            Location pos = post->GetStartLocation();
            auto dist = static_cast<long>(pos) - static_cast<long>(expected);
            WordPost* wp = reinterpret_cast<WordPost*>(post);

            if (isBold(*wp) || isHeading(*wp)) {
                span.isBoldHeading = true;
            }

            // Check exact phrase - terms must be exactly where expected
            if (pos > prevLocation) {
                span.isOrdered = true;
                prevLocation = pos;
            }

            if (dist == 0) {
                span.isExactPhrase = true;
            }

            if (abs(dist) <= CLOSE_THRESHOLD && !span.isClose) {
                span.termCount++;
                span.isClose = true;
            }
            post = term->NextInternal();
        }
    }

    return span;
}

ISRWord* Ranker::FindRarestTerm(const std::vector<ISRWord*>& terms, ISRDoc* doc) {
    ISRWord* rarestTerm = nullptr;
    size_t minFreq = static_cast<size_t>(-1);

    for (size_t i = 0; i < terms.size(); ++i) {
        size_t freq = terms[i]->GetOccurrencesInCurrDoc(doc->GetStartLocation(), doc->GetEndLocation());
        if (freq > 0 && freq < minFreq) {
            minFreq = freq;
            rarestTerm = terms[i];
        }
    }

    return rarestTerm;
}

StaticFeatures Ranker::ExtractStaticFeatures(Location start, Location end, const DocumentAttributes* attr) {
    StaticFeatures features;

    // Extract basic document features
    // features.documentLength = doc->GetDocumentLength();
    // features.uniqueTermCount = doc->GetUniqueWordCount();
    features.wordCount = attr->wordCount;
    features.urlLength = attr->urlLength;
    features.english = attr->english;
    features.titleLength = attr->titleLength;
    features.tld = static_cast<TLD>(attr->TLD);
    features.isUtilityPage = IsUtilityPage(attr->url);
    return features;
}

bool Ranker::IsUtilityPage(const char* url) {
    static const char* patterns[] = { "privacy", "terms", "404", "error", "policy", "legal" };

    std::string urlStr(url);
    for (size_t i = 0; i < urlStr.length(); i++) {
        urlStr[i] = tolower(urlStr[i]);
    }

    for (const char* pattern : patterns) {
        if (urlStr.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

QueryIntent Ranker::AnalyzeQueryIntent(const std::vector<ISRWord*>& queryTerms) {
    QueryIntent intent;
    intent.isUtilityQuery = false;

    // Utility-related terms
    static const std::string utilityTerms[] = { "privacy", "terms", "policy", "legal", "contact", "about", "cookies" };

    if (queryTerms.empty()) {
        return intent;
    }

    // Set main term as the first term
    intent.mainTerm = queryTerms[0]->GetKey();

    // Analyze additional terms for utility intent
    for (size_t i = 1; i < queryTerms.size(); i++) {
        std::string term = queryTerms[i]->GetKey();
        intent.modifiers.push_back(term);

        // Check if any modifier suggests utility content
        for (const auto& utilityTerm : utilityTerms) {
            if (term == utilityTerm) {
                intent.isUtilityQuery = true;
                break;
            }
        }
    }

    return intent;
}

DynamicFeatures Ranker::ExtractDynamicFeatures(Location start, Location end, const std::vector<ISRWord*>& queryTerms,
                                               const char* url) {
    DynamicFeatures features;

    // TODO: bold and heading features
    features.exactPhraseCount = 0;
    features.orderedCount = 0;
    features.closeCount = 0;
    features.doubleCount = 0;
    features.tripleCount = 0;
    features.boldHeadingCount = 0;
    features.topPositionSpans = 0;
    features.firstSpanPosition = static_cast<Location>(-1);
    features.hasUrlMatch = false;

    if (queryTerms.empty()) return features;

    // Calculate term frequencies and find rarest term
    features.termFrequencies.resize(queryTerms.size());
    ISRWord* rarestTerm = nullptr;
    size_t rarestTermIndex = 0;
    size_t minFreq = static_cast<size_t>(-1);

    std::string urlStr;
    if (url != nullptr) {
        urlStr = url;
        for (char& c : urlStr) {
            c = tolower(c);
        }
    }

    for (size_t i = 0; i < queryTerms.size(); i++) {
        features.termFrequencies[i] = queryTerms[i]->GetOccurrencesInCurrDoc(start, end);

        if (!features.hasUrlMatch && !urlStr.empty()) {
            std::string term = queryTerms[i]->GetKey();
            for (size_t j = 0; j < term.length(); j++) {
                term[j] = tolower(term[j]);
            }
            if (urlStr.find(term) != std::string::npos) {
                features.hasUrlMatch = true;
            }
        }

        if (features.termFrequencies[i] > 0 && features.termFrequencies[i] < minFreq) {
            minFreq = features.termFrequencies[i];
            rarestTerm = queryTerms[i];
            rarestTermIndex = i;
        }
    }

    if (!rarestTerm) {
        return features;
    }

    // Setup expected relative positions
    std::vector<ISRWord*> otherTerms;
    std::vector<Location> expectedPositions;
    for (size_t i = 0; i < queryTerms.size(); i++) {
        if (i != rarestTermIndex) {
            otherTerms.push_back(queryTerms[i]);
            int relativePos = static_cast<int>(i) - static_cast<int>(rarestTermIndex);
            expectedPositions.push_back(relativePos);
        }
    }

    // Process each occurrence of rarest term
    Post* post = rarestTerm->Seek(start);
    while (post && post->GetStartLocation() <= end) {
        Location rarestTermPos = post->GetStartLocation();

        // Track first span position
        if (features.firstSpanPosition == static_cast<Location>(-1)) {
            features.firstSpanPosition = rarestTermPos;
        }

        // Find best span around this occurrence of rarest term
        Span span = FindBestSpan(rarestTerm, otherTerms, rarestTermPos, expectedPositions);
        WordPost* wp = reinterpret_cast<WordPost*>(post);
        if (span.isBoldHeading || isBold(*wp) || isHeading(*wp)) {
            features.boldHeadingCount++;
        }

        // Update span counters
        if (span.isExactPhrase)
            features.exactPhraseCount++;
        else if (span.isOrdered)
            features.orderedCount++;
        else if (span.isClose)
            features.closeCount++;

        // Track doubles and triples
        if (span.termCount == 2)
            features.doubleCount++;
        else if (span.termCount >= 3 || queryTerms.size() == 1)
            features.tripleCount++;

        // Track position-based features
        if (rarestTermPos <= TOP_POSITION_THRESHOLD) {
            features.topPositionSpans++;
        }

        features.spans.push_back(span);

        post = rarestTerm->NextInternal();
    }

    return features;
}

double Ranker::GetTLDScore(const TLD tld) {
    switch (tld) {
    case TLD::GOV:
        return 1.0;
    case TLD::EDU:
        return 0.95;
    case TLD::ORG:
        return 0.9;
    case TLD::COM:
        return 0.75;
    case TLD::NET:
        return 0.7;
    case TLD::US:
        return 0.7;
    case TLD::IO:
        return 0.6;
    case TLD::DEV:
        return 0.6;
    case TLD::INFO:
        return 0.4;
    case TLD::BIZ:
        return 0.3;
    case TLD::XYZ:
        return 0.2;
    case TLD::TOP:
        return 0.1;
    case TLD::UNKNOWN:
    default:
        return 0.05;
    }
}

double Ranker::CalculateStaticScore(const StaticFeatures& features, const std::vector<ISRWord*>& queryTerms) {
    // Calculate URL length score
    double k_url = 0.02;
    double urlScore = custom_exp(-k_url * features.urlLength);

    // Calculate domain TLD score
    double tldScore = GetTLDScore(features.tld);

    // Calculate document length score
    double optimalLength = 600.0;
    double lengthDiff = features.wordCount - optimalLength;
    double docLengthScore = 1.0 / (1.0 + (lengthDiff * lengthDiff) / 250000.0);

    // Calculate title length score
    double k_title = 0.08;
    double titleDiff = features.titleLength > OPTIMAL_TITLE_LENGTH ? features.titleLength - OPTIMAL_TITLE_LENGTH : 0;
    double titleLengthScore = custom_exp(-k_title * titleDiff);

    // Combine scores with updated weights
    double baseScore = (urlScore * URL_LENGTH_WEIGHT) + (tldScore * TLD_WEIGHT) + (docLengthScore * DOC_LENGTH_WEIGHT)
                     + (titleLengthScore * TITLE_LENGTH_WEIGHT);

    // Apply language penalty if non-English
    if (!features.english) {
        baseScore *= NON_ENGLISH_WEIGHT;
    }

    // Apply utility page penalty unless it's a utility-focused query
    QueryIntent queryIntent = AnalyzeQueryIntent(queryTerms);
    if (features.isUtilityPage && !queryIntent.isUtilityQuery) {
        baseScore *= UTILITY_PAGE_PENALTY;
    }

    return baseScore;
}

double Ranker::CalculateDynamicScore(const DynamicFeatures& features, bool isTitle, uint32_t docLength) {
    // Calculate span quality score
    int total = features.exactPhraseCount + features.orderedCount + features.closeCount + features.doubleCount
              + features.tripleCount;
    double spanScore = features.exactPhraseCount * EXACT_PHRASE_WEIGHT + features.orderedCount * ORDERED_SPAN_WEIGHT
                     + features.closeCount * CLOSE_SPAN_WEIGHT + features.doubleCount * SHORTEST_SPAN_WEIGHT
                     + features.tripleCount * SHORT_SPAN_WEIGHT;

    if (total > 0) {
        spanScore /= total;
        if (!isTitle) {
            // Boost weight based on span count
            const double minBoost = 0.3;
            const double boostRange = 0.7;   // 1.0 - 0.3
            const double k = 1.2;
            const double x0 = 4;   // midpoint of the transition
            double spanBoost = minBoost + boostRange / (1.0 + custom_exp(-k * (total - x0)));

            const double minBoldBoost = 0.7;
            const double boostBoldRange = 0.3;   // 1.0 - 0.7
            const double bold_k = 4;
            const double bold_x0 = 1;   // midpoint of the transition
            double boldspanBoost
              = minBoldBoost
              + boostBoldRange
                  / (1.0 + custom_exp(-bold_k * (static_cast<double>(features.boldHeadingCount) - bold_x0)));

            spanScore *= spanBoost * boldspanBoost;
        }
    }

    // Calculate position score
    double positionScore = features.topPositionSpans * TOP_POSITION_WEIGHT;

    // Calculate frequency score
    double freqScore = 0.0;
    if (!features.termFrequencies.empty()) {
        size_t frequentCount = 0;
        for (size_t freq : features.termFrequencies) {
            double tf = static_cast<double>(freq) / static_cast<double>(docLength);
            if (tf >= FREQUENT_THRESHOLD) {
                frequentCount++;
            }
        }

        double freqWeight = 0.0;
        if (frequentCount == features.termFrequencies.size()) {
            freqWeight = ALL_FREQUENT_WEIGHT;
        } else if (frequentCount >= features.termFrequencies.size() * MOST_WORDS_RATIO) {
            freqWeight = MOST_FREQUENT_WEIGHT;
        } else if (frequentCount > 0) {
            freqWeight = SOME_FREQUENT_WEIGHT;
        }

        freqScore = freqWeight;
    }

    double baseScore = (spanScore * 0.5) + (positionScore * 0.3) + (freqScore * 0.2);

    if (features.hasUrlMatch && isTitle) {
        baseScore *= URL_TERM_MATCH_BOOST;
    }

    return baseScore;
}

void Ranker::InsertResult(std::vector<RankingResult>& results, RankingResult& newResult) {
    if (results.size() < maxResults) {
        results.push_back(newResult);
    } else if (newResult.score <= results.back().score) {
        return;
    } else {
        results.back() = newResult;
    }

    size_t i = results.size() - 1;
    while (i > 0 && results[i - 1].score < newResult.score) {
        results[i] = results[i - 1];
        i--;
    }
    results[i] = newResult;
}

void separateISRs(std::vector<ISRWord*>& terms, std::vector<ISRWord*>& title_words, std::vector<ISRWord*>& body_words,
                  std::vector<ISRWord*>& title_syn_words, std::vector<ISRWord*>& body_syn_words) {
    for (auto& term : terms) {
        const char* key = term->GetKey();
        bool syn = term->isSynonymWord();
        if (key != nullptr && key[0] == '@') {
            if (syn) {
                title_syn_words.push_back(term);
            } else {
                title_words.push_back(term);
            }
        } else {
            if (syn) {
                body_syn_words.push_back(term);
            } else {
                body_words.push_back(term);
            }
        }
    }
}

void* Ranker::WorkerThread(void* arg) {
    auto* args = static_cast<ThreadArgs*>(arg);
    Location lastDocID = 0;

    auto termsCopy = args->tree->getFlattenedTerms();

    std::vector<ISRWord*> title_words;
    std::vector<ISRWord*> body_words;
    std::vector<ISRWord*> title_syn_words;
    std::vector<ISRWord*> body_syn_words;
    separateISRs(termsCopy, title_words, body_words, title_syn_words, body_syn_words);

    while (true) {
        pthread_mutex_lock(args->queueMutex);
        // cout << "CALLING NEXT\n";
        Post* doc = args->root->Next();
        // cout << "PAST NEXT\n";
        if (!doc) {
            pthread_mutex_unlock(args->queueMutex);
            break;
        }
        if (doc->GetStartLocation() <= lastDocID) {
            // std::cerr << "[ERROR] Looping on same doc. Exiting.\n";
            pthread_mutex_unlock(args->queueMutex);
            break;
        }
        lastDocID = doc->GetStartLocation();
        auto docEnd = args->root->GetCurrentDoc();
        if (!docEnd) {
            // std::cerr << "[ERROR] No docEnd.\n";
            pthread_mutex_unlock(args->queueMutex);
            break;
        }
        auto start = docEnd->GetStartLocation();
        auto end = docEnd->GetEndLocation();
        auto attributes = args->index->GetDocAttributes(docEnd->GetID());

        pthread_mutex_unlock(args->queueMutex);

        Ranker dummyRanker(args->index, args->maxResults);

        auto static_features = dummyRanker.ExtractStaticFeatures(start, end, attributes);
        double staticScore = dummyRanker.CalculateStaticScore(static_features, termsCopy);
        // std::cout << "URL: " << attributes->url << std::endl;
        // std::cout << staticScore << endl;

        // Hard cutoff for extremely long titles (reject regardless of other scores)
        if (static_features.titleLength > OPTIMAL_TITLE_LENGTH * 4) {
            continue;
        }

        // threshold
        if (staticScore < STATIC_THRESHOLD) {
            continue;
        }

        dummyRanker.SeekToDocStart(termsCopy, start);

        auto title_features = dummyRanker.ExtractDynamicFeatures(start, end, title_words, attributes->url);
        auto body_features = dummyRanker.ExtractDynamicFeatures(start, end, body_words);

        double titleScore = dummyRanker.CalculateDynamicScore(title_features, true, attributes->titleLength);
        double bodyScore
          = dummyRanker.CalculateDynamicScore(body_features, false, attributes->wordCount - attributes->titleLength);
        double dynamicScore = TITLE_WEIGHT * titleScore + BODY_WEIGHT * bodyScore;

        if (dynamicScore < DYNAMIC_THRESHOLD) {
            title_features = dummyRanker.ExtractDynamicFeatures(start, end, title_syn_words, attributes->url);
            body_features = dummyRanker.ExtractDynamicFeatures(start, end, body_syn_words);
            titleScore = dummyRanker.CalculateDynamicScore(title_features, true, attributes->titleLength);
            bodyScore = dummyRanker.CalculateDynamicScore(body_features, false,
                                                          attributes->wordCount - attributes->titleLength);
            double newScore = TITLE_WEIGHT * titleScore + BODY_WEIGHT * bodyScore;
            dynamicScore = newScore * SYN_WEIGHT + dynamicScore * ORIGIN_WEIGHT;
            if (dynamicScore < DYNAMIC_THRESHOLD) {
                continue;
            }
        }
        double finalScore = dynamicScore * 0.75 + staticScore * 0.25;

        RankingResult result;
        result.url = attributes->url;
        result.title = (attributes->title) ? attributes->title : attributes->url;
        result.score = finalScore;
        // std::cout << "URL: " << result.url << " \nStatic: " << staticScore << " \nTitle Dynamic: " << titleScore
        //<< " Body Dynamic: " << bodyScore << " \nDynamic Score: " << dynamicScore << " \nFinal: " << finalScore <<
        //"\n";
        // std::cout << "URL: " << result.url << std::endl;
        // std::cout << "TITLE: " << result.title << std::endl;
        // std::cout << "SCORE: " << result.score << std::endl;

        pthread_mutex_lock(args->resultsMutex);

        dummyRanker.InsertResult(*args->results, result);
        args->processedDocs++;
        if (args->processedDocs >= MAX_DOCS) {
            // std::cout << "PROCESSED ENOUGH DOCS\n";
            pthread_mutex_unlock(args->resultsMutex);
            break;
        }
        pthread_mutex_unlock(args->resultsMutex);
    }

    for (auto& isr : termsCopy) {
        delete isr;
    }

    return nullptr;
}


std::vector<RankingResult> Ranker::RankResults(ISR_Tree* tree) {
    std::vector<RankingResult> results;

    ISR* root = tree->get_root();
    if (!root) return results;

    constexpr int NUM_THREADS = 14;
    pthread_t threads[NUM_THREADS];
    pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t resultsMutex = PTHREAD_MUTEX_INITIALIZER;

    ThreadArgs args { .tree = tree,
                      .root = root,
                      .index = index,
                      .processedDocs = 0,
                      .maxResults = maxResults,
                      .queueMutex = &queueMutex,
                      .resultsMutex = &resultsMutex,
                      .results = &results };

    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_create(&threads[i], nullptr, WorkerThread, &args) != 0) {
            perror("Failed to create thread in ranker");
        }
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_join(threads[i], nullptr) != 0) {
            perror("Failed to join thread in ranker");
        }
    }

    return results;
}

}   // namespace Ranker
