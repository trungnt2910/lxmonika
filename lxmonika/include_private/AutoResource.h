#pragma once

template <typename T, typename TFree>
concept AutoResourceFreer = requires(T handle, TFree free)
{
    free(handle);
};

template <typename T, typename TFree, bool CheckNull = true> requires AutoResourceFreer<T, TFree>
class AutoResource
{
private:
    T* m_handle;
    const TFree* m_free;
public:
    AutoResource(T& handle, const TFree& free)
        : m_handle(&handle), m_free(&free) { }
    ~AutoResource()
    {
        if constexpr (CheckNull)
        {
            if ((*m_handle) == (T)0)
            {
                return;
            }
        }
        if (m_free != nullptr)
        {
            (*m_free)(*m_handle);
        }
        *m_handle = (T)0;
    }
};

#define AUTO_RESOURCE(t, f)                                                                     \
    const auto t##__Free = [&](decltype(t) t) { return (void)((f)(t)); };                       \
    auto t##__AutoResource = AutoResource<decltype(t), decltype(t##__Free)>(t, t##__Free);
