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

void merge_sort(DynamicArray<Element>& temp, DynamicArray<Element>& output)
{
	const auto size = output.size();

	for(auto sub_buffer_size = std::size_t{1}; sub_buffer_size < size; sub_buffer_size = sub_buffer_size + sub_buffer_size)
	{
		const auto num_sub_buffers = (size + sub_buffer_size - 1)/sub_buffer_size;
		for(auto sub_buffer_index = std::size_t{}; sub_buffer_index < num_sub_buffers; sub_buffer_index += 2)
		{
			const auto lower_begin_index = sub_buffer_size*sub_buffer_index;
			const auto lower_end_index = std::min(lower_begin_index + sub_buffer_size, size);
			const auto upper_begin_index = lower_end_index;
			const auto upper_end_index = std::min(upper_begin_index + sub_buffer_size, size);

			auto lower_index = lower_begin_index;
			auto upper_index = upper_begin_index;
			auto output_index = lower_begin_index;

			for(;lower_index != lower_end_index && upper_index != upper_end_index; ++output_index)
			{
				if(temp[lower_index] < temp[upper_index])
				{
					output[output_index] = temp[lower_index];
					++lower_index;
				}
				else
				{
					output[output_index] = temp[upper_index];
					++upper_index;
				}
			}
		
			for(; lower_index != lower_end_index; ++lower_index, ++output_index)
			{
				output[output_index] = temp[lower_index];
			}
			for(; upper_index != upper_end_index; ++upper_index, ++output_index)
			{
				output[output_index] = temp[upper_index];
			}
		}
		std::swap(temp, output);
	}
	std::swap(temp, output);
}

void n_way_merge_sort(DynamicArray<Element>& temp, DynamicArray<Element>& output)
{
	constexpr auto num_streams = 4;
	const auto size = output.size();

	for(auto sub_buffer_size = std::size_t{1}; sub_buffer_size < size; sub_buffer_size *= num_streams)
	{
		const auto num_sub_buffers = (size + sub_buffer_size - 1)/sub_buffer_size;
		for(auto sub_buffer_index = std::size_t{}; sub_buffer_index < num_sub_buffers; sub_buffer_index += num_streams)
		{
			std::size_t stream_index[num_streams];
			std::size_t stream_end[num_streams];
			stream_index[0] = sub_buffer_size*sub_buffer_index;
			stream_end[0] = std::min(sub_buffer_size*(sub_buffer_index + 1), size);
			for(auto index = std::size_t{1}; index < num_streams; ++index)
			{
				stream_index[index] = stream_end[index - 1];
				stream_end[index] = std::min(stream_end[index - 1] + sub_buffer_size, size);
			}

			const auto output_end = std::min(sub_buffer_size*(sub_buffer_index + num_streams), size);
			for(auto output_index = sub_buffer_size*sub_buffer_index; output_index < output_end; ++output_index)
			{
				auto lowest_value = std::numeric_limits<Element>::max();
				auto lowest_value_stream_index = std::size_t{};
				for(auto index = std::size_t{0}; index < num_streams; ++index)
				{
					if(stream_index[index] < stream_end[index])
					{
						if(temp[stream_index[index]] <= lowest_value)
						{
							lowest_value_stream_index = index;
							lowest_value = temp[stream_index[index]];
						}
					}
				}

				output[output_index] = temp[stream_index[lowest_value_stream_index]];
				++stream_index[lowest_value_stream_index];
			}
		}
		std::swap(temp, output);
	}
	std::swap(temp, output);
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

	auto output = DynamicArray<Element>(input.size());

#if defined MULTITHREADING
	const std::size_t num_threads = std::thread::hardware_concurrency();

	auto threads = DynamicArray<std::thread>{};
	threads.reserve(num_threads - 1);
#elif !defined STL
	auto temp = input;
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
		temp = input;
		output = input;
		merge_sort(temp, output);
	}
#endif

	const auto end_point = std::chrono::steady_clock::now();

	const auto duration = end_point - start_point;
	const auto time_in_seconds = std::chrono::duration<double>{duration}.count();

	auto correct_output = input;
	std::stable_sort(std::execution::par_unseq, correct_output.begin(), correct_output.end());
	auto output_is_correct = output == correct_output;

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
	std::cout << "Num elements: " << num_elements << '\n';
	std::cout << "Num iterations: " << num_iterations << '\n';
	std::cout << "Correct output: " << std::boolalpha << output_is_correct << '\n';
	std::cout << "Time taken: " << time_in_seconds << "s\n";
	return EXIT_SUCCESS;
}
