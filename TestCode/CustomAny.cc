#include <iostream>
#include <string>
#include <cassert>
#include <typeinfo>
#include <typeindex>
#include <unistd.h>

// Any 类
// 基于 类型擦除 设计模式实现
// 任意类型的数据存储和访问
// 隐藏类型信息，对外仅暴露接口
// 将 静态多态（编译时多态） 转为 动态多态（运行时多态）
// 静态多态：模板
// 动态多态：虚函数
// 通过虚函数实现动态多态，实现类型擦除

// 结构组成
// 公共接口（Interface），这里是 PlaceHolder 类
// 具体包装类（Concrete  Class），这里是 Holder 类
// 外层包装类（Wrapper），这里是 Any 类
// 一般都会持有一个 PlaceHolder 类的指针，通过虚函数实现动态多态

class Any
{
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

    // 若 content 不为空，则返回 content 的类型信息，否则返回 void 类型信息
    const std::type_info &type(){
        return _content ? _content->type() : typeid(void);
    }

    // 获取 data 指针
    template<typename T>
    T* get(){
        assert(typeid(T) == _content->type());
        return &(static_cast<Holder<T>*>(_content)->_data);
    }
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
    // 限定作用域
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