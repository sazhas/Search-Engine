#include "../stemmer.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

bool Stemmer::is_name(const std::string& word) {
    return 'A' <= word.front() && word.front() <= 'Z';
}

std::string Stemmer::normalize(const std::string& word) {
    std::string result;
    result.reserve(word.size());

    for (char ch : word) {
        const size_t diff = 'a' - 'A';

        if ('A' <= ch && ch <= 'Z')
            result.push_back(static_cast<char>(ch + diff));
        else
            result.push_back(ch);
    }

    return result;
}

bool Stemmer::is_vowel(char ch) {
    return (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'y');
}

std::pair<char, char> gen_double(char c) {
    return std::make_pair(c, c);
}

bool Stemmer::is_double(std::string::const_iterator p1, std::string::const_iterator p2) {
    std::pair p = std::make_pair(*p1, *p2);
    return (p == gen_double('b') || p == gen_double('d') || p == gen_double('f') || p == gen_double('g')
            || p == gen_double('m') || p == gen_double('n') || p == gen_double('p') || p == gen_double('r')
            || p == gen_double('t'));
}

bool Stemmer::is_li_ending(char ch) {
    return (ch == 'c' || ch == 'd' || ch == 'e' || ch == 'g' || ch == 'h' || ch == 'k' || ch == 'm' || ch == 'n'
            || ch == 'r' || ch == 't');
}

size_t Stemmer::find_r1(const std::string& word) {
    size_t start_r1 = word.size();

    bool found_vowel = false;
    for (size_t i = 0; i < word.size(); ++i) {
        if (is_vowel(word[i]))
            found_vowel = true;
        else if (found_vowel) {
            start_r1 = i + 1;
            break;
        }
    }

    return start_r1;
}

std::pair<size_t, size_t> Stemmer::find_r1_r2(const std::string& word) {
    size_t start_r1 = find_r1(word);

    std::string r2 = word.substr(start_r1);

    size_t start_r2 = find_r1(r2);
    start_r2 += start_r1;

    return std::make_pair(start_r1, start_r2);
}

bool Stemmer::is_short_syllable(const std::string& word, std::string::const_iterator vow) {
    bool found = false;
    /*
        (a) a vowel followed by a non-vowel other than w, x or Y and preceded by a non-vowel
    */
    if (vow != word.begin()) {
        auto prev = vow - 1;
        auto next = vow + 1;

        found = (next != word.end() && !is_vowel(*prev) && is_vowel(*vow)
                 && (!is_vowel(*next) && *next != 'w' && *next != 'x' && *next != 'Y'));
    }

    /*
        (b) a vowel at the beginning of the word followed by a non-vowel
    */
    else {
        auto next = vow + 1;

        found = (next != word.end() && is_vowel(*vow) && !is_vowel(*next));
    }

    /*
        (c) past
    */
    if (!found) {
        found = (word == "past");
    }

    return found;
}

bool Stemmer::is_short(const std::string& word, size_t r1) {
    std::string::const_iterator last = word.end() - 1;

    return (is_short_syllable(word, last) && word.size() <= r1);
}

bool Stemmer::contains_vowel(std::string::const_iterator it_beg, std::string::const_iterator it_end) {
    for (auto it = it_beg; it != it_end; ++it)
        if (is_vowel(*it)) return true;

    return false;
}

void Stemmer::strip_init_apostrophe(std::string& word) {
    if (word.front() == '\'') {
        word.erase(word.begin());
    }
}

void Stemmer::mark_y(std::string& word) {
    if (word.front() == 'y') {
        word.front() = 'Y';
    }

    for (auto it = word.begin() + 1; it != word.end(); ++it) {
        if (*it == 'y') {
            auto prev = it - 1;
            if (is_vowel(*prev)) {
                *it = 'Y';
            }
        }
    }
}

Stemmer::Ruleset::Ruleset(std::vector<Rule>&& s, bool autosort)
    : suffixes_replaces_by_length { std::move(s) } {
    if (!autosort) return;

    struct RevSizeComp {
        bool operator()(const Rule& a, const Rule& b) { return b.first.size() < a.first.size(); }
    };

    std::sort(s.begin(), s.end(), RevSizeComp());
}

Stemmer::Chooser Stemmer::gen_static_chooser(const std::string& suffix) {
    return Chooser([suffix](const std::string&, const std::string&) { return suffix; });
}

