#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <execution>

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

auto program = [](const std::size_t num_threads, const std::size_t thread_id, const DynamicArray<Element>& input, const std::size_t num_iterations) -> DynamicArray<Element>
{
	return DynamicArray<Element>{};
};

template<typename Iterator, typename Sentinel>
auto merge_sort(const Iterator begin, const Sentinel end) -> DynamicArray<std::decay_t<decltype(*begin)>>
{
	const auto size = end - begin;
	if(size > 1)
	{
		const auto lower = merge_sort(begin, begin + size/2);
		const auto upper = merge_sort(begin + size/2, end);

		auto output = DynamicArray<std::decay_t<decltype(*begin)>>(size);

		auto lower_it = lower.begin();
		auto upper_it = upper.begin();
		auto output_it = output.begin();

		const auto lower_end = lower.end();
		const auto upper_end = upper.end();

		for(;lower_it != lower_end && upper_it != upper_end; ++output_it)
		{
			if(*lower_it < *upper_it)
			{
				*output_it = *lower_it;
				++lower_it;
			}
			else
			{
				*output_it = *upper_it;
				++upper_it;
			}
		}
		
		for(; lower_it != lower_end; ++lower_it, ++output_it)
		{
			*output_it = *lower_it;
		}
		for(; upper_it != upper_end; ++upper_it, ++output_it)
		{
			*output_it = *upper_it;
		}

		return output;
	}
	else
	{
		return DynamicArray<std::decay_t<decltype(*begin)>>(begin, end);
	}
}

int main(int num_arguments, const char*const*const arguments)
{
	constexpr auto num_elements = std::size_t{1'000'000};
	constexpr auto num_iterations = std::size_t{100};
	constexpr auto min_value = Element{};
	constexpr auto max_value = 100'000;
	constexpr auto seed = std::size_t{9879565};

	auto input = DynamicArray<Element>{};

	auto rng_engine = std::mt19937_64{seed};
	const auto random_int = std::uniform_int_distribution<Element>{min_value, max_value};

	input.resize(num_elements);
	for(auto index = std::size_t{}; index < num_elements; ++index)
	{
		input[index] = random_int(rng_engine);
	}

	auto output = DynamicArray<Element>();

#if defined MULTITHREADING
	const std::size_t num_threads = std::thread::hardware_concurrency();

	auto threads = DynamicArray<std::thread>{};
	threads.reserve(num_threads - 1);
#endif

	std::cout << "Concurrent merge sort test\n";
	const auto start_point = std::chrono::steady_clock::now();

#if defined MULTITHREADING
	for(auto index = std::size_t{}; index < num_threads - 1; ++index)
	{
		threads.push_back(std::thread{program, num_threads, index, std::cref(input), num_iterations});
	}

	output = program(num_threads, num_threads - 1, input, num_iterations);

	for(auto& thread : threads)
	{
		thread.join();
	}
#elif defined STL
	for(auto index = std::size_t{}; index < num_iterations; ++index)
	{
		output = input;
		std::stable_sort(std::execution::par_unseq, output.begin(), output.end());
	}
#else
	for(auto index = std::size_t{}; index < num_iterations; ++index)
	{
		output = merge_sort(input.begin(), input.end());
	}
#endif

	const auto end_point = std::chrono::steady_clock::now();

	const auto duration = end_point - start_point;
	const auto time_in_seconds = std::chrono::duration<double>{duration}.count();

	std::cout << "Done with: ";
	if constexpr(sizeof(void*) == 8)
	{
		std::cout << "[x64]";
	}
#if defined STL
	std::cout << "[STL]";
#endif
#if defined MULTITHREADING
	std::cout << "[MULTITHREADING]";
#endif
	std::cout << '\n';
	std::cout << "First value: " << output.front() << '\n';
	std::cout << "Last value: " << output.back() << '\n';
	std::cout << "Time taken: " << time_in_seconds << "s\n";
	return EXIT_SUCCESS;
}
