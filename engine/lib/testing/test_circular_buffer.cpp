#include <cassert>

#include <iostream>

#include "../circular_buffer.h"

void test_single_threaded() {

    Circular_Buffer<size_t> cbuf{2};

    assert(
        cbuf.empty() &&
        cbuf.size() == 0 &&
        !cbuf.full()
    );

    cbuf.insert_tail_blocking(1);
    cbuf.insert_tail_blocking(2);

    assert(cbuf.pop_and_get_front() == 1);
    assert(cbuf.pop_and_get_front() == 2);

    cbuf.insert_tail_resizing(3);
    cbuf.insert_tail_resizing(4);

    assert(cbuf.pop_and_get_front() == 3);
    assert(cbuf.pop_and_get_front() == 4);

    auto pred = [](const size_t& t) {
        std::cout << "I am " << t << '\n';
        return true;
    };

    assert(
        cbuf.size() == 0 &&
        !cbuf.full() && 
        cbuf.empty()
    );

    cbuf.insert_tail_resizing(5);
    cbuf.insert_tail_resizing(6);

    assert(
        cbuf.size() == 2 &&
        cbuf.full() &&
        !cbuf.empty()
    );

    cbuf.insert_tail_resizing(7);

    assert(
        cbuf.size() == 3 &&
        !cbuf.full() &&
        !cbuf.empty()
    );

    assert(cbuf.pop_and_get_front_if_pred(pred).get() == 5);
    assert(cbuf.pop_and_get_front_if_pred(pred).get() == 6);
    assert(cbuf.pop_and_get_front_if_pred(pred).get() == 7);

    assert(
        cbuf.size() == 0 &&
        !cbuf.full() && 
        cbuf.empty()
    );

}

void* test_multi_threaded_aux0(void* cbuf$) {
    Circular_Buffer<size_t>* cbuf = static_cast<Circular_Buffer<size_t>*>(cbuf$);

    for(size_t i = 0; i < 4; ++i) {
        auto v = cbuf->pop_and_get_front();
        std::cout << "popped " << v << '\n';
    }

    return nullptr;
}

void test_multi_threaded() {

    { /* blocking inserts (t0) + pops (t1) */
        Circular_Buffer<size_t> cbuf{2};

        pthread_t t1;
        pthread_attr_t attr1;

        pthread_attr_init(&attr1);
        pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_JOINABLE);

        int ret1 = pthread_create(&t1, &attr1, test_multi_threaded_aux0, (void*)&cbuf);

        cbuf.insert_tail_blocking(1);
        std::cout << "inserted 1\n";

        cbuf.insert_tail_blocking(2);
        std::cout << "inserted 2\n";

        cbuf.insert_tail_blocking(3);
        std::cout << "inserted 3\n";

        cbuf.insert_tail_blocking(4);
        std::cout << "inserted 4\n";
        
        pthread_join(t1, nullptr);
        pthread_attr_destroy(&attr1);
    }
}

int main() {

    test_single_threaded();
    test_multi_threaded();

}
