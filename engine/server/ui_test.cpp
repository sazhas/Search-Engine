#include <iostream>
#include <vector>
#include <string>
#include <fstream>

namespace Query {
    struct SearchResult {
        std::string url;
        std::string title;
        double score;
    };
}

std::string BuildSearchHTML(const std::vector<Query::SearchResult>& results);

int main() {
    // Sample results
    std::vector<Query::SearchResult> results = {
        {"https://example.com", "Example Domain", 0.95},
        {"https://openai.com", "OpenAI", 0.90},
        {"https://github.com", "GitHub", 0.89}
    };

    std::string html = BuildSearchHTML(results);

    std::ofstream out("results.html");
    out << html;
    out.close();

    std::cout << "HTML written to results.html" << std::endl;
    return 0;
}

std::string BuildSearchHTML(const std::vector<Query::SearchResult>& results) {
    std::string html =
        "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\" />"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />"
        "<title>Search Results</title>"
        "<link href=\"https://fonts.googleapis.com/css2?family=Inter&display=swap\" rel=\"stylesheet\">"
        "<style>"
        "body {"
        "  margin: 0;"
        "  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;"
        "  background-color: #f7f7f7;"
        "  display: flex;"
        "  justify-content: center;"
        "  padding: 40px;"
        "}"
        ".container {"
        "  text-align: center;"
        "  max-width: 800px;"
        "  width: 100%;"
        "}"
        "h1 {"
        "  font-size: 2.5rem;"
        "  margin-bottom: 2rem;"
        "}"
        "ul {"
        "  list-style-type: none;"
        "  padding: 0;"
        "}"
        "li {"
        "  margin: 15px 0;"
        "  font-size: 1.1rem;"
        "  background: white;"
        "  padding: 15px 20px;"
        "  border-radius: 8px;"
        "  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);"
        "  transition: transform 0.1s ease;"
        "}"
        "li:hover {"
        "  transform: translateY(-2px);"
        "}"
        "a {"
        "  text-decoration: none;"
        "  color: #007BFF;"
        "  display: block;"
        "}"
        "a:hover {"
        "  text-decoration: underline;"
        "}"
        ".back-link {"
        "  display: inline-block;"
        "  margin-top: 2rem;"
        "  font-size: 1rem;"
        "  color: #007BFF;"
        "  text-decoration: none;"
        "}"
        ".back-link:hover {"
        "  text-decoration: underline;"
        "}"
        "</style></head><body><div class=\"container\">"
        "<h1>Search Results</h1><ul>"
        "<a class=\"back-link\" href=\"/\">Back to Home</a>";

    for (const auto& res : results) {
        std::cout << res.url << " " << res.score << std::endl;
        html += "<li><a href=\"" + res.url + "\" target=\"_blank\">" + res.title + "</a></li>";
    }

    html += "</ul>/div></body></html>";
    return html;
}


