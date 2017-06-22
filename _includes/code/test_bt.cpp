#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include <thread>

using namespace std;

int faultOp(){
    char* addrPtr = reinterpret_cast<char*>(0x1);
    cout << (*addrPtr) << endl;
}

int outFunc(int num){
    if (num > 2) 
        outFunc(num-1);
    faultOp();
}


void handleCore(int signo){
    printf("Signal caught:%d, dumping backtrace...\n", signo);

    char* stack[20] = {0};
    int depth = backtrace(reinterpret_cast<void**>(stack), sizeof(stack)/sizeof(stack[0]));
    if (depth){
        char** symbols = backtrace_symbols(reinterpret_cast<void**>(stack), depth);
        if (symbols){
            for(size_t i = 0; i < depth; i++){
                printf("===[%d]:%s\n", (i+1), symbols[i]);
            }
        }
        free(symbols);
    }

    sleep(3);
    //re-throw 
    raise(SIGSEGV);
}

void normalOp(){
    cout << "THREAD-" << std::this_thread::get_id() << ": normal operation begin..." << endl;
    sleep(2);
    cout << "THREAD-" << std::this_thread::get_id() << ": normal operation end..." << endl;
}

void threadFunc(size_t id){
    if (id == 1){
        cout << "THREAD-" << std::this_thread::get_id() << ":bad operation follows..." << endl;
        outFunc(6);
    }else{
        normalOp();
    }
}

int main(){
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &handleCore;
    act.sa_flags |= SA_RESETHAND; //one-time only
    sigaction(SIGSEGV, &act, NULL);

    thread t1(bind(&threadFunc, 1));
    thread t2(bind(&threadFunc, 2));
    thread t3(bind(&threadFunc, 3));
    cout << "Threads started!" << endl;

    t1.join();
    t2.join();
    t3.join();
    //below prints will never get dumped
    cout << "All threads exited!" << endl;
