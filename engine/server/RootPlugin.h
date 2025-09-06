#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>

#include "../lib/algorithm.h"
#include "../query/query.h"
#include "../ranker/Ranker.hpp"
#include "Plugin.h"

// Helper function to get file size
static off_t GetFileSize(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    return st.st_size;
}


class RootPlugin : public PluginObject {
private:
    Query::Query_Compiler* compiler;

public:
    /**
     * @param ip_serv IP address of constraint solver
     * @param port_serv port of constraint solver
     */
    RootPlugin(const std::vector<Query::CSolverInfo>& endpoints)
        : compiler { nullptr } {
        Plugin = this;
        Query::Query_Compiler::init_instance(endpoints,
                                             "/Users/anaym/Documents/search engine/engine/server/synsets.txt");
        compiler = Query::Query_Compiler::get_instance();
    }

    bool MagicPath(const std::string path) override {
        return path == "/" || path.substr(0, 8) == "/search?" || path == "/logo.svg";
    }

    std::string ProcessSearch(std::string& query) {
        std::cout << "SENDING QUERY\n";
        std::vector<Query::SearchResult> results = compiler->send_query(query);

        // Builds entire html file in another function
        std::string html = BuildSearchHTML(results);

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(html.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += html;
        return response;
    }

    std::string ProcessIndex() {
        const char* filename = "index.html";

        int fd = open(filename, O_RDONLY);
        if (fd < 0) return "HTTP/1.1 404 Not Found\r\n\r\n";

        off_t size = GetFileSize(fd);
        if (size < 0) {
            close(fd);
            return "HTTP/1.1 404 Not Found\r\n\r\n";
        }

        std::string content;
        content.resize(size);
        ssize_t bytesRead = read(fd, &content[0], size);
        close(fd);

        if (bytesRead != size) return "HTTP/1.1 500 Internal Server Error\r\n\r\n";

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += content;

        return response;
    }

    std::string ProcessRequest(std::string request) override {
        // Extract the path from the request
        size_t pathStart = request.find("GET ") + 4;
        size_t pathEnd = request.find(" HTTP/");
        std::string path = request.substr(pathStart, pathEnd - pathStart);
        // std::cout<<"RECIEVED REQUEST" << path << "\n";

        // Handle search result injection
        if (path.substr(0, 10) == "/search?q=") {
            // std::cout<<"RECIEVED SEARCH REQUEST";
            std::string query = path.substr(10);
            to_lowercase(query);
            std::cout << query;
            return ProcessSearch(query);
        }

        if (path == "/logo.svg") {
            std::cout << "RECIEVED LOGO REQUEST";
            int fd = open("logo.svg", O_RDONLY);
            if (fd < 0) return "HTTP/1.1 404 Not Found\r\n\r\n";
            off_t size = GetFileSize(fd);
            std::string content(size, 0);
            read(fd, &content[0], size);
            close(fd);

            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: image/svg+xml\r\n";
            response += "Content-Length: " + std::to_string(size) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += content;
            return response;
        }

        return ProcessIndex();
    }

protected:
    std::string BuildSearchHTML(std::vector<Query::SearchResult>& results) {
        std::string html
          = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\" />"
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
            ".logo {"
            "    position: fixed;"
            "    top: 0px;"
            "    left: 75px;"
            "    width: 200px;"
            "    height: 200px;"
            "    z-index: 999;"
            "  }"
            "</style></head><body><div class=\"container\">"
            "<h1>Search Results</h1>"
            "<img src=\"/logo.svg\" alt=\"Logo\" class=\"logo\">"
            "<a class=\"back-link\" href=\"/\">Back to Home</a>"
            "<br>"
            "<ul>";
        ;
        ;
        ;
        ;

        // std::vector<Query::SearchResult> results = {
        //     {"https://example.com", "Example Domain", 0.95},
        //     {"https://openai.com", "OpenAI", 0.90},
        //     {"https://github.com", "GitHub", 0.89}
        // };

        for (const auto& res : results) {
            std::cout << res.url << " " << res.score << std::endl;
            html += "<li><a href=\"" + res.url + "\" target=\"_blank\">" + res.title + "</a></li>";
        }

        html += "</ul></div></body></html>";
        return html;
    }
};
