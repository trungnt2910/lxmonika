#pragma once

#include <ntddk.h>

template <typename T>
concept Lockable = requires(T t)
{
    t.Lock();
    t.Unlock();
};

template <Lockable T>
class Locker
{
private:
    T* m_obj = NULL;

public:
    Locker(T* obj) : m_obj(obj)
    {
        m_obj->Lock();
    }

    Locker(const Locker&) = delete;

    Locker(const Locker&& other)
    {
        m_obj = NULL;
        InterlockedExchangePointer((PVOID*)&other.m_obj, m_obj);
    }

    Locker& operator=(const Locker&) = delete;

    Locker& operator=(Locker&& other)
    {
        if (this == &other)
        {
            return *this;
        }
        this->~Locker();
        InterlockedExchangePointer((PVOID*)&other.m_obj, m_obj);
        return *this;
    }

    ~Locker()
    {
        T* obj = NULL;
        obj = (T*)InterlockedExchangePointer((PVOID*)&m_obj, obj);

        if (obj != NULL)
        {
            obj->Unlock();
        }
    }
};
