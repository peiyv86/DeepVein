#include "task_factory.h"
#include "task/actions/action_semantic.h"
#include "task/actions/action_exact.h"
#include "task/actions/action_chat.h"
#include "task/actions/action_file_analysis.h"
#include "task/actions/action_list_search.h"
#include "task/actions/action_hybrid_compare.h"

std::unique_ptr<ITaskRouter> TaskFactory::createRouter(IntentType intentType) {
    switch (intentType) {
    case IntentType::SemanticSearch:
        return std::make_unique<ActionSemantic>();
    case IntentType::ExactMatch:
        return std::make_unique<ActionExact>();
    case IntentType::DocumentInsight:
        return std::make_unique<ActionFileAnalysis>();
    case IntentType::DirectChat:
        return std::make_unique<ActionChat>();
    case IntentType::ListCrossSearch:
        return std::make_unique<ActionListCrossSearch>();
    case IntentType::HybridCompare:
        return std::make_unique<ActionHybridCompare>();
    case IntentType::Unknown:
    default:
        return std::make_unique<ActionChat>();
    }
}