Stemmer::Chooser Stemmer::gen_append_chooser(const std::string& str) {
    return Chooser([str](const std::string&, const std::string& suf) { return suf + str; });
}

Stemmer::Chooser Stemmer::gen_bounded_chooser(Chooser&& ch, size_t r1) {
    return Chooser([ch = std::move(ch), r1](const std::string& stem, const std::string& suffix) {
        if (r1 <= stem.size())
            return ch(stem, suffix);
        else
            return suffix;
    });
}

std::string Stemmer::apply_ruleset(const std::string& word, const Ruleset& rule) {
    for (const auto& suffix_replace : rule.suffixes_replaces_by_length) {
        const auto& suffix = suffix_replace.first;
        const auto replace = suffix_replace.second;

        if (word.size() < suffix.size()) continue;

        std::string last = word.substr(word.size() - suffix.size());

        if (last == suffix || suffix.front() == ' ' /* generic suffix */
        ) {
            std::string stem = word.substr(0, word.size() - suffix.size());
            return stem + (*replace)(stem, last);
        }
    }

    return word;
}

void Stemmer::phase0(std::string& word) {
    /*
        Search for the longest among the suffixes,
            '
            's
            's'
        and remove if found.
    */

    const Ruleset rules {
        std::vector<Rule> {{ "'s'", &ch_remover }, { "'s", &ch_remover }, { "'", &ch_remover }}
    };

    word = apply_ruleset(word, rules);
}

void Stemmer::phase1a(std::string& word) {
    /*
        Search for the longest among the following suffixes, and perform the action indicated.

            sses
                replace by ss
            ied+   ies*
                replace by i if preceded by more than one letter, otherwise by ie (so ties → tie, cries → cri)
            s
                delete if the preceding word part contains a vowel not immediately before the s (so gas and this retain
       the s, gaps and kiwis lose it) us+   ss do nothing
    */

    const auto ch_sses = gen_static_chooser("ss");

    const Stemmer::Chooser ch_ied_ies = [](const std::string stem, const std::string&) {
        if (1 < stem.size())
            return std::string("i");
        else
            return std::string("ie");
    };

    const Stemmer::Chooser ch_s = [](const std::string& stem, const std::string& suf) {
        if (stem.size() > 2 && contains_vowel(stem.begin(), stem.end() - 1))
            return std::string();
        else
            return suf;
    };

    const Ruleset rules {
        std::vector<Rule> {{ "sses", &ch_sses },
                           { "ied", &ch_ied_ies },
                           { "ies", &ch_ied_ies },
                           { "ws", &ch_nothing },
                           { "us", &ch_nothing },
                           { "ss", &ch_nothing },
                           { "s", &ch_s }}
    };

    word = apply_ruleset(word, rules);
}

bool Stemmer::phase1b(std::string& word, size_t r1) {
    /*
        Search for the longest among the following suffixes, and perform the action indicated.

            eed   eedly+
                replace by ee if in R1
            ed   edly+   ing   ingly+
                delete if the preceding word part contains a vowel, and after the deletion:
                if the word ends at, bl or iz add e (so luxuriat → luxuriate), or
                if the word ends with a double preceded by something other than exactly a, e or o then remove the last
       letter (so hopp → hop but add, egg and off are not changed), or if the word does not end with a double and is
       short, add e (so hop → hope)
    */

    const Chooser ch_eed_eedly = [r1](const std::string& stem, const std::string& suf) {
        if (r1 <= stem.size())
            return std::string("ee");
        else
            return suf;
    };

    bool flag = false;

    const Chooser ch_ed_edly_ing_ingly = [&flag = flag](const std::string& stem, const std::string& suf) {
        if (contains_vowel(stem.begin(), stem.end())) {
            flag = true;
            return std::string();
        } else
            flag = false;
        return suf;
    };

    Ruleset rules {
        std::vector<Rule> {{ "eed", &ch_eed_eedly },
                           { "eedly", &ch_eed_eedly },
                           { "ed", &ch_ed_edly_ing_ingly },
                           { "edly", &ch_ed_edly_ing_ingly },
                           { "ing", &ch_ed_edly_ing_ingly },
                           { "ingly", &ch_ed_edly_ing_ingly }}
    };

    word = apply_ruleset(word, rules);

    return flag;
}

