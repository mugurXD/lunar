#include <lunar/core/jobs.hpp>

#include <algorithm>

namespace lunar
{
	namespace
	{
		constexpr size_t MAIN_THREAD_COUNT = 1;
		constexpr size_t MIN_WORKER_COUNT  = 1;
	}

	JobSystem::JobSystem(size_t worker_count) noexcept
	{
		const size_t count = std::max(worker_count, MIN_WORKER_COUNT);

		workers.reserve(count);
		for (size_t index = 0; index < count; index++)
			workers.emplace_back([this](std::stop_token stop_token) { runWorker(stop_token); });
	}

	JobSystem::~JobSystem() noexcept
	{
		{
			const std::lock_guard lock(mutex);
			pendingJobs.clear();
		}

		workers.clear();
	}

	void JobSystem::cancel(JobHandle job)
	{
		{
			const std::lock_guard lock(mutex);

			std::erase_if(pendingJobs,   [&](const PendingJob& pending)     { return pending.id == job.id; });
			std::erase_if(completedJobs, [&](const CompletedJob& completed) { return completed.id == job.id; });

			if (runningJobs.contains(job.id))
				droppedJobs.insert(job.id);
		}

		idle.notify_all();
	}

	size_t JobSystem::processCompleted()
	{
		std::vector<CompletedJob> completed;
		{
			const std::lock_guard lock(mutex);
			completed.swap(completedJobs);
		}

		for (CompletedJob& job : completed)
			job.completion();

		return completed.size();
	}

	void JobSystem::waitIdle()
	{
		std::unique_lock lock(mutex);
		idle.wait(lock, [&] { return pendingJobs.empty() && runningJobs.empty(); });
	}

	size_t JobSystem::getWorkerCount() const
	{
		return workers.size();
	}

	size_t JobSystem::defaultWorkerCount()
	{
		const size_t hardware_threads = std::thread::hardware_concurrency();
		return hardware_threads > MAIN_THREAD_COUNT ? hardware_threads - MAIN_THREAD_COUNT : MIN_WORKER_COUNT;
	}

	JobHandle JobSystem::enqueue(Task task)
	{
		JobHandle handle;
		{
			const std::lock_guard lock(mutex);
			handle = { nextJobId++ };
			pendingJobs.push_back({ handle.id, std::move(task) });
		}

		workAvailable.notify_one();
		return handle;
	}

	void JobSystem::runWorker(std::stop_token stop_token)
	{
		while (std::optional<PendingJob> job = takeJob(stop_token))
			finishJob(job->id, job->task());
	}

	std::optional<JobSystem::PendingJob> JobSystem::takeJob(std::stop_token stop_token)
	{
		std::unique_lock lock(mutex);
		if (!workAvailable.wait(lock, stop_token, [&] { return !pendingJobs.empty(); }))
			return std::nullopt;

		PendingJob job = std::move(pendingJobs.front());
		pendingJobs.pop_front();
		runningJobs.insert(job.id);

		return job;
	}

	void JobSystem::finishJob(uint64_t id, Completion completion)
	{
		{
			const std::lock_guard lock(mutex);
			runningJobs.erase(id);

			if (droppedJobs.erase(id) == 0)
				completedJobs.push_back({ id, std::move(completion) });
		}

		idle.notify_all();
	}
}
