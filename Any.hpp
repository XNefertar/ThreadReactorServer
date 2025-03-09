#include <typeinfo>

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