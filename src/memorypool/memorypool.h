#ifndef MEMORYPOOL_H
#define MEMORYPOOL_H
#include <vector>
#include <mutex>
#include <utility>
#include <iostream>
template <typename T>
class MemoryPool {
private:
    // 核心技巧：复用内存。闲置时是 next 指针，分配后是真实数据 T
    union Chunk {
        T payload;
        Chunk* next;
    };

    Chunk* freeList = nullptr;                // 指向第一个空闲块
    std::vector<void*> allocatedBlocks;       // 记录所有向操作系统“批发”的大块内存，用于最终销毁
    size_t objectsPerBlock;                   // 每次批发多少个对象的大小
    std::mutex mtx;                           // 保护 freeList 的多线程安全

    // 内部动作：空闲链表干了，向操作系统批发一大块新内存
    void allocateBlock() {
        // 一次性申请 N 个 Chunk 大小的连续内存
        void* newBlock = ::operator new(objectsPerBlock * sizeof(Chunk));
        allocatedBlocks.push_back(newBlock);

        Chunk* chunkArray = static_cast<Chunk*>(newBlock);

        // 将这块连续内存串成一个单向链表
        for (size_t i = 0; i < objectsPerBlock - 1; ++i) {
            chunkArray[i].next = &chunkArray[i + 1];
        }
        chunkArray[objectsPerBlock - 1].next = nullptr;

        freeList = chunkArray;
    }

public:
    explicit MemoryPool(size_t blockSize = 1000) : objectsPerBlock(blockSize) {}

    ~MemoryPool() {
        // 析构时，把所有批发来的大内存块还给操作系统
        for (void* block : allocatedBlocks) {
            ::operator delete(block);
        }
        // std::cout << "MemoryPool destroyed, resources freed.\n";
    }

    // 分配对象（完美转发参数给 T 的构造函数）
    template <typename... Args>
    T* allocate(Args&&... args) {
        std::lock_guard<std::mutex> lock(mtx);

        if (freeList == nullptr) {
            allocateBlock(); // 没库存了，去进货
        }

        // 从链表头摘下一个空闲块
        Chunk* chunk = freeList;
        freeList = freeList->next;

        // 定位 Placement new：在已经分配好的内存地址上，原地调用 T 的构造函数
        return new (&chunk->payload) T(std::forward<Args>(args)...);
    }

    // 回收对象
    void deallocate(T* ptr) {
        if (!ptr) return;

        // 显式调用对象的析构函数，清理其内部资源（如释放 std::string）
        ptr->~T();

        // 将这块干净的内存重新挂载回空闲链表的头部
        std::lock_guard<std::mutex> lock(mtx);
        Chunk* chunk = reinterpret_cast<Chunk*>(ptr);
        chunk->next = freeList;
        freeList = chunk;
    }
};

#endif // MEMORYPOOL_H
