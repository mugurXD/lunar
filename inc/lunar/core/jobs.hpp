#pragma once
#include <lunar/api.hpp>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lunar
{
	struct LUNAR_API JobHandle
	{
		uint64_t id = 0;

		bool operator==(const JobHandle&) const = default;
	};

	class LUNAR_API JobSystem
	{
	public:
		JobSystem(size_t worker_count) noexcept;
		~JobSystem() noexcept;

		JobSystem(const JobSystem&)            = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		template<typename Work, typename Complete>
		JobHandle submit(Work work, Complete complete)
		{
			return enqueue([work = std::move(work), complete = std::move(complete)]() mutable -> Completion {
				if constexpr (std::is_void_v<std::invoke_result_t<Work&>>)
				{
					work();
					return [complete = std::move(complete)]() mutable { complete(); };
				}
				else
				{
					return [result = work(), complete = std::move(complete)]() mutable { complete(std::move(result)); };
				}
			});
		}

		void   cancel(JobHandle job);
		size_t processCompleted();
		void   waitIdle();
		size_t getWorkerCount() const;

		static size_t defaultWorkerCount();

	private:
		using Completion = std::move_only_function<void()>;
		using Task       = std::move_only_function<Completion()>;

		struct PendingJob
		{
			uint64_t id   = 0;
			Task     task = {};
		};

		struct CompletedJob
		{
			uint64_t   id         = 0;
			Completion completion = {};
		};

		JobHandle                 enqueue(Task task);
		void                      runWorker(std::stop_token stop_token);
		std::optional<PendingJob> takeJob(std::stop_token stop_token);
		void                      finishJob(uint64_t id, Completion completion);

		std::mutex                   mutex;
		std::condition_variable_any  workAvailable;
		std::condition_variable      idle;
		std::deque<PendingJob>       pendingJobs;
		std::vector<CompletedJob>    completedJobs;
		std::unordered_set<uint64_t> runningJobs;
		std::unordered_set<uint64_t> droppedJobs;
		uint64_t                     nextJobId = 1;
		std::vector<std::jthread>    workers;
	};
}
