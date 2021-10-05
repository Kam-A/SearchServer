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
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> data;
    };
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");
    struct Access {
    private:
        std::lock_guard<std::mutex> guard;
    public:
        Value& ref_to_value;
        Access(const Key& key,  Bucket& bucket)
                :   guard(bucket.mutex) ,ref_to_value(bucket.data[key]){}
    };

    explicit ConcurrentMap(size_t bucket_count) : buckets(bucket_count) {
    }
    Access operator[](const Key& key) {
        uint64_t bucket_idx  = GetBucketIndex(key);
        return  Access(key,
                       buckets[bucket_idx]);
    }
    void erase(const Key& key){
        uint64_t bucket_idx  = GetBucketIndex(key);
        std::lock_guard<std::mutex> guard(buckets[bucket_idx].mutex);
        buckets[bucket_idx].data.erase(key);
    }
    uint64_t GetBucketIndex(const Key& key){
        return static_cast<uint64_t>(key) % buckets.size();
    }
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> res;
        for (auto& [mutex, data] : buckets) {
            std::lock_guard<std::mutex> guard(mutex);
            res.merge(data);

        }
        return res;
    }
private:
    std::vector<Bucket> buckets;

};