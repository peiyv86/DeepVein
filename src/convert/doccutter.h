#ifndef DOCCUTTER_H
#define DOCCUTTER_H

#include "core/global_defs.h"
#include "memorypool/memorypool.h"
#include <algorithm>
#include <QList>

class Doccutter
{
public:
    // 使用单例，因为切片规则全局统一即可
    static Doccutter& getInstance() {
        static Doccutter instance;
        return instance;
    }

    /**
     * @brief 滑动窗口切片算法 (原版：值传递)
     * 适合用于切分数量较少的父切片，或者无需极致性能优化的场景。
     * @param fileData 从 file_io 模块传来的完整文件结构
     * @param chunkSize 每个切片的目标最大字符数 (默认 500)
     * @param overlap 上下文重叠的字符数 (默认 50，防止句子被硬切断)
     * @return 切割好的片段列表 (副本传递)
     */
    QList<DocChunk> splitText(const FileTxt& fileData, int chunkSize = 500, int overlap = 50);

    /**
     * @brief 滑动窗口切片算法
     * 专门为高并发流水线设计，避免大量子切片在堆栈上频繁 new/delete 导致的内存碎片。
     * @param fileData 完整文件结构
     * @param chunkSize 每个切片的目标最大字符数
     * @param overlap 上下文重叠的字符数
     * @param pool 外部传入的（推荐 thread_local）内存池引用
     * @return 切割好的片段指针列表
     */
    QList<DocChunk*> splitText(const FileTxt& fileData, int chunkSize, int overlap, MemoryPool<DocChunk>& pool);

private:
    Doccutter();
    ~Doccutter() = default;
    Q_DISABLE_COPY(Doccutter)
};

#endif // DOCCUTTER_H
