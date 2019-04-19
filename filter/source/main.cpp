#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>

using Element = int;

constexpr auto cache_line_size = std::size_t{64};

class ByteAllocator
{
public:
	static constexpr auto maximum_alignment = cache_line_size;
	
	ByteAllocator() = default;

	static char* allocate(const std::size_t size) noexcept
	{
		const auto num_blocks = (size + sizeof(Block) - 1) / sizeof(Block);
		
		return reinterpret_cast<char*>(new Block[num_blocks]);
	}
	static void deallocate(char*const allocation, const std::size_t size) noexcept
	{
		delete[] reinterpret_cast<Block*>(allocation);
	}

private:
	struct Block
	{
		alignas(cache_line_size) char bytes[cache_line_size];
	};
};

template<typename Type>
class Allocator
{
public:
	static_assert(alignof(Type) <= ByteAllocator::maximum_alignment, "The allocator type's alignment requirement is too damn high!");
	using value_type = Type;

	Allocator() = default;
	template<typename Type>
	constexpr Allocator(const Allocator<Type>&) noexcept
	{

	}

	template<typename Type>
	constexpr bool operator==(const Allocator<Type>&) const noexcept
	{
		return true;
	}
	template<typename Type>
	constexpr bool operator!=(const Allocator<Type>&) const noexcept
	{
		return false;
	}
	
	Type* allocate(const std::size_t size) noexcept
	{
		return reinterpret_cast<Type*>(ByteAllocator::allocate(size * sizeof(Type)));
	}
	void deallocate(Type*const allocation, const std::size_t size) noexcept
	{
		ByteAllocator::deallocate(reinterpret_cast<char*>(allocation), size * sizeof(Type));
	}
};

template<typename Type>
using DynamicArray = std::vector<Type, Allocator<Type>>;

auto program = [](const std::size_t num_threads, const std::size_t thread_id, const DynamicArray<Element>& input, DynamicArray<Element>& output, const Element filter_max, const std::size_t num_iterations) -> std::size_t
{
	alignas(cache_line_size) static std::atomic<std::size_t> threads_filled;
	alignas(cache_line_size) static std::atomic<std::size_t> threads_completed;
	alignas(cache_line_size) static DynamicArray<std::size_t> subarray_sizes(num_threads - 1);
	alignas(cache_line_size) static std::size_t final_size;

	const auto begin_index = (input.size()*thread_id) / num_threads;
	const auto end_index = (input.size()*(thread_id + 1)) / num_threads;

	auto temp = DynamicArray<Element>(end_index - begin_index);

	for(auto index = std::size_t{}; index < num_iterations; ++index)
	{
		auto filtered_size = std::size_t{};
		for(auto index = begin_index; index < end_index; ++index)
		{
			if(input[index] < filter_max)
			{
				temp[filtered_size] = input[index];
				++filtered_size;
			}
		}

		// If we are not the last thread.
		if(thread_id != num_threads - 1)
		{
			subarray_sizes[thread_id] = filtered_size;
		}
		
		// If we are not the last thread to finish filling our buffer.
		if(threads_filled.fetch_add(1, std::memory_order_acq_rel) != num_threads - 1)
		{
			while(threads_filled.load(std::memory_order_acquire) != 0)
			{
			}
		}
		else
		{
			threads_filled.store(std::size_t{}, std::memory_order_release);
		}

		const auto lower_id = thread_id - 1;

		auto write_index = std::size_t{};
		if(thread_id != 0)
		{
			write_index = subarray_sizes[0];
			for(auto subarray_size_id = std::size_t{1}; subarray_size_id < thread_id; ++subarray_size_id)
			{
				write_index += subarray_sizes[subarray_size_id];
			}

			// If we are the last thread.
			if(thread_id == num_threads - 1)
			{
				final_size = write_index + filtered_size;
			}
		}

		for(auto read_index = std::size_t{}; read_index < filtered_size; ++write_index, ++read_index)
		{
			output[write_index] = temp[read_index];
		}
		
		// If we are not the last one to exit the operation.
		if(threads_completed.fetch_add(1, std::memory_order_acq_rel) != num_threads - 1)
		{
			while(threads_completed.load(std::memory_order_acquire) != 0)
			{
			}
		}
		else
		{
			threads_completed.store(std::size_t{}, std::memory_order_release);
		}
	}

	return final_size;
};

int main(int num_arguments, const char*const*const arguments)
{
	constexpr auto num_elements = std::size_t{1'000'000};
	constexpr auto num_iterations = std::size_t{1000};
	constexpr auto min_value = Element{};
	constexpr auto max_value = 10'000;
	constexpr auto filter_max = Element{6700};
	constexpr auto seed = std::size_t{9879565};

	auto input = DynamicArray<Element>{};

	auto rng_engine = std::mt19937_64{seed};
	const auto random_int = std::uniform_int_distribution<Element>{min_value, max_value};

	input.resize(num_elements);
	for(auto index = std::size_t{}; index < num_elements; ++index)
	{
		input[index] = random_int(rng_engine);
	}

	auto output = DynamicArray<Element>(input.size());

#if defined MULTITHREADING
	const std::size_t num_threads = std::thread::hardware_concurrency();

	auto threads = DynamicArray<std::thread>{};
	threads.reserve(num_threads - 1);
#endif

	std::cout << "Concurrent filter test\n";
	const auto start_point = std::chrono::steady_clock::now();

#if defined MULTITHREADING
	for(auto index = std::size_t{}; index < num_threads - 1; ++index)
	{
		threads.push_back(std::thread{program, num_threads, index, std::cref(input), std::ref(output), filter_max, num_iterations});
	}

	const auto size = program(num_threads, num_threads - 1, input, output, filter_max, num_iterations);

	for(auto& thread : threads)
	{
		thread.join();
	}
	output.resize(size);
#else
	auto filtered_end_index = std::size_t{};
	for(auto index = std::size_t{}; index < num_iterations; ++index)
	{
		for(auto index = std::size_t{}; index < input.size(); ++index)
		{
			if(input[index] < filter_max)
			{
				output[filtered_end_index] = input[index];
				++filtered_end_index;
			}
		}
	}
	output.resize(filtered_end_index);
#endif

	const auto end_point = std::chrono::steady_clock::now();

	const auto duration = end_point - start_point;
	const auto time_in_seconds = std::chrono::duration<double>{duration}.count();

	std::cout << "Test has succeeded!\n";
	std::cout << "First value: " << output.front() << '\n';
	std::cout << "Last value: " << output.back() << '\n';
	std::cout << "Time taken: " << time_in_seconds << "s\n";
	return EXIT_SUCCESS;
}
