#ifndef SET_H
#define SET_H

#include <limits>
#include <cstddef>

#include "vector.h"
#include "avl_tree.h"
#include "mutex.h"

template <typename Obj_Caller$>
size_t get_num_and_incr_while_locked(size_t& n, Mutex& mut, Obj_Caller$ ptr) {
    Lock_Guard<Mutex, &Mutex::lock> g{mut};

    if(id_latest < std::numeric_limits<size_t>::max())
        return n++;
    else
        throw std::runtime_error("Set::Bucket::id_latest overflow by " + ptr);
}

/**
 * @brief concurrency-safe dynamic set that supports writing contiguously to memory
 */
template <typename T, bool (*comp)(const T&, const T&), size_t (*hash)(const T&)>
class Set {
private:

    class Bucket {
    private:

        using BST_T = BST<T, comp>;

        struct Wrap {

            /* the dividend */
            size_t val;

            BST_T bst;

            Wrap(size_t dividend) :
                val{dividend},
                bst{}
            {}

            /* leave bst uninitialized is fine as we only ever copy Wraps with empty bsts */
            Wrap(const Wrap& other) :
                val{other.val},
                bst{}
            {}

        };

        static bool comp_Wrap(const Wrap& w1, const Wrap& w2) {
            return w1.dividend < w2.dividend;
        }

        using BST_Wrap = BST<Wrap, &comp_Wrap>;

        /* WARNING: if scaled large, this could overflow! */
        inline static size_t id_latest = 0;
        static Mutex mut_id_latest;

        /* the hash value corresponding to this bucket; the idx within the overarching table */
        size_t id;

        BST_Wrap bst;

        mutable Mutex_Shared mut_bkt;

        static size_t get_id_latest_and_incr() {
            return get_num_and_incr_while_locked(id_latest, mut_id_latest, this);
        }

    public:

        Bucket() :
            id{get_id_latest_and_incr()},
            bst{BST()}
        {}

        ~Bucket() {}

        size_t get_size() const {
            Lock_Guard_Read g{mut_bkt};
            return bst.get_size();
        }

        /**
         * @param dividend hash(t) / table.size()
         */
        void insert(const T& t, size_t dividend) {
            Lock_Guard_Write g{mut_bkt};
            // TODO: figure out how to lock per-Wrap

            const Wrap tmp(dividend);

            auto wrap = bst.find(tmp);

            /* case where we have a corresponding Wrap */
            if(wrap != bst.end())
                (*wrap).bst.insert(t);
            
            /* case where we need to insert a new Wrap */
            else
                bst.insert(tmp);
        }

        class Iterator {
            friend class Bucket;
        private:

            Bucket* bucket;
            BST_Wrap::Iterator it_Wrap;
            BST_T::Iterator it_T;

        public:

            Iterator(Bucket* bucket) :
                bucket{bucket}
            {}

            const T& operator*() const {
                return *(it_T);
            }

            Iterator& operator++() {
                /* TODO */
            }

            Iterator& operator--() {
                /* TODO */
            }

        }; /* class Iterator */

        /* TODO: find function and associates */

    }; /* class Bucket */

    constexpr static const size_t SIZE_INIT = 1 << 12; // 4096
    constexpr static const double LOAD_OPTIMAL = 0.75; // for AVL-based collision-reduction

    /* WARNING: if scaled large, this could overflow! */
    inline static size_t id_latest = 0;
    static Mutex mut_id_latest;

    const size_t id;

    vector<Bucket> table;

    /* number of elements */
    size_t num_elems;

    mutable Mutex_Shared mut_all;

    static size_t get_id_latest_and_incr() {
        return get_num_and_incr_while_locked(id_latest, mut_id_latest, this);
    }

    size_t get_num_buckets_nonlocking() const {
        return table.size();
    }

    Cartesian_Pair<size_t, size_t> get_idx_table_and_wrap_nonlocking(size_t val_hash) const {
        size_t num_buckets = get_num_buckets_nonlocking();
        return Cartesian_Pair{val_hash / num_buckets, val_hash % num_buckets};
    }

    double get_load_nonlocking() const {
        return num_elems / get_num_buckets_nonlocking();
    }

    void rehash_nonlocking() {
        /* TODO: remember to 2x the size and do the modulus magickery */
    }

public:

    Set() :
        id{get_id_latest_and_incr()},
        table{SIZE_INIT},
        num_elems{0}
    {}

    ~Set() {}

    size_t size() const {
        Lock_Guard_Read g{mut_all};
        return num_elems;
    }

    void insert(const T& val) {
        bool flag_rehash = false;
        {
            Lock_Guard_Read g{mut_all};
            if(LOAD_OPTIMAL < get_load_nonlocking())
                flag_rehash = true;
        }

        // lock to ensure that the insert inducing the resize is the one that inserts first
        static Mutex mut_local;
        Lock_Guard<Mutex, &Mutex::lock> g_local{mut_local}; g_local.unlock();

        if(flag_rehash) {
            Lock_Guard_Write g{mut_all};

            /// check again as we lose the lock between the last lock and now
            if(LOAD_OPTIMAL < get_load_nonlocking()) {
                g_local.lock();
                rehash_nonlocking();
            }
        }
        
        size_t val_hash = hash(val);

        /* TODO: find a way to not lock here, especially since we now have per-wrap locking s*/
        Lock_Guard_Read g{mut_all};

        auto idx_table_and_wrap = get_idx_table_and_wrap_nonlocking(val_hash);
        const auto& idx_table = idx_table_and_wrap.x;
        const auto& wrap = idx_table_and_wrap.y;

        table[idx_table].insert(val, wrap);

        // unlock g_local via RAII
    }

    /* TODO: iterators */

    /* TODO: find function and associates */

    static Set union_a_b(const Set& a, const Set& b) {

    }

}; /* class Set */

#endif /* SET_H */
