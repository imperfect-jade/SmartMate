#include "services/TaskService.h"

#include "services/TaskDraftValidator.h"

namespace smartmate::model {

TaskService::TaskService(ITaskRepository &repository,
                         ITaskDependencyRepository &dependencyRepository,
                         ITaskCreationRepository &creationRepository,
                         ITaskBatchTransitionRepository &batchTransitionRepository,
                         ITaskDeletionRepository &deletionRepository,
                         ITaskCategoryRepository &categoryRepository,
                         QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_dependencyRepository(dependencyRepository)
    , m_creationRepository(creationRepository)
    , m_batchTransitionRepository(batchTransitionRepository)
    , m_deletionRepository(deletionRepository)
    , m_categoryRepository(categoryRepository)
{
}

TaskValidationResult TaskService::validateDraft(const TaskDraft &draft) const
{
    return validateTaskDraft(draft);
}

TaskValidationResult TaskService::validateEstimatedMinutes(const int minutes) const
{
    return validateTaskEstimatedMinutes(minutes);
}

} // namespace smartmate::model
