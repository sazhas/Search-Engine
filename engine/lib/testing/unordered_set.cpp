#include "../UnorderedSet.h"

int main() {
    // Create an unordered set instance to store C-style strings.
    UnorderedSet<const char*> mySet;

    // Initially, the set should be empty.
    assert(mySet.Size() == 0);

    // Test insertion of new elements.
    bool inserted;
    inserted = mySet.Insert("apple");
    assert(inserted && "apple should be inserted for the first time.");

    inserted = mySet.Insert("banana");
    assert(inserted && "banana should be inserted for the first time.");

    inserted = mySet.Insert("cherry");
    assert(inserted && "cherry should be inserted for the first time.");

    // Duplicate insertion should fail.
    inserted = mySet.Insert("apple");
    assert(!inserted && "Duplicate apple should not be inserted.");

    // Check the size is as expected.
    assert(mySet.Size() == 3);

    // Test contains.
    assert(mySet.Contains("apple") && "Set should contain apple.");
    assert(mySet.Contains("banana") && "Set should contain banana.");
    assert(mySet.Contains("cherry") && "Set should contain cherry.");
    assert(!mySet.Contains("date") && "Set should not contain date.");

    // Test erasing an element.
    bool erased = mySet.Erase("banana");
    assert(erased && "banana should be erased successfully.");
    assert(mySet.Size() == 2);
    assert(!mySet.Contains("banana") && "After erasure, set should not contain banana.");

    // Attempt to erase an element that doesn't exist.
    erased = mySet.Erase("banana");
    assert(!erased && "Erasing non-existent element should return false.");

    // Use the iterator to print all remaining elements.
    cout << "Remaining elements in the set:" << endl;
    for (auto it = mySet.begin(); it != mySet.end(); ++it) {
        cout << *it << endl;
    }

    cout << "All tests passed successfully." << endl;
    return 0;
}
