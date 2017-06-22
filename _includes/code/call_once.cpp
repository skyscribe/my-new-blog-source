#include <cstdlib>
#include <iostream>
#include <thread>

using namespace std;

once_flag flag;

void someComplicatedThings()
{
    call_once(flag, [](){
        cout << "called only once " << endl;
    });

    std::cout << "Called each time!" << endl;
}

int main(int argc, const char *argv[])
{
    thread t1(someComplicatedThings);
    thread t2(someComplicatedThings);
    thread t3(someComplicatedThings);
    thread t4(someComplicatedThings);
    thread t5(someComplicatedThings);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    return 0;
}
