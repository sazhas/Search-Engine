// Linux tiny HTTP server.
// Nicole Hamilton  nham@umich.edu

// This variation of LinuxTinyServer supports a simple plugin interface
// to allow "magic paths" to be intercepted.  (But the autograder will
// not test this feature.)

// Usage:  LinuxTinyServer port rootdirectory

// Compile with g++ -pthread LinuxTinyServer.cpp -o LinuxTinyServer
// To run under WSL (Windows Subsystem for Linux), may have to elevate
// with sudo if the bind fails.

// LinuxTinyServer does not look for default index.htm or similar
// files.  If it receives a GET request on a directory, it will refuse
// it, returning an HTTP 403 error, access denied.

// It also does not support HTTP Connection: keep-alive requests and
// will close the socket at the end of each response.  This is a
// perf issue, forcing the client browser to reconnect for each
// request and a candidate for improvement.

#include <cassert>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
using namespace std;


// The constructor for any plugin should set Plugin = this so that
// LinuxTinyServer knows it exists and can call it.

#include "Plugin.h"
#include "RootPlugin.h"
PluginObject* Plugin = nullptr;


// Root directory for the website, taken from argv[ 2 ].
// (Yes, a global variable since it never changes.)

char* RootDirectory;


//  Multipurpose Internet Mail Extensions (MIME) types

struct MimetypeMap {
    const char *Extension, *Mimetype;
};

const MimetypeMap MimeTable[] = {
    // List of some of the most common MIME types in sorted order.
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Complete_list_of_MIME_types
    ".3g2",   "video/3gpp2",
    ".3gp",   "video/3gpp",
    ".7z",    "application/x-7z-compressed",
    ".aac",   "audio/aac",
    ".abw",   "application/x-abiword",
    ".arc",   "application/octet-stream",
    ".avi",   "video/x-msvideo",
    ".azw",   "application/vnd.amazon.ebook",
    ".bin",   "application/octet-stream",
    ".bz",    "application/x-bzip",
    ".bz2",   "application/x-bzip2",
    ".csh",   "application/x-csh",
    ".css",   "text/css",
    ".csv",   "text/csv",
    ".doc",   "application/msword",
    ".docx",  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    ".eot",   "application/vnd.ms-fontobject",
    ".epub",  "application/epub+zip",
    ".gif",   "image/gif",
    ".htm",   "text/html",
    ".html",  "text/html",
    ".ico",   "image/x-icon",
    ".ics",   "text/calendar",
    ".jar",   "application/java-archive",
    ".jpeg",  "image/jpeg",
    ".jpg",   "image/jpeg",
    ".js",    "application/javascript",
    ".json",  "application/json",
    ".mid",   "audio/midi",
    ".midi",  "audio/midi",
    ".mpeg",  "video/mpeg",
    ".mpkg",  "application/vnd.apple.installer+xml",
    ".odp",   "application/vnd.oasis.opendocument.presentation",
    ".ods",   "application/vnd.oasis.opendocument.spreadsheet",
    ".odt",   "application/vnd.oasis.opendocument.text",
    ".oga",   "audio/ogg",
    ".ogv",   "video/ogg",
    ".ogx",   "application/ogg",
    ".otf",   "font/otf",
    ".pdf",   "application/pdf",
    ".png",   "image/png",
    ".ppt",   "application/vnd.ms-powerpoint",
    ".pptx",  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
    ".rar",   "application/x-rar-compressed",
    ".rtf",   "application/rtf",
    ".sh",    "application/x-sh",
    ".svg",   "image/svg+xml",
    ".swf",   "application/x-shockwave-flash",
    ".tar",   "application/x-tar",
    ".tif",   "image/tiff",
    ".tiff",  "image/tiff",
    ".ts",    "application/typescript",
    ".ttf",   "font/ttf",
    ".vsd",   "application/vnd.visio",
    ".wav",   "audio/x-wav",
    ".weba",  "audio/webm",
    ".webm",  "video/webm",
    ".webp",  "image/webp",
    ".woff",  "font/woff",
    ".woff2", "font/woff2",
    ".xhtml", "application/xhtml+xml",
    ".xls",   "application/vnd.ms-excel",
    ".xlsx",  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    ".xml",   "application/xml",
    ".xul",   "application/vnd.mozilla.xul+xml",
    ".zip",   "application/zip"
};

