#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <variant>
#include <vector>

namespace LoopScheduler
{
    class SequentialGroup final : public Group
    {
    public:
        SequentialGroup(std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>>);
    protected:
        virtual std::weak_ptr<Module> GetNextModule() override;
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsDone() override;
        virtual void WaitForNextModule() override;
        virtual void StartNextIteration() override;
    private:
        std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members;
        std::mutex Mutex;
        std::condition_variable ConditionVariable;
        std::shared_mutex SharedMutex;
        int CurrentMemberIndex;
        int RunningThreadsCount;
        bool IsCurrentMemberGroup;

        int GetNextModuleIndex();
    };
}
