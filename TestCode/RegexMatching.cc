#include <iostream>
#include <string>
#include <regex>

void ReqLine()
{
    std::cout << "-------------Line Start-------------" << std::endl;
    // 完整的HTTP请求行
    std::string str = "GET /index.html?name=123&age=20 HTTP/1.1\r\n";
    std::regex reg("(GET|HEAD|POST|PUT|DELETE) (([^?]+)(?:\\?(.*?))?) (HTTP/1\\.[01])(?:\r\n|\n)");
    std::smatch match;
    std::regex_match(str, match, reg);

    int index = 0;
    for (auto x : match)
    {
        std::cout << "match[" << index << "]: " << x << std::endl;
        index++;
    }

    std::cout << "-------------Line End-------------" << std::endl;
}

void MethodMatch(const std::string &str)
{
    std::cout << "-------------Method Start-------------" << std::endl;
    // 匹配HTTP请求方法
    std::regex reg("(GET|POST|PUT|DELETE) .*");
    std::smatch match;
    std::regex_search(str, match, reg);

    int index = 0;
    for (auto x : match)
    {
        std::cout << "match[" << index << "]: " << x << std::endl;
        index++;
    }

    std::cout << "-------------Method End-------------" << std::endl;
}

void PathMatch(const std::string &str)
{
    std::cout << "-------------Path Start-------------" << std::endl;
    // 匹配HTTP请求路径
    std::regex reg("([^?]+).*");
    std::smatch match;
    std::regex_search(str, match, reg);

    int index = 0;
    for (auto x : match)
    {
        std::cout << "match[" << index << "]: " << x << std::endl;
        index++;
    }

    std::cout << "-------------Path End-------------" << std::endl;
}

void QueryMatch(const std::string &str)
{
    std::cout << "-------------Query Start-------------" << std::endl;
    // 匹配HTTP请求参数
    std::regex reg("(?:\\?(.*?))? .*");
    std::smatch match;
    std::regex_search(str, match, reg);

    int index = 0;
    for (auto x : match)
    {
        std::cout << "match[" << index << "]: " << x << std::endl;
        index++;
    }

    std::cout << "-------------Query End-------------" << std::endl;
}

void VersionMatch(const std::string &str)
{
    std::cout << "-------------Version Start-------------" << std::endl;
    // 匹配HTTP请求版本
    std::regex reg("(HTTP/1\\.[01])(?:\r\n|\n)");
    std::smatch match;
    std::regex_search(str, match, reg);

    int index = 0;
    for (auto x : match)
    {
        std::cout << "match[" << index << "]: " << x << std::endl;
        index++;
    }

    std::cout << "-------------Version End-------------" << std::endl;
}

int main()
{
    ReqLine();
    MethodMatch("GET /s");
    PathMatch("/index.html?name=123&age=20\r\n");
    QueryMatch("?name=123&age=20 HTTP/1.1\r\n");
    VersionMatch("HTTP/1.1\r\n");
    return 0;
}