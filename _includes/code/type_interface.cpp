#include <cstdlib>
#include <iostream>
#include <vector>
#include <tr1/functional>

using namespace std;
using namespace std::tr1;
using std::tr1::placeholders::_1;
using std::tr1::placeholders::_2;

bool func(int a, double b, std::string c, int d){
    cout << "called with:" << a << "," << b << "," << c << "," << d << endl;
    return true;
}


int main(int argc, const char *argv[])
{
    //auto for functor
    auto newFunc = bind(&func, 1, 2.0, _1, _2);
    int arg = 0;
    newFunc("autoFunc", arg);

    vector<int> aVec = {1,2,3,4};
    size_t idx = 0;
    for (auto it = aVec.begin(), itEnd = aVec.end(); it != itEnd; ++it){
        cout << "aVec[" << idx << "]" << *it << endl;
    }

    //decltype
    auto v = 1;
    decltype(aVec[0]) b = v;
    cout << "decltype b:" << b << endl;
}
