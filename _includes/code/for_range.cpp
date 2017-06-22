#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std;
int main(int argc, const char *argv[])
{
    //initializer list
    std::vector<int> aVec = {1,2,3,4,5,6,7};

    //range based for
    for(auto v:aVec){
        cout << v << endl;
    }

    //array
    int arr[] = {1,2,3,4,5};
    for(int& x : arr){
        x *= x;
    }
    for(auto x : arr){
        cout << x << ' ';
    }
    cout << endl;

}