const char* Mimetype(const string filename) {
    // Return the Mimetype associated with any extension on the filename.
    const char *begin = filename.c_str(), *p = begin + filename.length() - 1;
    // Scan back from the end for an extension.
    while (p >= begin && *p != '.') p--;
    if (*p == '.') {
        // Found an extension. Binary search for a matching mimetype.
        int i = 0, j = sizeof(MimeTable) / sizeof(MimetypeMap) - 1;
        while (i <= j) {
            int mid = (i + j) / 2, compare = strcasecmp(p, MimeTable[mid].Extension);
            if (compare == 0) return MimeTable[mid].Mimetype;
            if (compare < 0)
                j = mid - 1;
            else
                i = mid + 1;
        }
    }
    // Anything not matched is an "octet-stream", treated as
    // an unknown binary, which browsers treat as a download.
    return "application/octet-stream";
}


int HexLiteralCharacter(char c) {
    // If c contains the Ascii code for a hex character, return the
    // binary value; otherwise, -1.
    int i;
    if ('0' <= c && c <= '9')
        i = c - '0';
    else if ('a' <= c && c <= 'f')
        i = c - 'a' + 10;
    else if ('A' <= c && c <= 'F')
        i = c - 'A' + 10;
    else
        i = -1;
    return i;
}


string UnencodeUrlEncoding(string& path) {
    // Unencode any %xx encodings of characters that can't be
    // passed in a URL.
    // (Unencoding can only shorten a string or leave it unchanged.
    // It never gets longer.)
    const char *start = path.c_str(), *from = start;
    string result;
    char c, d;
    while ((c = *from++) != 0)
        if (c == '%') {
            c = *from;
            if (c) {
                d = *++from;
                if (d) {
                    int i, j;
                    i = HexLiteralCharacter(c);
                    j = HexLiteralCharacter(d);
                    if (i >= 0 && j >= 0) {
                        from++;
                        result += (char) (i << 4 | j);
                    } else {
                        // If the two characters following the %
                        // aren't both hex digits, treat as
                        // literal text.
                        result += '%';
                        from--;
                    }
                }
            }
        } else
            result += c;
    return result;
}


bool SafePath(const char* path) {
    // Watch out for paths containing .. segments that
    // attempt to go higher than the root directory
    // for the website.

    // Should start with a /.
    if (*path != '/') return false;

    int depth = 0;
    for (const char* p = path + 1; *p;)
        if (strncmp(p, "../", 3) == 0) {
            depth--;
            if (depth < 0)
                return false;
            else
                p += 3;
        } else {
            for (p++; *p && *p != '/'; p++)
                ;
            if (*p == '/') {
                depth++;
                p++;
            }
        }
    return true;
}


off_t FileSize(int f) {
    // Return -1 for directories.
    struct stat fileInfo;
    fstat(f, &fileInfo);
    if ((fileInfo.st_mode & S_IFMT) == S_IFDIR) return -1;
    return fileInfo.st_size;
}


void AccessDenied(int talkSocket) {
    const char accessDenied[]
      = "HTTP/1.1 403 Access Denied\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    cout << accessDenied;
    // Use our helper function to ensure all data is sent.
    // (We assume the length is sizeof(accessDenied)-1.)
    ssize_t total = 0;
    while (total < (ssize_t) (sizeof(accessDenied) - 1)) {
        ssize_t n = send(talkSocket, accessDenied + total, (sizeof(accessDenied) - 1) - total, 0);
        if (n <= 0) break;
        total += n;
    }
}


void FileNotFound(int talkSocket) {
    const char fileNotFound[]
      = "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    cout << fileNotFound;
    ssize_t total = 0;
    while (total < (ssize_t) (sizeof(fileNotFound) - 1)) {
        ssize_t n = send(talkSocket, fileNotFound + total, (sizeof(fileNotFound) - 1) - total, 0);
        if (n <= 0) break;
        total += n;
    }
}


// *******************
// Helper Functions for Sending and Receiving All Data
// *******************

