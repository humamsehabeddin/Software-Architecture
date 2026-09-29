#include "logflow/Pipeline.hpp"

#include <exception>

namespace logflow {

Pipeline::Pipeline(std::function<void()> body, std::vector<detail::StageHooks> stages)
    : body_(std::move(body)), stages_(std::move(stages)) {}

void Pipeline::run() const {
    std::size_t opened = 0;
    try {
        for (; opened < stages_.size(); ++opened) {
            stages_[opened].open();
        }
        body_();
    } catch (...) {
        // Something already failed: clean up what we opened, but keep the
        // original error as the one the caller sees.
        closeOpened(stages_, opened, /*rethrow=*/false);
        throw;
    }
    // Normal completion: a failing close() is a real error.
    closeOpened(stages_, opened, /*rethrow=*/true);
}

void Pipeline::closeOpened(const std::vector<detail::StageHooks>& stages,
                           std::size_t opened, bool rethrow) {
    std::exception_ptr first;
    for (std::size_t i = opened; i > 0; --i) {  // reverse order
        try {
            stages[i - 1].close();
        } catch (...) {
            if (!first) first = std::current_exception();
        }
    }
    if (rethrow && first) std::rethrow_exception(first);
}

}  // namespace logflow
