#pragma once

#include "viewmodel/contracts/AppearanceSettingsContract.h"
#include "viewmodel/contracts/TaskListContract.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskEditorContract.h"
#include "viewmodel/contracts/TaskCategoryContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskGraphContract.h"

namespace smartmate::view::widgets {

/// 主窗口当前纵向切片所需的最小依赖集合。
///
/// 组合根负责保证所有 Contract 的生命周期长于 MainWindow；后续页面迁移时
/// 只按实际使用增加新的窄 Contract，不提前注入具体 ViewModel。
struct MainWindowDependencies {
    viewmodel::AppearanceSettingsContract &appearanceSettings;
    viewmodel::TaskListContract &taskList;
    viewmodel::TaskFocusContract &taskFocus;
    viewmodel::TaskDetailsContract &taskDetails;
    viewmodel::TaskEditorContract &taskEditor;
    viewmodel::TaskCategoryContract &taskCategories;
    viewmodel::TaskDependencyContract &taskDependencies;
    viewmodel::TaskGraphContract &taskGraph;
};

} // namespace smartmate::view::widgets
