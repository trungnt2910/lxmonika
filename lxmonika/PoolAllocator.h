#pragma once

#include <ntddk.h>
#include <intsafe.h>

class PoolAllocator
{
private:
    PVOID m_ptr = NULL;
    ULONG m_tag;
public:
    PoolAllocator() = default;
    PoolAllocator(POOL_TYPE poolType, SIZE_T size, ULONG tag = 'PALL');
    PoolAllocator(SIZE_T size, ULONG tag = 'PALL');
    PoolAllocator(const PoolAllocator&) = delete;
    ~PoolAllocator();

    template <typename T>
    T* Get() const
    {
        return (T*)m_ptr;
    }

    PVOID Get() const
    {
        return m_ptr;
    }
};
