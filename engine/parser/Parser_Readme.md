# Compiling

To compile the parser, use the following gcc command:

```bash
g++ -g -std=c++17 Parser.cpp HtmlParser.cpp HtmlTags.cpp -pthread -lcrypto -o html_parser
```

# Running
Finally run the parser with:
```bash
./html_parser
```

This should be done before running the http crawler.
