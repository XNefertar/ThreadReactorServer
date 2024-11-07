#include <iostream>
#include <string>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <unistd.h>

class Any
{
public:
    Any(): _content(nullptr){}

    template <typename T>
    Any(const T &data) : _content(new Holder<T>(data)) {}
    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {}
    ~Any()
    {
        if (_content)
            delete _content;
    }

    const std::type_info &type(){
        return _content ? _content->type() : typeid(void);
    }

    template<typename T>
    T* get(){
        assert(typeid(T) == _content->type());
        return &(((Holder<T>*)_content)->_data);
    }



private:
    class PlaceHolder
    {
    public:
        virtual ~PlaceHolder() {}
        virtual const std::type_info& type() = 0;
        virtual PlaceHolder* clone() = 0;
    };

    template<class T>
    class Holder : public PlaceHolder
    {
    public:
        Holder(const T& data)
            :_data(data)
        {}
        ~Holder() = default;
        const std::type_info& type(){
            return typeid(T);
        }
        PlaceHolder* clone(){
            return new Holder(_data);
        }
    public:
        T _data;
        
    };

private:
    PlaceHolder* _content;
};

class Test{
public:
    std::string _data;
public:
    Test(const std::string &data): _data(data){
        std::cout << "Test constructor" << std::endl;
    }
    Test(const Test &other): _data(other._data){
        std::cout << "Test copy constructor" << std::endl;
    }
    ~Test(){
        std::cout << "Test destructor" << std::endl;
    }
};

int main()
{
    {
        int a = 10;
        double b = 3.14;
        std::string c = "hello";

        Any val1(a);
        Any val2(b);
        Any val3(c);

        int* p1 = val1.get<int>();
        double* p2 = val2.get<double>();
        std::string* p3 = val3.get<std::string>();

        std::cout << *p1 << std::endl;
        std::cout << *p2 << std::endl;
        std::cout << *p3 << std::endl;
    }
    std::cout << "-------------Method End-------------" << std::endl;
    {
        Test test("hello");
        Any val(test);
        Test* p = val.get<Test>();
        std::cout << p->_data << std::endl;

        Any val2 = val;
        Test* p2 = val2.get<Test>();
        std::cout << p2->_data << std::endl;
    }
    std::cout << "-------------Method End-------------" << std::endl;
    {
        Any val(10);
        int* p = val.get<int>();
        std::cout << *p << std::endl;
    }
}