void Stemmer::phase1b_del(std::string& word, size_t r1) {
    /*
        if the word ends at, bl or iz add e (so luxuriat → luxuriate), or
        if the word ends with a double preceded by something other than exactly a, e or o then remove the last letter
       (so hopp → hop but add, egg and off are not changed), or if the word does not end with a double and is short, add
       e (so hop → hope)
    */

    const Chooser ch_at_bl_iz = gen_append_chooser("e");

    const Chooser ch_DOUBLE = [](const std::string& stem, const std::string& suf) {
        if (!(stem.size() == 1 && (stem.back() == 'a' || stem.back() == 'e' || stem.back() == 'o')))
            return suf.substr(0, suf.size() - 1);
        else
            return suf;
    };

    const Chooser ch_SHORT = [&word = word, r1](const std::string& stem, const std::string& suf) {
        if (is_short(word, r1))
            return suf + "e";
        else
            return suf;
    };

    Ruleset rules {
        std::vector<Rule> {{ "at", &ch_at_bl_iz },
                           { "bl", &ch_at_bl_iz },
                           { "iz", &ch_at_bl_iz },
                           { "bb", &ch_DOUBLE },
                           { "dd", &ch_DOUBLE },
                           { "ff", &ch_DOUBLE },
                           { "gg", &ch_DOUBLE },
                           { "mm", &ch_DOUBLE },
                           { "nn", &ch_DOUBLE },
                           { "pp", &ch_DOUBLE },
                           { "rr", &ch_DOUBLE },
                           { "tt", &ch_DOUBLE },
                           { "  ", &ch_SHORT }}
    };

    word = apply_ruleset(word, rules);
}

void Stemmer::phase1c(std::string& word) {
    /*
        replace suffix y or Y by i if preceded by a non-vowel which is not the first letter of the word (so cry → cri,
       by → by, say → say)
    */

    const Chooser ch_y_Y = [](const std::string& stem, const std::string& suf) {
        if (1 < stem.size() && !is_vowel(stem.back()))
            return std::string("i");
        else
            return suf;
    };

    Ruleset rules {
        std::vector<Rule> {{ "y", &ch_y_Y }, { "Y", &ch_y_Y }}
    };

    word = apply_ruleset(word, rules);
}

void Stemmer::phase2(std::string& word, size_t r1) {
    /*
        Search for the longest among the following suffixes, and, if found and in R1, perform the action indicated.

            tional:   replace by tion
            enci:   replace by ence
            anci:   replace by ance
            abli:   replace by able
            entli:   replace by ent
            izer   ization:   replace by ize
            ational   ation   ator:   replace by ate
            alism   aliti   alli:   replace by al
            fulness:   replace by ful
            ousli   ousness:   replace by ous
            iveness   iviti:   replace by ive
            biliti   bli+:   replace by ble
            ogi+:   replace by og if preceded by l
            fulli+:   replace by ful
            lessli+:   replace by less
            li+:   delete if preceded by a valid li-ending
    */

    const Chooser ch_tional = gen_static_chooser("tion");
    const Chooser ch_enci = gen_static_chooser("ence");
    const Chooser ch_anci = gen_static_chooser("ance");
    const Chooser ch_abli = gen_static_chooser("able");
    const Chooser ch_entli = gen_static_chooser("ent");
    const Chooser ch_izer_ization = gen_static_chooser("ize");
    const Chooser ch_ational_ation_ator = gen_static_chooser("ate");
    const Chooser ch_alism_aliti_alli = gen_static_chooser("al");
    const Chooser ch_fulness = gen_static_chooser("ful");
    const Chooser ch_ousli_ousness = gen_static_chooser("ous");
    const Chooser ch_iveness_iviti = gen_static_chooser("ive");
    const Chooser ch_biliti_bli = gen_static_chooser("ble");

    const Chooser ch_ogi = [](const std::string& stem, const std::string& suf) {
        if (!stem.empty() && stem.back() == 'l')
            return std::string("og");
        else
            return suf;
    };

    const Chooser ch_fulli = gen_static_chooser("ful");
    const Chooser ch_lessli = gen_static_chooser("less");

    const Chooser ch_li = [](const std::string& stem, const std::string& suf) {
        if (!stem.empty() && is_li_ending(stem.back()))
            return std::string();
        else
            return suf;
    };

    Ruleset rules {
        std::vector<Rule> {{ "tional", &ch_tional },
                           { "enci", &ch_enci },
                           { "anci", &ch_anci },
                           { "abli", &ch_abli },
                           { "entli", &ch_entli },
                           { "izer", &ch_izer_ization },
                           { "ization", &ch_izer_ization },
                           { "ational", &ch_ational_ation_ator },
                           { "ation", &ch_ational_ation_ator },
                           { "ator", &ch_ational_ation_ator },
                           { "alism", &ch_alism_aliti_alli },
                           { "aliti", &ch_alism_aliti_alli },
                           { "alli", &ch_alism_aliti_alli },
                           { "fulness", &ch_fulness },
                           { "ousli", &ch_ousli_ousness },
                           { "ousness", &ch_ousli_ousness },
                           { "iveness", &ch_iveness_iviti },
                           { "iviti", &ch_iveness_iviti },
                           { "biliti", &ch_biliti_bli },
                           { "bli", &ch_biliti_bli },
                           { "ogi", &ch_ogi },
                           { "fulli", &ch_fulli },
                           { "lessli", &ch_lessli },
                           { "li", &ch_li }}
    };
    word = apply_ruleset(word, rules);
}

