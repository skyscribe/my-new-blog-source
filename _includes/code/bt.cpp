#include <signal.h>
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
    printf("Signal caught:%d\n", signo);
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

    //re-throw 
    raise(SIGSEGV);
}


int main(){
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &handleCore;
    act.sa_flags |= SA_RESETHAND; //one-time only
    sigaction(SIGSEGV, &act, NULL);

    outFunc(6);
}
