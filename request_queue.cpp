#include "request_queue.h"
using namespace std;
RequestQueue::RequestQueue(const SearchServer& search_server) :
    search_server_(search_server),time_(0),empty_req_num_(0) {
}
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    vector<Document> res = search_server_.FindTopDocuments(raw_query, status);
    time_incr_and_check(res);
    return res;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    vector<Document> res = search_server_.FindTopDocuments(raw_query);
    time_incr_and_check(res);
    return res;
}
int RequestQueue::GetNoResultRequests() const {
    return empty_req_num_;
}
void RequestQueue::time_incr_and_check(std::vector<Document> resp){
    ++time_;
    if (time_ > sec_in_day_){
        QueryResult old_resp = requests_.front();
        requests_.pop_front();
        if (old_resp.is_empty){
            --empty_req_num_;
        }
    }
    if (resp.size()==0){
        requests_.push_back({true});
        ++empty_req_num_;
    }else{
        requests_.push_back({false});
    }
}
