#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <cstddef>
#include <cassert>

#include "vector.h"
#include "algorithm.h"

template <typename T1, typename T2>
struct Cartesian_Pair {
    T1 x; T2 y;
    Cartesian_Pair(const T1& first, const T2& second) :
        x{first},
        y{second}
    {}
};

template <typename T>
class Node_Binary_Backreferencing {
    using Node = Node_Binary_Backreferencing<T>;
public:

    enum class SIDE { LEFT, RIGHT, NONE };

private:    

    Node* parent;
    Node* left;
    Node* right;

    SIDE side_on_parent;
    size_t height;
    T val;

    template <Node* Node::* Child>
    void designate_child(Node* node) {
        assert(node);

        auto& child = this->*Child;

        assert(!child);

        constexpr Node::SIDE side = (Child == &Node::left) ? Node::SIDE::LEFT : Node::SIDE::RIGHT;

        node->parent = this;
        node->side_on_parent = side;
        child = node;
    
        rebalance_height();
    }

    template <Node* Node::* Child>
    void constr_child(const T& val) {
        Node* node = new Node(val);

        designate_child<Child>(node);
    }

    template <Node* Node::* Child>
    Node* sever_child() {
        auto& child = this->*Child;

        assert(child);

        Node* orphan = child;

        child = nullptr;

        rebalance_height();

        orphan->parent = nullptr;
        orphan->side_on_parent = SIDE::NONE;
        orphan->height = 1;
        return orphan;
    }

public:

    void rebalance_height() {
        size_t height_left = left ? left->height : 0;
        size_t height_right = right ? right->height : 0;

        height = (height_left < height_right ? height_right : height_left) + 1;
    }

    Node_Binary_Backreferencing(const T& val) :
        parent{nullptr},
        left{nullptr},
        right{nullptr},
        side_on_parent{SIDE::NONE},
        height{1},
        val{val}
    {}

    Node* get_parent() const { return parent; }

    Node* get_left() const { return left; }

    Node* get_right() const { return right; }

    SIDE get_side_on_parent() const { return side_on_parent; }

    size_t get_height() const { return height; }

    T& get_val() { return val; }

    const T& get_val() const { return val; }

    void designate_child_left(Node* node) {
        designate_child<&Node::left>(node);
    }

    void designate_child_right(Node* node) {
        designate_child<&Node::right>(node);
    }

    void constr_child_left(const T& val) {
        constr_child<&Node::left>(val);
    }

    void constr_child_right(const T& val) {
        constr_child<&Node::right>(val);
    }

    Node* sever_child_left() {
        return sever_child<&Node::left>();
    }

    Node* sever_child_right() {
        return sever_child<&Node::right>();
    }

};

template <typename T, bool (*comp)(const T&, const T&)>
class BST {
    friend class Iterator;
    using Node = Node_Binary_Backreferencing<T>;
private:

    bool lt(const T& a, const T& b) const {
        return comp(a, b);
    }

    bool gt(const T& a, const T& b) const {
        return comp(b, a);
    }

    bool eq(const T& a, const T& b) const {
        return !(lt(a, b) || gt(a, b));
    }

    bool leq(const T& a, const T& b) const {
        return !(gt(a, b));
    }

    bool geq(const T& a, const T& b) const {
        return !(lt(a, b));
    }

    Node* root;
    size_t size;

    void destroy(Node* node) {
        if(!node) return;
        destroy(node->get_left());
        destroy(node->get_right());
        delete node;
    }

    int get_balance(Node* node) {
        int balance = 0;

        if(node->get_left())
            balance -= node->get_left()->get_height();

        if(node->get_right())
            balance += node->get_right()->get_height();

        return balance;
    }

    bool get_balance_state(Node* node) {
        int balance = get_balance(node);

        if(balance < -1 || 1 < balance)
            return false;
        else
            return true;
    }

