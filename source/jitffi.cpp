#include "jitffi.h"
#include "opcode.h"
#include <cassert>
#include <algorithm>
#include <functional>

#if defined(_WIN32)
#	include <Windows.h>
#else
#	include <sys/mman.h>
#endif

namespace JitFFI
{

	// JitFuncPool

	void* JitFuncPool::alloc(size_t size, bool readonly)
	{
#if defined(_WIN32)
		auto flags = readonly ? PAGE_EXECUTE_READ : PAGE_EXECUTE_READWRITE;
		void *dp = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, flags);
#else
		auto flags = (readonly ? PROT_READ : PROT_WRITE) | PROT_EXEC;
		void *dp = mmap(NULL, size, flags, MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
		return dp;
	}

	void JitFuncPool::protect(void * dp, size_t size)
	{
		assert(size != 0);
#if defined(_WIN32)
		DWORD w;
		BOOL v = VirtualProtect(dp, size, PAGE_EXECUTE_READ, &w);
		assert(v);
#else
		int v = mprotect(dp, size, PROT_READ | PROT_EXEC);
		assert(v == 0);
#endif
	}

	void JitFuncPool::unprotect(void * dp, size_t size)
	{
		assert(size != 0);
#if defined(_WIN32)
		DWORD w;
		BOOL v = VirtualProtect(dp, size, PAGE_EXECUTE_READWRITE, &w);
		assert(v);
#else
		int v = mprotect(dp, size, PROT_WRITE | PROT_EXEC);
		assert(v == 0);
#endif
	}

	void JitFuncPool::free(void * dp, size_t size)
	{
#if defined(_WIN32)
		BOOL v = VirtualFree(dp, 0, MEM_RELEASE);
		assert(v);
#else
		int v = munmap(dp, size);
		assert(v == 0);
#endif
	}

	// JitFuncCallerCreater


	byte JitFuncCallerCreater::get_offset() {
		return (push_count % 2 == 0) ? 0 : 0x8;
	}

	auto JitFuncCallerCreater::get_add_offset() {
		auto push_offset = push_count * 0x8;
		assert(push_offset <= UINT32_MAX - 0x10);
		return 0x8 + get_offset() + push_offset;
	}

	byte& JitFuncCallerCreater::sub_rsp_unadjusted() {
		OpCode_x64::sub_rsp_byte(jfc, 0x8);
		return *(jfc.end() - 1);
	}

	void JitFuncCallerCreater::add_rsp() {
		OpCode_x64::add_rsp_uint32(jfc, get_add_offset());  // !NOTICE! this num may > 1 byte.
	}

	void JitFuncCallerCreater::adjust_sub_rsp(byte &d) {
		d += get_offset();
	}

	void JitFuncCallerCreater::add_int(uint64_t dat) {
		return _add_int([&](unsigned int c) { return OpCode_curr::add_int(jfc, dat, c); });
	}
	void JitFuncCallerCreater::add_int_uint32(uint32_t dat) {
		return _add_int([&](unsigned int c) { return OpCode_curr::add_int_uint32(jfc, dat, c); });
	}
	void JitFuncCallerCreater::add_int_rbx() {
		return _add_int([&](unsigned int c) { return OpCode_curr::add_int_rbx(jfc, c); });
	}
	void JitFuncCallerCreater::add_double(uint64_t dat) {
		return _add_double([&](unsigned int c) { return OpCode_curr::add_double(jfc, dat, c); });
	}

	void JitFuncCallerCreater::_add_int(const std::function<OpHandler> &handler) {
#if (defined(_WIN64))
		assert(argn - add_count >= 0);
		++add_count;
		push_count += handler(argn - add_count);
#elif (defined(__x86_64__))
		assert(addarg_int_count >= 0);
		--addarg_int_count;
		push_count += handler(addarg_int_count);
#endif
	}
	void JitFuncCallerCreater::_add_double(const std::function<OpHandler> &handler) {
#if (defined(_WIN64))
		assert(argn - add_count >= 0);
		++add_count;
		push_count += handler(argn - add_count);
#elif (defined(__x86_64__))
		assert(addarg_double_count >= 0);
		--addarg_double_count;
		push_count += handler(addarg_double_count);
#endif
	}

	void JitFuncCallerCreater::push(uint64_t dat) {
		OpCode_x64::mov_rax_uint64(jfc, dat);
		OpCode_x64::push_rax(jfc);
		push_count += 1;
	}

	void JitFuncCallerCreater::push_rbx() {
		OpCode_x64::push_rbx(jfc);
		OpCode_x64::sub_rsp_byte(jfc, 0x8);
	}
	void JitFuncCallerCreater::pop_rbx() {
		OpCode_x64::add_rsp_byte(jfc, 0x8);
		OpCode_x64::pop_rbx(jfc);
	}

	void JitFuncCallerCreater::call() {
#if (defined(_WIN64))
		OpCode_x64::sub_rsp_byte(jfc, 0x20);
#endif
		assert(have_init);
		OpCode::call_func(jfc, func);
#if (defined(_WIN64))
		OpCode_x64::add_rsp_byte(jfc, 0x20);
#endif
	}

	void JitFuncCallerCreater::ret() {
		OpCode::ret(jfc);
	}

}
