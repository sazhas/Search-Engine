#ifndef STEMMER_H
#define STEMMER_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

class Stemmer {
private:
    static bool is_name(const std::string& word);

    static std::string normalize(const std::string& word);

    static bool is_vowel(char ch);

    static bool is_double(std::string::const_iterator p1, std::string::const_iterator p2);

    static bool is_li_ending(char end);

    static size_t find_r1(const std::string& word);

    static std::pair<size_t, size_t> find_r1_r2(const std::string& word);

    static bool is_short_syllable(const std::string& word, std::string::const_iterator vow);

    static bool is_short(const std::string& word, size_t r1);

    static bool contains_vowel(std::string::const_iterator it_beg, std::string::const_iterator it_end);

    /* phase functions */

    static void strip_init_apostrophe(std::string& word);

    static void mark_y(std::string& word);

    using Chooser = std::function<std::string(const std::string&, const std::string&)>;

    using Rule = std::pair<std::string, const Chooser*>;

    struct Ruleset {
        const std::vector<Rule> suffixes_replaces_by_length;

        Ruleset(std::vector<Rule>&& suffixes_replaces_by_length, bool autosort = true);
    };

    inline static const Chooser ch_remover = [](const std::string&, const std::string&) { return ""; };

    inline static const Chooser ch_nothing = [](const std::string&, const std::string& suffix) { return suffix; };

    static Chooser gen_static_chooser(const std::string& str);

    static Chooser gen_append_chooser(const std::string& str);

    /**
     * @brief creates a Chooser that will only apply the given sub-Chooser if the given suffix is within the given
     * bounds
     */
    static Chooser gen_bounded_chooser(Chooser&& ch, size_t r1);

    static std::string apply_ruleset(const std::string& word, const Ruleset& rule);

    static void phase0(std::string& word);

    static void phase1a(std::string& word);

    static bool phase1b(std::string& word, size_t r1);

    static void phase1b_del(std::string& word, size_t r1);

    static void phase1c(std::string& word);

    static void phase2(std::string& word, size_t r1);

    static void phase3(std::string& word, size_t r1, size_t r2);

    static void phase4(std::string& word, size_t r2);

    static void phase5(std::string& word, size_t r1, size_t r2);

    static void phase6(std::string& word, size_t r1);

    static void y_to_Y(std::string& word);

public:
    /**
     * @brief stems the given word using the Porter 2 Stemming Algorithm; names and words with less than 3 characters
     * are not stemmed
     */
    static std::string stem(const std::string& word);

}; /* class Stemmer */

#endif /* STEMMER_H */