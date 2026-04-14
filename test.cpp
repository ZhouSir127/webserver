#include <semaphore>
#include <iostream>

int main() {
    std::counting_semaphore<10> sem(0);
    sem.release();
    sem.acquire();
    std::cout << "C++20 Semaphore is working!" << std::endl;
    return 0;
}