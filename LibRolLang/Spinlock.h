#pragma once
#include <atomic>
#include <mutex>

class Spinlock
{
private:
	std::atomic_flag _lockFlag = ATOMIC_FLAG_INIT;

public:
	void lock()
	{
		while (_lockFlag.test_and_set(std::memory_order_acquire));
	}

	bool try_lock()
	{
		return !_lockFlag.test_and_set(std::memory_order_acquire);
	}

	void unlock()
	{
		_lockFlag.clear(std::memory_order_release);
	}
};