// Helper function: send all data from buffer
bool send_all(int sock, const char* data, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(sock, data + total_sent, length - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += sent;
    }
    return true;
}

// Helper function: send all data from a std::string
bool send_data(int sock, const string& data) {
    return send_all(sock, data.c_str(), data.size());
}

// Helper function: receive all data until a specified length is reached.
ssize_t recv_all(int sock, char* buffer, size_t length) {
    ssize_t total_received = 0;
    do {
        auto received = recv(sock, buffer + total_received, length - total_received - 1, 0);

        if (received <= 0) break;
        total_received += received;
        buffer[total_received] = '\0';

        if (!strcmp(&buffer[total_received - 4], "\r\n\r\n")) break;
    } while (total_received < length - 1);
    return total_received;
}


// *******************
// Thread function to handle a client connection (talk socket).
// *******************
void* Talk(void* talkSocket) {
    // TO DO:  Look for a GET message, then reply with the
    // requested file.
    // Cast from void * to int * to recover the talk socket id
    // then delete the copy passed on the heap.
    int* pTalkSock = (int*) talkSocket;
    int sock = *pTalkSock;
    delete pTalkSock;

    // Read the request from the socket and parse it to extract
    // the action and the path, unencoding any %xx encodings.
    const int BUFSIZE = 8192;
    char buf[BUFSIZE];
    int n = recv_all(sock, buf, BUFSIZE - 1);   // using our recv_all helper for simplicity
    if (n <= 0) {
        close(sock);
        return nullptr;
    }
    buf[n] = '\0';
    string request(buf);

    cout << "[LOG] Full HTTP request received:\n" << request << endl;

    // Check that the request is a GET request.
    if (request.substr(0, 4) != "GET ") {
        close(sock);
        return nullptr;
    }

    // Extract the path from the request ("GET /path HTTP/1.1")
    size_t start = 4;
    size_t end = request.find(' ', start);
    if (end == string::npos) {
        close(sock);
        return nullptr;
    }
    string path = request.substr(start, end - start);

    // Unencode any %xx encodings in the path.
    string decodedPath = UnencodeUrlEncoding(path);

    // Check to see if there's a plugin and, if there is,
    // whether this is a magic path intercepted by the plugin.
    if (Plugin && Plugin->MagicPath(decodedPath)) {
        cout << "INTERCEPTED\n";
        string pluginResponse = Plugin->ProcessRequest(request);
        send_all(sock, pluginResponse.c_str(), pluginResponse.size());
        close(sock);
        return nullptr;
    }

    // If it isn't intercepted, action must be "GET" and
    // the path must be safe.
    if (!SafePath(decodedPath.c_str())) {
        AccessDenied(sock);
        close(sock);
        return nullptr;
    }

    // If the path refers to a directory, access denied.
    if (decodedPath.back() == '/') {
        AccessDenied(sock);
        close(sock);
        return nullptr;
    }

    // If the path refers to a file, write it to the socket.
    // Construct the full file path.
    string fullPath = string(RootDirectory) + decodedPath;

    // Open the file.
    int fd = open(fullPath.c_str(), O_RDONLY);
    if (fd < 0) {
        FileNotFound(sock);
        close(sock);
        return nullptr;
    }

    // Check if the path refers to a directory.
    struct stat st;
    fstat(fd, &st);
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
        AccessDenied(sock);
        close(fd);
        close(sock);
        return nullptr;
    }

    // Get the file size.
    off_t size = FileSize(fd);
    if (size < 0) {
        FileNotFound(sock);
        close(fd);
        close(sock);
        return nullptr;
    }

    // Prepare HTTP response header.
    string header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: ";
    header += Mimetype(fullPath);
    header += "\r\n";
    header += "Content-Length: " + to_string(size) + "\r\n";
    header += "Connection: close\r\n\r\n";

    cout << "[LOG] Serving file: " << fullPath << " with size: " << size << " bytes\n";

    // Send the header using our helper.
    if (!send_data(sock, header)) {
        close(fd);
        close(sock);
        return nullptr;
    }

    // Send the file content.
    char fileBuffer[4096];
    int bytesRead;
    while ((bytesRead = read(fd, fileBuffer, sizeof(fileBuffer))) > 0) {
        if (!send_all(sock, fileBuffer, bytesRead)) break;
    }

    close(fd);
    close(sock);
    return nullptr;
}