    /**
     * @brief rotate a subtree designated by a node left or right without regard to balance 
     */
    template <Node* (Node::* Dir)() const, Node* (Node::* Dir_R)() const, Node* (Node::* Sever_Dir)(), Node* (Node::* Sever_Dir_R)(), void (Node::* Designate_Child_Dir)(Node*), void (Node::* Designate_Child_Dir_R)(Node*)>
    void rotate(Node* node) {
        /* obtain the anchor point before we modify node */
        Node* anchor = node->get_parent();
        typename Node::SIDE side = node->get_side_on_parent();

        /* obtain the leading head of the rotation */
        Node* pull = node;

        assert(pull);

        /* points to the proper severance and designation functions */
        Node* (Node::* sev)();
        void (Node::* des)(Node*); 

        if(side == Node::SIDE::LEFT) {
            sev = &Node::sever_child_left;
            des = &Node::designate_child_left;
        } 
        
        else if(side == Node::SIDE::RIGHT) {
            sev = &Node::sever_child_right;
            des = &Node::designate_child_right;
        } 
        
        else
            assert(anchor == nullptr);

        /* sever the correct child which is pull */
        if(anchor)
            (anchor->*sev)();

        /* obtain the new root of this rotational subtree */
        Node* root_new = (node->*Sever_Dir_R)();

        assert(root_new);

        /* obtain the pushing end of the rotation */
        Node* push = (
            ((root_new->*Dir)()) ?
                (root_new->*Sever_Dir)()
            :
                nullptr
        );

        /* delegate the children */
        if(push)
            (pull->*Designate_Child_Dir_R)(push);

        (root_new->*Designate_Child_Dir)(pull);

        /* reassign root_new to anchor as a child if needed */
        if(anchor)
            (anchor->*des)(root_new);
        else
            root = root_new;
    }

    void rotate_left(Node* node) {
        rotate<
            &Node::get_left, 
            &Node::get_right, 
            &Node::sever_child_left, 
            &Node::sever_child_right,
            &Node::designate_child_left,
            &Node::designate_child_right
        >(node);
    }

    void rotate_right(Node* node) {
        rotate<
            &Node::get_right, 
            &Node::get_left, 
            &Node::sever_child_right, 
            &Node::sever_child_left,
            &Node::designate_child_right,
            &Node::designate_child_left
        >(node);
    }

    /**
     * @brief returns whether or not the arm designated by Dir is crinkled
     */
    template <Node* (Node::* Dir)() const, Node* (Node::* Dir_R)() const>
    bool get_crinkled(Node* node) const {
        assert(node);

        Node* anchor = (node->*Dir)();

        if(!anchor) 
            return false;

        Node* in = (anchor->*Dir_R)();

        if(!in)
            return false;

        Node* out = (anchor->*Dir)();

        if(!out)
            return true;

        if(out->get_height() < in->get_height())
            return true;
        else
            return false;
    }

    bool get_crinkled_right(Node* node) const {
        return (
            get_crinkled<
                &Node::get_right,
                &Node::get_left
            >(node)
        );
    }

    bool get_crinkled_left(Node* node) const {
        return (
            get_crinkled<
                &Node::get_left,
                &Node::get_right
            >(node)
        );
    }

    /**
     * @brief properly rotates the subtree designated by node until it maintains the AVL invariant of balance
     */
    void rotate_to_balance(Node* node) {
        assert(node);

        int balance = get_balance(node);

        if(balance < -1) 
        /* left bias, needs right rotation */ {
            Node* left = node->get_left();

            assert(left);

            if(get_crinkled_left(node))
                rotate_left(left);

            rotate_right(node);
        }

        else if(1 < balance) 
        /* right bias, needs left rotation */ {
            Node* right = node->get_right();

            assert(right);

            if(get_crinkled_right(node))
                rotate_right(right);
            
            rotate_left(node);
        }

        else
            return;
    }

    /**
     * @brief starting from this node, successively rebalance it and all of its parents
     */
    void retrace(Node* node) {
        while(node) {
            node->rebalance_height();
            Node* parent = node->get_parent();
            rotate_to_balance(node);
            node = parent;
        }
    }

