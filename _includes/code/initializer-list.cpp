#include <cstdlib>
#include <iostream>
#include <vector>
#include <initializer_list>

using namespace std;

class InitializerType {
public:
    InitializerType (std::initializer_list<int> list) : m_data(list){}
    virtual ~InitializerType (){};

    //show the data
    void show(){
        size_t i = 0;
        for(auto v : m_data){
            cout << "m_data[" << i++ << "] = " << v << endl;
        }
    }

    //reset data
    void reset(std::initializer_list<int> list){
        m_data = list;
    }
private:
    /* data */
    std::vector<int> m_data;
};

//uniform initialization
struct DataCollection {
    std::string m_name;
    int         m_id;
    float       m_price;
};

DataCollection getDefault(){
    return {"test", 1, 1.0f};
}

using namespace std;
int main(int argc, const char *argv[])
{
    //For non-POD class constructor
    InitializerType obj = {1,2,3,4,5,6,7};
    obj.show();

    std::initializer_list<int> newv = {11,22,33,44,55};
    cout << "reset as new value:";
    obj.reset(newv);
    obj.show();

    cout << "uniform initialization using initialization list" << endl;
    auto data = getDefault();
    cout << "Data info:" << data.m_name << "," << data.m_id << "," << data.m_price << endl;
}
