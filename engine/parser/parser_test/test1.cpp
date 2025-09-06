#include "../HtmlParser.h"
#include <iostream>

void printWordFlags(const std::vector<WFs>& words_flags) {
    for (const auto& wf : words_flags) {
        std::cout << "word: \"" << wf.word << "\" flags: ";
        if (wf.flags & 0x01) std::cout << "[BOLD] ";
        if (wf.flags & 0x02) std::cout << "[HEADING] ";
        if (wf.flags & 0x04) std::cout << "[LARGE FONT] ";
        std::cout << '\n';
    }
}

int main() {
    const char* html =
        "<html>"
        "<head><title>This is a Test Title</title></head>"
        "<body>"
        "<h2>This is a heading</h2>"
        "<p>This is a <b>bold</b> word.</p>"
        "<a href=\"http://example.com\">Example Link</a>"
        "</body></html>";

    HtmlParser parser(html, strlen(html));

    std::cout << "--- Title Words ---\n";
    for (const auto& w : parser.titleWords) {
        std::cout << w << '\n';
    }

    std::cout << "\n--- Title Chunk ---\n";
    std::cout << parser.title_chunk << "\n";

    std::cout << "\n--- Words with Flags ---\n";
    printWordFlags(parser.words_flags);

    std::cout << "\n--- Links ---\n";
    for (const auto& link : parser.links) {
        std::cout << "URL: " << link.URL << '\n';
        std::cout << "Anchor Text: ";
        for (const auto& word : link.anchorText) {
            std::cout << word << ' ';
        }
        std::cout << "\n";
    }

    return 0;
}