void Stemmer::phase3(std::string& word, size_t r1, size_t r2) {
    /*
        Search for the longest among the following suffixes, and, if found and in R1, perform the action indicated.

            tional+:   replace by tion
            ational+:   replace by ate
            alize:   replace by al
            icate   iciti   ical:   replace by ic
            ful   ness:   delete
            ative*:   delete if in R2
    */

    const Chooser ch_tional = gen_bounded_chooser(gen_static_chooser("tion"), r1);
    const Chooser ch_ational = gen_bounded_chooser(gen_static_chooser("ate"), r1);
    const Chooser ch_alize = gen_bounded_chooser(gen_static_chooser("al"), r1);
    const Chooser ch_icate_iciti_ical = gen_bounded_chooser(gen_static_chooser("ic"), r1);
    const Chooser ch_ful_ness = gen_bounded_chooser(Chooser { ch_remover }, r1);
    const Chooser ch_ative = gen_bounded_chooser(Chooser { ch_remover }, r2);

    Ruleset rules {
        std::vector<Rule> {{ "tional", &ch_tional },
                           { "ational", &ch_ational },
                           { "alize", &ch_alize },
                           { "icate", &ch_icate_iciti_ical },
                           { "iciti", &ch_icate_iciti_ical },
                           { "ical", &ch_icate_iciti_ical },
                           { "ful", &ch_ful_ness },
                           { "ness", &ch_ful_ness },
                           { "ative", &ch_ative }}
    };

    word = apply_ruleset(word, rules);
}

void Stemmer::phase4(std::string& word, size_t r2) {
    /*
        Search for the longest among the following suffixes, and, if found and in R2, perform the action indicated.

            al   ance   ence   er   ic   able   ible   ant   ement   ment   ent   ism   ate   iti   ous   ive   ize
                delete
            ion
                delete if preceded by s or t
    */

    const Chooser ch_ALL = gen_bounded_chooser(Chooser { ch_remover }, r2);

    const Chooser ch_ion = gen_bounded_chooser(
      [](const std::string& stem, const std::string& suf) {
          if (!stem.empty() && (stem.back() == 's' || stem.back() == 't'))
              return std::string();
          else
              return suf;
      },
      r2);

    const Chooser ch_ize_safe = gen_bounded_chooser(

      [](const std::string& stem, const std::string&) {
          if (stem.size() >= 5)

              return std::string();

          else

              return std::string("ize");
      },

      r2);
    Ruleset rules {
        std::vector<Rule> {{ "al", &ch_ALL },
                           { "ance", &ch_ALL },
                           { "ence", &ch_ALL },
                           { "er", &ch_ALL },
                           { "ic", &ch_ALL },
                           { "able", &ch_ALL },
                           { "ible", &ch_ALL },
                           { "ant", &ch_ALL },
                           { "ement", &ch_ALL },
                           { "ment", &ch_ALL },
                           { "ent", &ch_ALL },
                           { "ism", &ch_ALL },
                           { "ate", &ch_ALL },
                           { "iti", &ch_ALL },
                           { "ous", &ch_ALL },
                           { "ive", &ch_ALL },
                           { "ize", &ch_ize_safe },
                           { "ion", &ch_ion }}
    };

    word = apply_ruleset(word, rules);
}

