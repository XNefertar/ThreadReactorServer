# C++ One-Thread-One-Loop Concurrent Server Implementation
C++ - Implementation of One-Thread-One-Loop Concurrent Server Using Muduo-like Library


This project implements a high-concurrency server using the "One-Thread-One-Loop" model, inspired by the Muduo library. The server is designed to handle multiple client requests concurrently using a single thread and a single event loop, following the Reactor pattern.

## Features
- **One-Thread-One-Loop Model**: Each thread handles a single event loop, improving efficiency and simplifying concurrency.
- **Reactor Pattern**: The server uses the Reactor pattern to manage asynchronous I/O operations.
- **High Concurrency**: Optimized for handling many concurrent connections with minimal overhead.
- **Muduo-like Implementation**: Built upon concepts similar to the Muduo library, providing an efficient and scalable design for high-performance networking applications.

## Requirements
- **C++11 or later**: This project uses features of C++11 and above.
- **Linux-based System**: Recommended for running this server due to its reliance on Linux-specific APIs for event handling.

## TestCode

### Custom Any Class Demo

Use the parent class pointer to point to the features of the child class object, encapsulate the two with an Any Class, and automatically derive a simple demo similar to C++17 std::any through the automatic derivation of template parameter types

### Regex Match Demo

Use the C++ regular expression library to parse HTTP request headers and obtain information such as Method/Path/Query/Version

### Time Wheel Demo

1. The Timer class is managed by using the timewheel method (std::vector as the timewheel container) (pointed to and managed by std::shared_ptr<Timer>, and the scattered Timers are organized to form a single element in the timewheel container using std::list)
2. In order to facilitate management and avoid introducing circular references and not increasing the number of counters, use std::unordered_map<uint64_t, std::weak_ptr> to establish a Timer-> weak_ptr mapping relationship for each shared_ptr object
3. Encapsulate the function pointer, use std::bind the corresponding handler to each Timer object, execute the corresponding function when the corresponding timer element is reached, and release the object

### Linux Timer Demo

## Modules

### Server Module

#### Buffer Module

The std::vector <char> and read/write offset are used to represent a user-mode read/write buffer, and when the buffer capacity is insufficient, the memory space is dynamically allocated for expansion.

Provides write and read functions for multiple types of overloads, and provides log macros to record function processing information.

#### Socket Module

#### Channel Module

#### Connection Module

#### Acceptor Module

#### TimerQueue Module

#### Poller Module

#### EventLoop Module

#### TCPServer Module

### Protocol Module