    /**
     * @brief travels to the proper location for val within the BST and returns either the associated Node or the parent Node and relative side
     * @returns (node_val, side) OR (parent, side)
     */
    Cartesian_Pair<Node*, typename Node::SIDE> traverse_to_val(const T& val) const {
        Node* head = root;
        Node* prev = nullptr;
        /* side of head relative to prev */
        typename Node::SIDE side = Node::SIDE::NONE;

        while(head) {
            prev = head;
            const T& head_val = head->get_val();
            if(lt(head_val, val)) {
                head = head->get_right();
                side = Node::SIDE::RIGHT;
            }
            else if(lt(val, head_val)) {
                head = head->get_left();
                side = Node::SIDE::LEFT;
            }
            else /* head->get_val() == val */
                return Cartesian_Pair(prev, side);
        }

        return Cartesian_Pair(prev, side);
    }

    Node* insert_val(const T& val) {
        Cartesian_Pair<Node*, typename Node::SIDE> pair = traverse_to_val(val);

        Node* parent = pair.x;
        typename Node::SIDE side = pair.y;

        if(parent && eq(parent->get_val(), val))
            return parent;

        Node* node = new Node(val);

        if (side != Node::SIDE::NONE) {
            (side == Node::SIDE::LEFT) ? 
                parent->designate_child_left(node) 
            : 
                parent->designate_child_right(node);
            
            retrace(parent);
        }
        
        else
            root = node;

        ++size;

        return node;
    }

    Node* find_node(const T& val) const {
        Cartesian_Pair<Node*, typename Node::SIDE> pair = traverse_to_val(val);
        if(pair.x && eq(pair.x->get_val(), val))
            return pair.x;
        else
            return nullptr;
    }

    void erase_node(Node* node) {
        assert(false);
        // TODO: wtf
    }

    template<Node* (Node::* Direction)() const>
    Node* get_extrema() const {
        if(!root)
            return nullptr;

        Node* head = root;
        Node* next = (head->*Direction)();
        while(next) {
            head = next;
            next = (head->*Direction)();
        }

        return head;
    }

    Node* get_min_node() const {
        return get_extrema<&Node::get_left>();
    }

    Node* get_max_node() const {
        return get_extrema<&Node::get_right>();
    }

    Node* get_next_node(Node* node) const {
        assert(node);

        if(Node* down = node->get_right()) {

            while(auto left = down->get_left())
                down = left;

            return down;
        }

        else /* we have no right node, must look up the parent chain */ {
            Node* up = node;

            /* try to find a larger element by travelling up the parent chain */
            while(up != root) {
                if(up->get_side_on_parent() == Node::SIDE::LEFT)
                    return up->get_parent();

                up = up->get_parent();
            }

            /* handle no larger element found */
            return nullptr;
        }
    }

    Node* get_prev_node(Node* node) const {
        assert(node);

        if(Node* down = node->get_left()) {

            while(auto right = down->get_right())
                down = right;

            return down;
        }

        else /* we have no left node, must look up the parent chain */ {
            Node* up = node;

            /* try to find a larger element by travelling up the parent chain */
            while(up != root) {
                if(up->get_side_on_parent() == Node::SIDE::RIGHT)
                    return up->get_parent();

                up = up->get_parent();
            }

            /* handle no larger element found */
            return nullptr;
        }
    }

    /* TODO: check AVL invariant - should be good; potentially multithread this */
    static Node* build_bst_from_sorted(const vector<Node*>& arr, int l, int r) {
        if (l > r) return nullptr;
    
        int mid = l + (r - l) / 2;
        Node* root = new Node(arr[mid]);
    
        root->left = build_bst_from_sorted(arr, l, mid - 1);
        root->right = build_bst_from_sorted(arr, mid + 1, r);
    
        return root;
    }

public:

