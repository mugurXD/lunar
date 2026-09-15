#include <lunar/core/jobs.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <semaphore>
#include <thread>

namespace
{
	constexpr size_t SINGLE_WORKER     = 1;
	constexpr size_t MULTIPLE_WORKERS  = 4;
	constexpr int    MANY_JOBS         = 1000;
	constexpr int    WORK_RESULT       = 42;
}

TEST(JobSystem, CreatesAtLeastOneWorker)
{
	lunar::JobSystem jobs(0);

	EXPECT_GE(jobs.getWorkerCount(), SINGLE_WORKER);
	EXPECT_GE(lunar::JobSystem::defaultWorkerCount(), SINGLE_WORKER);
}

TEST(JobSystem, WorkRunsOnWorkerAndCompletionOnCallingThread)
{
	lunar::JobSystem      jobs(MULTIPLE_WORKERS);
	const std::thread::id main_thread     = std::this_thread::get_id();
	std::thread::id       work_thread     = {};
	std::thread::id       complete_thread = {};

	jobs.submit(
		[] { return std::this_thread::get_id(); },
		[&](std::thread::id id) { work_thread = id; complete_thread = std::this_thread::get_id(); }
	);

	jobs.waitIdle();
	EXPECT_EQ(jobs.processCompleted(), 1u);
	EXPECT_NE(work_thread, main_thread);
	EXPECT_EQ(complete_thread, main_thread);
}

TEST(JobSystem, CompletionRunsOnlyWhenProcessed)
{
	lunar::JobSystem jobs(MULTIPLE_WORKERS);
	int              result = 0;

	jobs.submit([] { return WORK_RESULT; }, [&](int value) { result = value; });
	jobs.waitIdle();

	EXPECT_EQ(result, 0);
	jobs.processCompleted();
	EXPECT_EQ(result, WORK_RESULT);
}

TEST(JobSystem, VoidWorkIsSupported)
{
	lunar::JobSystem jobs(MULTIPLE_WORKERS);
	std::atomic_bool worked    = false;
	bool             completed = false;

	jobs.submit([&] { worked = true; }, [&] { completed = true; });
	jobs.waitIdle();
	jobs.processCompleted();

	EXPECT_TRUE(worked);
	EXPECT_TRUE(completed);
}

TEST(JobSystem, AllJobsComplete)
{
	lunar::JobSystem jobs(MULTIPLE_WORKERS);
	int              sum = 0;

	for (int index = 0; index < MANY_JOBS; index++)
		jobs.submit([index] { return index; }, [&](int value) { sum += value; });

	jobs.waitIdle();
	EXPECT_EQ(jobs.processCompleted(), static_cast<size_t>(MANY_JOBS));
	EXPECT_EQ(sum, MANY_JOBS * (MANY_JOBS - 1) / 2);
}

TEST(JobSystem, CompletionCanSubmitFollowUpJobs)
{
	lunar::JobSystem jobs(MULTIPLE_WORKERS);
	int              result = 0;

	jobs.submit([] { return WORK_RESULT; }, [&](int value) {
		jobs.submit([value] { return value + 1; }, [&](int follow_up) { result = follow_up; });
	});

	jobs.waitIdle();
	jobs.processCompleted();
	jobs.waitIdle();
	jobs.processCompleted();

	EXPECT_EQ(result, WORK_RESULT + 1);
}

TEST(JobSystem, CancelledPendingJobNeverRuns)
{
	lunar::JobSystem      jobs(SINGLE_WORKER);
	std::binary_semaphore release(0);
	std::atomic_bool      cancelled_ran = false;
	bool                  completed     = false;

	jobs.submit([&] { release.acquire(); }, [] {});
	const lunar::JobHandle pending = jobs.submit([&] { cancelled_ran = true; }, [&] { completed = true; });

	jobs.cancel(pending);
	release.release();
	jobs.waitIdle();
	jobs.processCompleted();

	EXPECT_FALSE(cancelled_ran);
	EXPECT_FALSE(completed);
}

TEST(JobSystem, CancelledRunningJobDropsItsCompletion)
{
	lunar::JobSystem      jobs(SINGLE_WORKER);
	std::binary_semaphore started(0);
	std::binary_semaphore release(0);
	bool                  completed = false;

	const lunar::JobHandle running = jobs.submit([&] { started.release(); release.acquire(); }, [&] { completed = true; });

	started.acquire();
	jobs.cancel(running);
	release.release();
	jobs.waitIdle();

	EXPECT_EQ(jobs.processCompleted(), 0u);
	EXPECT_FALSE(completed);
}

TEST(JobSystem, CancelledFinishedJobDropsItsCompletion)
{
	lunar::JobSystem jobs(MULTIPLE_WORKERS);
	bool             completed = false;

	const lunar::JobHandle finished = jobs.submit([] {}, [&] { completed = true; });
	jobs.waitIdle();
	jobs.cancel(finished);

	EXPECT_EQ(jobs.processCompleted(), 0u);
	EXPECT_FALSE(completed);
}

TEST(JobSystem, DestroyingWithPendingJobsDoesNotRunTheirCompletions)
{
	bool completed = false;
	{
		lunar::JobSystem jobs(SINGLE_WORKER);
		for (int index = 0; index < MANY_JOBS; index++)
			jobs.submit([] {}, [&] { completed = true; });
	}

	EXPECT_FALSE(completed);
}
