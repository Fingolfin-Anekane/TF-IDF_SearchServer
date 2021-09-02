#pragma once
#include <cstdlib>
#include <future>
#include <map>
#include <string>
#include <vector>



template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        Access(std::mutex& mux, std::map<Key, Value>& mp, const Key& key)
            :mux_link_((mux.lock(), mux)),
            ref_to_value(mp[key])
        {
        }

        ~Access() {
            mux_link_.unlock();
        }

    private:
        std::mutex& mux_link_;
    public:
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) {
        map_vec_ = std::vector<std::map<Key, Value>>(bucket_count);
        mux_vec_ = std::vector<std::mutex>(bucket_count);
    }

    Access operator[](const Key& key) {
        return Access(mux_vec_[static_cast<uint64_t>(key) % map_vec_.size()], map_vec_[static_cast<uint64_t>(key) % map_vec_.size()], key);
    }

    void Erase(const Key& key) {
        uint64_t ukey = static_cast<uint64_t>(key) % map_vec_.size();
        std::lock_guard guard(mux_vec_[ukey]);
        map_vec_[ukey].erase(key);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        std::map<Key, Value> map_buf;
        int i = 0;
        for (const auto& mp : map_vec_) {
            mux_vec_[i].lock();
            map_buf = mp;
            mux_vec_[i].unlock();
            result.merge(map_buf);
            ++i;
        }
        return result;
    }

private:
    std::vector<std::map<Key, Value>> map_vec_;
    std::vector<std::mutex> mux_vec_;
};


template <typename T>
class Synchronized {
public:
    explicit Synchronized(T initial = T())
        :value_(initial)
    {}

    struct Access {
        friend class Synchronized<T>;
        Access(Synchronized<T>* sync)
            :ref_to_value(sync->value_),
            mut(sync->value_mutex_)
        {
            mut.lock();
        }
        ~Access() {
            mut.unlock();
        }
        T& ref_to_value;
    private:
        std::mutex& mut;
    };

    Access GetAccess() {
        return Access(this);
    }

private:
    T value_;
    std::mutex value_mutex_;
};