    BST() :
        root{nullptr},
        size{0}
    {}

    BST(const BST& other) = delete;

    BST(BST&& other) :
        root{std::move(other.root)},
        size{other.size}
    {
        other.root = nullptr;
        other.size = 0;
    }

    BST& operator=(const BST& other) = delete;
    BST& operator=(BST&& other) = delete;

    ~BST() {
        destroy(root);
    }

    size_t get_height() const {
        assert(root);

        return root->get_height();
    }

    size_t get_size() const {
        return size;
    }

    bool empty() const {
        return size == 0;
    }

    void absorb(BST&& other) {
        vector<Node*> ours; ours.reserve(get_size());
        while(Node* node = get_min_node(); node != nullptr; node = get_next_node(node))
            ours.pushBack(node);

        vector<Node*> theirs; theirs.reserve(other.get_size());
        while(Node* node = other.get_min_node(); node != nullptr; node = other.get_next_node(node))
            theirs.pushBack(node);

        other.root = nullptr;
        other.size = nullptr;

        vector<Node*> merged(merge_sorted(ours, theirs));

        size = merged.size();
        root = build_bst_from_sorted(merged, 0, merged.size() - 1);
    }

    class Iterator {
        friend class BST;
    private:

        BST* bst;
        Node* data;
        
    public:

        Iterator() :
            bst{nullptr},
            data{nullptr}
        {}

        Iterator(BST* bst, Node* n) :
            bst{bst},
            data{n}
        {}

        T& operator*() {
            assert(data);

            return data->get_val();
        }

        const T& operator*() const {
            assert(data);

            return data->get_val();
        }

        Iterator next() const {
            // assert(data);

            // if(Node* down = data->get_right()) {

            //     while(auto left = down->get_left())
            //         down = left;

            //     return Iterator(bst, down);
            // }

            // else /* we have no right node, must look up the parent chain */ {
            //     Node* up = data;

            //     /* try to find a larger element by travelling up the parent chain */
            //     while(up != bst->root) {
            //         if(up->get_side_on_parent() == Node::SIDE::LEFT)
            //             return Iterator(bst, up->get_parent());

            //         up = up->get_parent();
            //     }

            //     /* handle no larger element found */
            //     return Iterator::end();
            // }

            return Iterator(bst, bst->get_next_node(data));
        }

        Iterator prev() const {
            // assert(data);

            // if(Node* down = data->get_left()) {

            //     while(auto right = down->get_right())
            //         down = right;

            //     return Iterator(bst, down);
            // }

            // else /* we have no left node, must look up the parent chain */ {
            //     Node* up = data;

            //     /* try to find a larger element by travelling up the parent chain */
            //     while(up != bst->root) {
            //         if(up->get_side_on_parent() == Node::SIDE::RIGHT)
            //             return Iterator(bst, up->get_parent());

            //         up = up->get_parent();
            //     }

            //     /* handle no larger element found */
            //     return Iterator::end();
            // }
            
            return Iterator(bst, bst->get_prev_node(data));
        }

        Iterator& operator++() {
            *this = next();
            return *this;
        }

        Iterator& operator--() {
            *this = prev();
            return *this;
        }

        bool operator==(const Iterator& other) const {
            return data == other.data;
        }

        bool operator!=(const Iterator& other) const {
            return !operator==(other);
        }
    };

    Iterator begin() const {
        return Iterator(this, get_min_node());
    }

    Iterator end() const {
        return Iterator(this, nullptr);
    }

    Iterator front() const {
        return begin();
    }

    Iterator back() const {
        return Iterator(this, get_max_node());
    }

    Iterator insert(const T& val) {
        return Iterator(this, insert_val(val));
    }

    Iterator find(const T& val) {
        return Iterator(this, find_node(val));
    }

    Iterator find(const T& val) const {
        return Iterator(this, find_node(val));
    }

    void erase(Iterator it) {
        erase_node(it.data);
    }

};

#endif /* AVL_TREE_H */