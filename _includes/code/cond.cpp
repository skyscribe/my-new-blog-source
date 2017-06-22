#include <cstdlib>
#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>

using namespace std;


struct BoundedBuffer {
    int* buffer;
    int capacity;

    int front;
    int rear;
    int count;

    std::mutex lock;

    std::condition_variable not_full;
    std::condition_variable not_empty;

    BoundedBuffer(int capacity) : capacity(capacity), front(0), rear(0), count(0) {
        buffer = new int[capacity];
    }

    ~BoundedBuffer(){
        delete[] buffer;
    }

    void deposit(int data){
        std::unique_lock<std::mutex> l(lock);

        not_full.wait(l, [&count, &capacity](){return count != capacity; });

        buffer[rear] = data;
        rear = (rear + 1) % capacity;
        ++count;

        not_empty.notify_one();
    }

    int fetch(){
        std::unique_lock<std::mutex> l(lock);

        not_empty.wait(l, [&count](){return count != 0; });

        int result = buffer[front];
        front = (front + 1) % capacity;
        --count;

        not_full.notify_one();

        return result;
    }

    void fetchAllButLastOne(){
        std::unique_lock<std::mutex> l(lock);
        bool should_notify = (count == capacity);
        while (count > 1){
            front = (front + 1) % capacity;
            --count;
        }
        if (should_notify){
            not_full.notify_one();
        } 
    }
};

int main(int argc, const char *argv[])
{
    BoundedBuffer buf(12);    

    std::mutex exitMutex;
    bool terminate = false;

    //producer every 20 senc
    std::thread producer([&](){
        size_t i = 1;
        bool exit;
        while (true){
            {
                std::unique_lock<std::mutex> l(exitMutex);
                exit = terminate;
            }
            if (!exit){
                cout << "deposit " << i << endl;
                buf.deposit(i++);       
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }else{
                break;
            }
        }
    });

    std::thread consumer([&](){
        for (size_t i = 0; i < 20; i++) {
            cout << "Got data:" << buf.fetch() << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        std::unique_lock<std::mutex> l(exitMutex);
        cout << "exiting..." << endl;
        terminate = true;
        buf.fetchAllButLastOne();
    });

    producer.join();
    consumer.join();
    return 0;
}