void Stemmer::phase5(std::string& word, size_t r1, size_t r2) {
    /*
        Search for the following suffixes, and, if found, perform the action indicated.

            e
                delete if in R2, or in R1 and not preceded by a short syllable
            l
                delete if in R2 and preceded by l
    */

    const Chooser ch_e = [r1, r2](const std::string& stem, const std::string& suf) {
        if (r2 <= stem.size())
            return std::string();
        else if (r1 <= stem.size() && !is_short_syllable(stem, stem.end() - 1))
            return std::string();
        else
            return suf;
    };

    const Chooser ch_l = gen_bounded_chooser(
      [](const std::string& stem, const std::string& suf) {
          if (!stem.empty() && stem.back() == 'l')
              return std::string();
          else
              return suf;
      },
      r2);

    Ruleset rules {
        std::vector<Rule> {{ "e", &ch_e }, { "l", &ch_l }}
    };

    word = apply_ruleset(word, rules);
}

void Stemmer::phase6(std::string& word, size_t r1) {
    const Chooser ch_er_est = gen_bounded_chooser(Chooser { ch_remover }, r1);

    Ruleset rules {
        std::vector<Rule> {{ "er", &ch_er_est }, { "est", &ch_er_est }}
    };

    word = apply_ruleset(word, rules);
}


void Stemmer::y_to_Y(std::string& word) {
    for (auto it = word.begin(); it != word.end(); ++it) {
        if (*it == 'Y') *it = 'y';
    }
}

std::string Stemmer::stem(const std::string& word) {
    static const std::unordered_set<char> punct { '.', ',', ';', ':', '!', '?', '"', ')', ']', '}', '\'' };
    static const std::unordered_map<std::string, std::string> exceptions = {
        {   "children",       "child"},
        {      "women",       "woman"},
        {        "men",         "man"},
        {       "news",        "news"},
        {       "data",        "data"},
        {      "media",       "media"},
        {   "analysis",    "analysis"},
        {     "series",      "series"},
        {    "species",     "species"},
        {      "money",       "money"},
        {"information", "information"},
        {     "person",      "person"},
        {     "people",      "people"},
        {      "teeth",       "tooth"},
        {      "geese",       "goose"},
        {       "feet",        "foot"},
        {       "mice",       "mouse"},
        {    "indices",       "index"},
        {   "matrices",      "matrix"},
        {       "oxen",          "ox"},
        {       "dice",         "die"},
        {     "knives",       "knife"},
        {     "leaves",        "leaf"},
        {      "wives",        "wife"},
        {      "lives",        "life"},
        {    "thieves",       "thief"},
        {     "wolves",        "wolf"},
        {   "potatoes",      "potato"},
        {   "tomatoes",      "tomato"},
        {     "heroes",        "hero"},
        {     "echoes",        "echo"},
        {      "buses",         "bus"},
        {     "polite",      "polite"}
    };
    static const std::unordered_set<std::string> stop
      = { "the", "a",   "an",    "and",  "or",   "but",   "if",    "with", "by",  "on",   "for",
          "in",  "of",  "to",    "at",   "is",   "are",   "was",   "were", "be",  "been", "being",
          "as",  "it",  "its",   "this", "that", "these", "those", "he",   "she", "they", "them",
          "his", "her", "their", "you",  "your", "we",    "our",   "i",    "me",  "my" };

    auto _word = word;

    while (!_word.empty() && punct.count(_word.back())) _word.pop_back();
    if (_word.empty()) return _word;

    // auto work_norm = normalize(_word);
    if (stop.count(_word)) return "";

    auto it_exc = exceptions.find(_word);
    if (it_exc != exceptions.end()) return it_exc->second;

    if (_word.size() < 3) return _word;

    strip_init_apostrophe(_word);
    mark_y(_word);
    auto r1_r2 = find_r1_r2(_word);
    auto r1 = r1_r2.first;
    auto r2 = r1_r2.second;
    phase0(_word);
    phase1a(_word);
    if (phase1b(_word, r1)) phase1b_del(_word, r1);
    phase1c(_word);
    phase2(_word, r1);
    phase3(_word, r1, r2);
    phase4(_word, r2);
    phase5(_word, r1, r2);
    phase6(_word, r1);
    y_to_Y(_word);
    return _word;
}