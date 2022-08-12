#include <iostream>
#include "../rms_client/buffer_queue.h"

int main()
{
    buffer_queue bqueue;
    auto b4096 = std::make_unique<char[]>(4096);
    auto b5000 = std::make_unique<char[]>(5000);
    auto b8192 = std::make_unique<char[]>(8192);
    
    memset(b4096.get(), 0, 4096);
    memset(b5000.get(), 1, 5000);
    memset(b8192.get(), 2, 8192);

    bqueue.push(b4096.get(), 4096);
    bqueue.push(b8192.get(), 8192);
    bqueue.get(8192, b8192.get());
    bqueue.push(b5000.get(), 5000);

    std::cout << bqueue.get_total();

    //bqueue.peek()
}
