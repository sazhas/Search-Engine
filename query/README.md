# README - query

## Initialization

`ip_serv` and `port_serv` are the IP and port of the constraint solver, correspondingly.

Only one instance of the query compiler may exist at once.

```cpp
// ...

Query_Compiler::init_instance(ip_serv, port_serv);
auto* instance = Query_Compiler::get_instance();

// ...
```

## Querying

`query` is a `std::string` following the query format:

- AND: `<T1> & <T2>`
- OR: `<T1> | <T2>`
- NOT: `-<T1>`
- PHRASES: `"<S1> <S2> <...> <SN>"`
- PRECEDENCE: `<T1> (<T2>)`

```cpp
// ...

instance->send_query(query);

// ...

```

The parsed expression is sent to the constraint solver, which will then forward the results elsewhere.

**TODO:**
[ ] incorporate decorators; "The Pistons" -> "title:The Pistons" | "body:The Pistons"