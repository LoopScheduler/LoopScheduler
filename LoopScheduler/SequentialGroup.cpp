#include "SequentialGroup.h"

#include <thread>
#include <utility>

#include "Module.h"

namespace LoopScheduler
{
    SequentialGroup::SequentialGroup(
            std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members
        ) : Members(Members), CurrentMemberIndex(-1), CurrentMemberRunsCount(0), RunningThreadsCount(0)
    {
    }

    class IncrementGuardLockingOnDecrement
    {
    public:
        IncrementGuardLockingOnDecrement(int& num, std::unique_lock<std::shared_mutex>& lock) : num(num), lock(lock)
        {
            num++;
        }
        ~IncrementGuardLockingOnDecrement()
        {
            lock.lock();
            num--;
            lock.unlock();
        }
    private:
        int& num;
        std::unique_lock<std::shared_mutex>& lock;
    };

    bool SequentialGroup::RunNextModule(double MaxEstimatedExecutionTime)
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (ShouldIncrementCurrentMemberIndex())
        {
            CurrentMemberIndex++;
            CurrentMemberRunsCount = 0;
        }
        if (ShouldRunNextModuleFromCurrentMemberIndex())
        {
            {
                IncrementGuardLockingOnDecrement increment_guard(RunningThreadsCount, lock);
                CurrentMemberRunsCount++;
                lock.unlock();
                std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex])->Run();
            }
            NextEventConditionVariable.notify_all();
            return true;
        }
        else if (ShouldTryRunNextGroupFromCurrentMemberIndex())
        {
            bool success = false;
            {
                IncrementGuardLockingOnDecrement increment_guard(RunningThreadsCount, lock);
                CurrentMemberRunsCount++;
                lock.unlock();
                success = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->RunNextModule();
            }
            NextEventConditionVariable.notify_all();
            return success;
        }
        return false;
    }

    void SequentialGroup::WaitForNextEvent()
    {
        std::unique_lock<std::mutex> lock(NextEventConditionMutex);
        NextEventConditionVariable.wait(lock, [this] {
            std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
            if (ShouldIncrementCurrentMemberIndex()
                || ShouldRunNextModuleFromCurrentMemberIndex()) // There is a next module to run.
            {
                return true;
            }
            if (ShouldTryRunNextGroupFromCurrentMemberIndex()) // Can wait for the group.
            {
                lock.unlock();
                std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->WaitForNextEvent();
                return true;
            }
            if ((CurrentMemberIndex == Members.size() - 1)
                && (RunningThreadsCount == 0)); // (And no next module to run) => IsDone=true.
            {
                return true;
            }
            return false;
        });
    }

    bool SequentialGroup::IsDone()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return (CurrentMemberIndex == Members.size() - 1)
               && (RunningThreadsCount == 0)
               && (
                    CurrentMemberIndex == -1
                    || (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]) ?
                        (CurrentMemberRunsCount != 0)
                        : (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone()))
                );
    }

    void SequentialGroup::StartNextIteration()
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        CurrentMemberIndex = -1;
    }

    inline bool SequentialGroup::ShouldRunNextModuleFromCurrentMemberIndex()
    {
        // NO MUTEX LOCK
        return RunningThreadsCount == 0 && CurrentMemberRunsCount == 0
            && CurrentMemberIndex != -1
            && std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]);
    }
    inline bool SequentialGroup::ShouldTryRunNextGroupFromCurrentMemberIndex()
    {
        // NO MUTEX LOCK
        return CurrentMemberIndex != -1
            && std::holds_alternative<std::shared_ptr<Group>>(Members[CurrentMemberIndex])
            && !std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone();
    }
    inline bool SequentialGroup::ShouldIncrementCurrentMemberIndex()
    {
        // NO MUTEX LOCK
        return (RunningThreadsCount == 0)
            && (CurrentMemberIndex < Members.size() - 1)
            && (
                CurrentMemberIndex == -1
                || (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]) ?
                    (CurrentMemberRunsCount != 0)
                    : (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone()))
            );
    }
}