int main(int argc, char** argv) {
    if (argc < 5 || ((argc - 3) % 2 != 0)) {
        cerr << "Usage: " << argv[0] << " listen_port root_directory [qc_ip qc_port]..." << endl;
        return 1;
    }

    int port = atoi(argv[1]);
    RootDirectory = argv[2];
    vector<Query::CSolverInfo> qcEndpoints;
    for (int i = 3; i < argc; i += 2) {
        string ip = argv[i];
        uint16_t qp = static_cast<uint16_t>(atoi(argv[i + 1]));
        qcEndpoints.emplace_back(ip, qp);
    }


    // Remove trailing slash from RootDirectory if present.
    char* r = RootDirectory;
    if (*r) {
        do {
            r++;
        } while (*r);
        r--;
        if (*r == '/') *r = '\0';
    }

    RootPlugin rootPlugin(qcEndpoints);
    Plugin = &rootPlugin;

    // We'll use two sockets, one for listening for new
    // connection requests, the other for talking to each
    // new client.
    int listenSocket, talkSocket;

    // Create socket address structures to go with each
    // socket.
    struct sockaddr_in listenAddress, talkAddress;
    socklen_t talkAddressLength = sizeof(talkAddress);
    memset(&listenAddress, 0, sizeof(listenAddress));
    memset(&talkAddress, 0, sizeof(talkAddress));

    // Fill in details of where we'll listen.
    // We'll use the standard internet family of protocols.
    listenAddress.sin_family = AF_INET;
    // htons( ) transforms the port number from host (our)
    // byte-ordering into network byte-ordering (which could
    // be different).
    listenAddress.sin_port = htons(port);
    // INADDR_ANY means we'll accept connections to any IP
    // assigned to this machine.
    listenAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // TO DO:  Create the listenSocket, specifying that we'll r/w
    // it as a stream of bytes using TCP/IP.
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        cerr << "Failed to create listen socket" << endl;
        return 1;
    }

    const int enable = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // TO DO:  Bind the listen socket to the IP address and protocol
    // where we'd like to listen for connections.
    if (::bind(listenSocket, (struct sockaddr*) &listenAddress, sizeof(listenAddress)) < 0) {
        cerr << "Failed to bind listen socket" << endl;
        return 1;
    }

    // TO DO:  Begin listening for clients to connect to us.
    if (listen(listenSocket, SOMAXCONN) < 0) {
        cerr << "Failed to listen on socket" << endl;
        return 1;
    }

    cout << "LinuxTinyServer is running on port " << port << endl;

    // TO DO;  Accept each new connection and create a thread to talk with
    // the client over the new talk socket that's created by Linux
    // when we accept the connection.
    {
        while (true) {
            // Accept new connection.
            talkSocket = accept(listenSocket, (struct sockaddr*) &talkAddress, &talkAddressLength);
            if (talkSocket < 0) {
                cerr << "Accept failed" << endl;
                continue;
            }

            // TO DO:  Create and detach a child thread to talk to the
            // client using pthread_create and pthread_detach.
            // When creating a child thread, you get to pass a void *,
            // usually used as a pointer to an object with whatever
            // information the child needs.
            //
            // The talk socket is passed on the heap rather than with a
            // pointer to the local variable because we're going to quickly
            // overwrite that local variable with the next accept( ).
            // Since this is multithreaded, we can't predict whether the child will
            // run before we do that.  The child will be responsible for
            // freeing the resource.  We do not wait for the child thread
            // to complete.
            //
            // (A simpler alternative in this particular case would be to
            // cast the int talkSocket to a void *, knowing that a void *
            // must be at least as large as the int.  But that would not
            // demonstrate what to do in the general case.)
            int* pTalkSocket = new int;
            *pTalkSocket = talkSocket;
            pthread_t threadId;
            if (pthread_create(&threadId, nullptr, Talk, pTalkSocket) != 0) {
                cerr << "Failed to create thread" << endl;
                close(talkSocket);
                delete pTalkSocket;
            } else {
                pthread_detach(threadId);
            }
        }
    }

    close(listenSocket);
    return 0;
}
