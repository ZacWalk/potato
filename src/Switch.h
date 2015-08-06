#pragma once

class Tasks
{
private:

    CEvent _event;
    critical_section _cs;

    typedef std::function<void()> Task;
    std::deque<Task> _q;
    Task _single;

public:

    Tasks() : _event(FALSE, FALSE)
    {
    }

    ~Tasks()
    {
        critical_section_lock l(_cs);
        _q.clear();
    }

    bool Pop(Task &result)
    {
        critical_section_lock l(_cs);

        if (_single)
        {
            result = std::move(_single);
            _single = nullptr;
            return true;
        }

        if (_q.empty()) return false;
        result = std::move(_q.front());
        _q.pop_front();
        return true;
    }

    inline void Single(const Task & f)
    {
        critical_section_lock l(_cs);
        _single = f;
        _event.Set();
    }

    inline void operator()(const Task & f)
    {
        critical_section_lock l(_cs);
        _q.push_back(f);
        _event.Set();
    }

    void Run()
    {
        Task task;

        while (Pop(task))
        {
            try
            {
                task();
            }
            catch (std::exception &e)
            {
            }

            task = nullptr;
        }
    }

    operator HANDLE()
    {
        return _event;
    }
};


extern DWORD g_mainThreadId;

extern Tasks Switch;
extern Tasks Async;

extern CEvent eventExit;