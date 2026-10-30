#include <catch2/catch_test_macros.hpp>
#include <keet/deep_copy_ptr.hpp>
#include <string>

// Test recursive node structure matching the spec:
// class Node { let val: int; let next: Node | null; }
struct Node {
    int val{0};
    keet::deep_copy_ptr<Node> next{nullptr};

    Node() = default;
    explicit Node(int v) : val(v), next(nullptr) {}
    Node(int v, keet::deep_copy_ptr<Node> n) : val(v), next(std::move(n)) {}

    bool operator==(const Node& other) const {
        return val == other.val && next == other.next;
    }
};

TEST_CASE("keet::deep_copy_ptr basic creation and dereference", "[runtime]") {
    keet::deep_copy_ptr<int> pNull;
    REQUIRE_FALSE(pNull);
    REQUIRE(pNull == nullptr);
    REQUIRE(pNull.get() == nullptr);

    keet::deep_copy_ptr<int> pVal(42);
    REQUIRE(pVal);
    REQUIRE(*pVal == 42);

    *pVal = 100;
    REQUIRE(*pVal == 100);

    pVal = nullptr;
    REQUIRE_FALSE(pVal);
}

TEST_CASE("keet::deep_copy_ptr deep-copy semantics on assignment and copy constructor", "[runtime]") {
    keet::deep_copy_ptr<std::string> original("hello");
    keet::deep_copy_ptr<std::string> clone = original;

    REQUIRE(clone);
    REQUIRE(*clone == "hello");

    // Deep copy guarantees that modifying the clone does NOT mutate the original:
    *clone = "world";
    REQUIRE(*clone == "world");
    REQUIRE(*original == "hello");
    REQUIRE(clone.get() != original.get()); // Disjoint addresses
}

TEST_CASE("keet::deep_copy_ptr move semantics", "[runtime]") {
    keet::deep_copy_ptr<std::string> p1("keetscript");
    const std::string* addr = p1.get();

    keet::deep_copy_ptr<std::string> p2 = std::move(p1);
    REQUIRE(p2);
    REQUIRE(*p2 == "keetscript");
    REQUIRE(p2.get() == addr); // Ownership transferred without reallocation
    REQUIRE_FALSE(p1);         // Moved-from is empty
}

TEST_CASE("keet::deep_copy_ptr recursive data structures (LinkedList)", "[runtime]") {
    // Construct linked list: 1 -> 2 -> 3
    Node list1(1, keet::make_deep_copy<Node>(2, keet::make_deep_copy<Node>(3)));

    REQUIRE(list1.val == 1);
    REQUIRE(list1.next->val == 2);
    REQUIRE(list1.next->next->val == 3);
    REQUIRE(list1.next->next->next == nullptr);

    // Deep copy of the entire linked chain
    Node list2 = list1;
    REQUIRE(list2 == list1);

    // Mutate node in the copied list
    list2.next->val = 99;
    REQUIRE(list2.next->val == 99);
    REQUIRE(list1.next->val == 2); // Original untouched!
}
