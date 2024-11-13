#pragma once

#include <functional>

class Transaction
{
private:
    bool _commit = false;
    std::function<void()> _cleanup;
public:
    Transaction(
        const std::function<void()>& action,
        std::function<void()>&& cleanup
    ) : _cleanup(std::move(cleanup))
    {
        action();
    }

    Transaction(const Transaction&) = delete;

    void Commit()
    {
        _commit = true;
    }

    ~Transaction()
    {
        if (!_commit)
        {
            _cleanup();
        }
    }
};
