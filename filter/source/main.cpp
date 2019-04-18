#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>

using Element = int;

constexpr auto block_num_bytes = std::size_t{64};
constexpr auto element_block_size = block_num_bytes / sizeof(Element);

auto program = [](const std::size_t num_threads, const std::size_t thread_id, const std::vector<Element>& input, std::vector<Element>& output, const Element filter_max, const std::size_t num_iterations) -> std::size_t
{
	static std::atomic<bool> operation_done;
	static std::atomic<std::size_t> threads_waiting_on_initialized;
	static std::atomic<bool> operation_initialized;
	static std::vector<std::size_t> subarray_ends(num_threads);
	static std::vector<std::atomic<bool>> joins(num_threads - 1);

	for(auto index = std::size_t{}; index < num_iterations; ++index)
	{		
		const auto num_blocks = (input.size() + element_block_size - 1) / element_block_size;
		const auto starting_block = (num_blocks * thread_id) / num_threads;
		const auto ending_block = (num_blocks * (thread_id + 1)) / num_threads;
		const auto begin_index = starting_block * element_block_size;
		const auto end_index = std::min(input.size(), ending_block * element_block_size);

		auto filtered_end_index = begin_index;
		for(auto index = begin_index; index < end_index; ++index)
		{
			if(input[index] < filter_max)
			{
				output[filtered_end_index] = input[index];
				++filtered_end_index;
			}
		}

		subarray_ends[thread_id] = filtered_end_index;
		
		[thread_id, num_threads, &input, &output, num_blocks]
		{
			for(auto pair_size = std::size_t{2}, displacement = std::size_t{}; (num_threads + pair_size/2 - 1) / pair_size != 0; displacement += (num_threads + pair_size/2 - 1) / pair_size, pair_size *= 2)
			{
				const auto pair_index = thread_id / pair_size;
				const auto lower_id = pair_index * pair_size;
				const auto upper_id = lower_id + pair_size/2;
				
				if(upper_id < num_threads)
				{
					const auto join_flag_id = displacement + pair_index;

					// if we don't join
					if(!joins[join_flag_id].exchange(true, std::memory_order_acq_rel))
					{
						while(!operation_done.load(std::memory_order_acquire))
						{
						}
						return;
					}

					const auto lower_end_index = subarray_ends[lower_id];
					const auto upper_starting_block = (num_blocks * upper_id) / num_threads;
					const auto upper_begin_index = upper_starting_block * element_block_size;
					const auto upper_end_index = subarray_ends[upper_id];
					const auto upper_size = upper_end_index - upper_begin_index;
					subarray_ends[lower_id] = lower_end_index + upper_size;
					for(auto write_index = lower_end_index, read_index = upper_begin_index, end = lower_end_index + upper_size; write_index < end; ++write_index, ++read_index)
					{
						output[write_index] = output[read_index];
					}
				}
			}

			operation_initialized.store(false, std::memory_order_relaxed);
			operation_done.store(true, std::memory_order_release);
		}();
		
		// If we are not the last one to exit the operation.
		if(threads_waiting_on_initialized.fetch_add(1, std::memory_order_relaxed) != num_threads - 1)
		{
			while(!operation_initialized.load(std::memory_order_acquire))
			{

			}
		}
		else
		{
			for(auto& join : joins)
			{
				join.store(false, std::memory_order_relaxed);
			}

			threads_waiting_on_initialized.store(std::size_t{}, std::memory_order_relaxed);
			operation_done.store(false, std::memory_order_relaxed);
			operation_initialized.store(true, std::memory_order_release);
		}
	}
	return subarray_ends.front();
};

int main(int num_arguments, const char*const*const arguments)
{
	constexpr auto num_elements = std::size_t{1'000'000};
	constexpr auto num_iterations = std::size_t{1000};
	constexpr auto min_value = Element{};
	constexpr auto max_value = 10'000;
	constexpr auto filter_max = Element{6700};
	constexpr auto seed = std::size_t{9879565};

	auto input = std::vector<Element>{};

	auto rng_engine = std::mt19937_64{seed};
	const auto random_int = std::uniform_int_distribution<Element>{min_value, max_value};

	input.resize(num_elements);
	for(auto index = std::size_t{}; index < num_elements; ++index)
	{
		input[index] = random_int(rng_engine);
	}

	auto output = std::vector<Element>(input.size());

#if defined MULTITHREADING
	const std::size_t num_threads = std::thread::hardware_concurrency();

	auto threads = std::vector<std::thread>{};
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
	for(auto index = std::size_t{}; index < num_iterations; ++index)
	{		
		auto filtered_end_index = std::size_t{};
		for(auto index = std::size_t{}; index < input.size(); ++index)
		{
			if(input[index] < filter_max)
			{
				output[filtered_end_index] = input[index];
				++filtered_end_index;
			}
		}
	}
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
