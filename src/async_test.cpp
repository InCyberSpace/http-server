#include "async.hpp"

#include <coroutine>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

namespace
{
	class TaskError final : public std::runtime_error
	{
	  public:
		TaskError() : std::runtime_error("original task failure")
		{
		}

		const int code = 42;
	};

	template <typename T, typename Exception = TaskError>
	async_lib::Task<T> failing_task(const bool suspend, Exception error = {})
	{
		if (suspend)
		{
			co_await std::suspend_always{};
		}
		throw error;
	}

	template <typename T>
	async_lib::Task<void> await_task(async_lib::Task<T> &task)
	{
		co_await task;
	}

	template <typename T>
	void expect_original_exception(async_lib::Task<T> &task)
	{
		ASSERT_TRUE(task.is_done());
		try
		{
			task.await_resume();
			FAIL() << "The task did not rethrow its exception";
		}
		catch (const TaskError &error)
		{
			EXPECT_STREQ(error.what(), "original task failure");
			EXPECT_EQ(error.code, 42);
		}
		catch (...)
		{
			FAIL() << "The task changed the exception type";
		}
	}

	void expect_current_exception()
	{
		try
		{
			throw;
		}
		catch (const TaskError &error)
		{
			EXPECT_STREQ(error.what(), "original task failure");
			EXPECT_EQ(error.code, 42);
		}
		catch (...)
		{
			FAIL() << "The coroutine changed the exception type";
		}
	}

	template <typename T>
	async_lib::Task<void> catch_task_in_loop(async_lib::Task<T> &task,
											 int &caught_count)
	{
		for (int i = 0; i != 2; ++i)
		{
			try
			{
				co_await task;
			}
			catch (const std::exception &)
			{
				// Inspect the exception outside the coroutine to avoid LLVM
				// #182584.
				expect_current_exception();
				++caught_count;
			}
		}
	}

	template <typename T> class async_exception_test : public ::testing::Test
	{
	};
	using TaskResultTypes = ::testing::Types<int, void>;
	TYPED_TEST_SUITE(async_exception_test, TaskResultTypes);

	TYPED_TEST(async_exception_test, preserves_original_exception)
	{
		for (const bool suspend : {false, true})
		{
			SCOPED_TRACE(suspend);
			auto task = failing_task<TypeParam>(suspend);
			ASSERT_EQ(task.is_done(), !suspend);
			if (suspend)
			{
				task.resume();
			}
			expect_original_exception(task);
		}
	}

	TYPED_TEST(async_exception_test,
			   propagates_original_exception_through_co_await)
	{
		for (const bool suspend : {false, true})
		{
			SCOPED_TRACE(suspend);
			auto task = failing_task<TypeParam>(suspend);
			auto caller = await_task(task);
			ASSERT_EQ(caller.is_done(), !suspend);
			if (suspend)
			{
				task.resume();
			}
			expect_original_exception(caller);
		}
	}

	TYPED_TEST(async_exception_test, preserves_non_standard_exception)
	{
		for (const bool suspend : {false, true})
		{
			SCOPED_TRACE(suspend);
			auto task = failing_task<TypeParam>(suspend, 17);
			if (suspend)
			{
				task.resume();
			}
			ASSERT_TRUE(task.is_done());
			try
			{
				task.await_resume();
				FAIL() << "The task did not rethrow its exception";
			}
			catch (const int error)
			{
				EXPECT_EQ(error, 17);
			}
			catch (...)
			{
				FAIL() << "The task changed the exception type";
			}
		}
	}

	TYPED_TEST(async_exception_test,
			   propagates_original_exception_across_threads)
	{
		auto task = failing_task<TypeParam>(true);
		auto caller = await_task(task);
		ASSERT_FALSE(caller.is_done());
		std::thread worker([&task] { task.resume(); });
		worker.join();
		expect_original_exception(caller);
	}

	TYPED_TEST(async_exception_test,
			   catches_original_exception_in_coroutine_loop)
	{
		for (const bool suspend : {false, true})
		{
			SCOPED_TRACE(suspend);
			auto task = failing_task<TypeParam>(suspend);
			int caught_count = 0;
			auto caller = catch_task_in_loop(task, caught_count);
			ASSERT_EQ(caller.is_done(), !suspend);
			if (suspend)
			{
				task.resume();
			}
			ASSERT_TRUE(caller.is_done());
			EXPECT_NO_THROW(caller.await_resume());
			EXPECT_EQ(caught_count, 2);
		}
	}
} // namespace
