#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#include <cassert>

namespace RolLang {

struct GCGlobalState
{
	std::mutex GlobalLock;
	//list of types
};

struct RuntimeType;

struct GCTypeInfo
{
	RuntimeType* RuntimeType;
	bool IsVariableSize;
	short Size;
	std::unique_ptr<short[]> PointerOffset;
};

//object header for gc
struct GCHeader
{
	GCTypeInfo* Info;
	//GCHeader* Next; //linked list?
};

class GC
{
public:
	GC(GC& cloneFrom) : _g(cloneFrom._g)
	{

	}

	explicit GC() : _g(std::make_unique<GCGlobalState>())
	{
	}

private:
	~GC()
	{
	}

public:
	void* AllocateRaw(std::size_t size);
	void FreeRaw(void* ptr, std::size_t size);

public:
	//allocate/free type
	//add external ref (use ObjectHandle)

private:
	std::shared_ptr<GCGlobalState> _g;
};

}
