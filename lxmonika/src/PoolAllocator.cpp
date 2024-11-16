#include "PoolAllocator.h"

#include "compat.h"

PoolAllocator::PoolAllocator(POOL_TYPE poolType, SIZE_T size, ULONG tag)
{
    m_ptr = ExAllocatePool2(poolType, size, tag);
    m_tag = tag;
}

PoolAllocator::PoolAllocator(SIZE_T size, ULONG tag)
    : PoolAllocator(PagedPool, size, tag)
{

}

PoolAllocator::~PoolAllocator()
{
    PVOID ptr = NULL;
    ptr = InterlockedExchangePointer(&m_ptr, ptr);

    if (ptr != NULL)
    {
        ExFreePoolWithTag(ptr, m_tag);
    }
}
