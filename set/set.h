#ifndef SET_H
#define SET_H

#include <unordered_set>
#include <mutex>

class Set
{
private:
    std::unordered_set<int>set;
    std::mutex lock;
public:
    void add(int fd){
        std::unique_lock<std::mutex>Lock(lock);
        set.insert(fd);
    }

    void clean(){
        std::unique_lock<std::mutex>Lock(lock);
        set.clear();
    }

    std::unordered_set<int> getSet(){
        std::unique_lock<std::mutex>Lock(lock);
        
        std::unordered_set<int> tmp = set;

        set.clear();
        return tmp;
    }
};



#endif