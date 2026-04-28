#ifndef DEATH_H
#define DEATH_H

#include <unordered_set>
#include <mutex>

class Death
{
private:
    std::unordered_set<int>deathSet;
    std::mutex lock;
public:
    void add(int fd){
        std::unique_lock<std::mutex>Lock(lock);
        deathSet.insert(fd);
    }

    void clean(){
        std::unique_lock<std::mutex>Lock(lock);
        deathSet.clear();
    }

    std::unordered_set<int> getDeath(){
        std::unique_lock<std::mutex>Lock(lock);
        
        std::unordered_set<int> tmp = deathSet;

        deathSet.clear();
        return tmp;
    }
};



#endif