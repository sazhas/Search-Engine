# README - csolver

## Initialization

`ip_self` and `port_self` are the IP and the port to listen on. `blob` is an `IndexBlob*` that points to the `IndexBlob` to be searched.

```cpp
// ...

CSolver::Init(ip_self, port_self, blob);

auto* csolver = CSolver::GetInstance();

// ...
```

## Usage

```cpp
// ...

csolver->serve_requests();

// ...
```

### `CSolver::serve_requests()`

We're constructing an `Expr_AST ast` which represents the user's query. `ast` directly reads from the socket for efficiency. It is then used to construct an `ISR_Tree isr_tree`, which is a tree structure of `ISR`s that will do the actual searching via the `IndexBlob*`. 

**TODO:** 

- [ ] use docEndISR to only obtain one matching post per page
- [ ] derived ISR functionality is not complete and we are unsure of the interface
    - [ ] merge isr_copy.h with isr.h
    - [ ] implement derived ISR virtual functions
- [ ] agree on the format of the results and how to pass them to the ranker

