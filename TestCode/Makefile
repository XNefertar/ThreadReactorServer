# 定义变量
CC = g++
CFLAGS = -std=c++11 -lpthread
TARGET = test
SRC = CustomAny.cc

# 使用变量
$(TARGET):$(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS)

.PHONY: clean
clean:
	rm -rf $(TARGET)