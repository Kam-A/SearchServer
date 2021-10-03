#pragma once
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <deque>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <utility>
template <typename Key, typename Value>
class ConcurrentMap {
public:
    int backet_maps_;
    struct Bucket {
        std::mutex bucket_mutex;
        std::map<Key, Value> bucket_;
    };
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");
    struct Access {
    private:
        std::lock_guard<std::mutex> guard;
    public:
        Value& ref_to_value;
        Access(const Key& key,  std::vector<Bucket>& buckets_,int num)
                :   guard(buckets_[num].bucket_mutex) ,ref_to_value(buckets_[num].bucket_[key]){}
    };

    explicit ConcurrentMap(size_t bucket_count) : backet_maps_(bucket_count),sub_dict(bucket_count) {
    }
    Access operator[](const Key& key) {
        int num_  = static_cast<int>(static_cast<uint64_t>(key) % backet_maps_);
        return  Access(key,
                       sub_dict,num_);
    }
    void erase(const Key& key){
        int num_  = static_cast<int>(static_cast<uint64_t>(key) % backet_maps_);
        std::lock_guard<std::mutex> guard(sub_dict[num_].bucket_mutex);
        sub_dict[num_].bucket_.erase(key);
    }
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> res;
        for (auto& dict_ : sub_dict) {
            std::lock_guard<std::mutex> guard(dict_.bucket_mutex);
            res.merge(dict_.bucket_);

        }
        return res;
    }
private:
    std::vector<Bucket> sub_dict;

};