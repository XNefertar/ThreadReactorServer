#ifndef __BUFFER_HPP__
#define __BUFFER_HPP__


#include <string>
#include <vector>
#include <memory>
#include "Log.hpp"

class Buffer
{
private:
    std::vector<char> _buffer;
    ssize_t _readIndex;
    ssize_t _writeIndex;

public:
    Buffer(ssize_t size = 1024)
        : _buffer(size), _readIndex(0), _writeIndex(0)
    {}
    ~Buffer() = default;

    char *GetBegin()      { return &_buffer[0]; }
    char *GetWriteBegin() { return &_buffer[_writeIndex]; }
    char *GetReadBegin()  { return &_buffer[_readIndex]; }
    uint64_t GetTailRestSize() { return _buffer.size() - _writeIndex; }
    uint64_t GetReadableSize() { return _writeIndex - _readIndex; }
    uint64_t GetHeadRestSize() { return _readIndex; }

    void UpdateReadIndex(uint64_t len)  { _readIndex += len; }
    void UpdateWriteIndex(uint64_t len) { _writeIndex += len; }

    // 考虑扩容
    void EnsureWirteSize(uint64_t len){
        if(len <= GetTailRestSize()) return;

        if(len <= GetHeadRestSize() + GetTailRestSize()){
            std::copy(GetReadBegin(), GetWriteBegin(), GetBegin());
            _writeIndex -= _readIndex;
            _readIndex = 0;
        }
        else{
            DBG_LOG("Buffer resize %d", _writeIndex + len);
            _buffer.resize(_writeIndex + len);
        }
    }

    void Write(const void* data, uint64_t len){
        if(len == 0) return;
        EnsureWirteSize(len);
        const char* str = static_cast<const char*>(data);
        std::copy(str, str + len, GetWriteBegin());
    }

    void WritePush(const char* data, uint64_t len){
        Write(data, len);
        UpdateWriteIndex(len);
    }

    void WriteString(std::string str){
        Write(str.c_str(), str.size());
    }

    void WriteBuffer(Buffer& buffer){
        Write(buffer.GetReadBegin(), buffer.GetReadableSize());
    }

    void Read(const void* OutBuffer, uint64_t len){
        if(len == 0) return;
        if(len > GetReadableSize()){
            ERR_LOG("Buffer Read Error, len: %d, readableSize: %d", len, GetReadableSize());
            return;
        }
        const char* str = static_cast<const char*>(OutBuffer);
        std::copy(GetReadBegin(), GetReadBegin() + len, str);
    }

    void ReadPop(const void* OutBuffer, uint64_t len){
        Read(OutBuffer, len);
        UpdateReadIndex(len);
    }

    void ReadString(std::string& str, uint64_t len){
        str.clear();
        str.append(GetReadBegin(), len);
        UpdateReadIndex(len);
    }
    
    void Clear(){
        _readIndex = 0;
        _writeIndex = 0;
    }


};

#endif