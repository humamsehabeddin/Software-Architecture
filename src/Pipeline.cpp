#include "logflow/Pipeline.hpp"

#include <exception>

namespace logflow {

Pipeline::Pipeline(std::function<void()> body, std::vector<detail::StageHooks> lifecycle,
                   std::size_t stageCount)
    : body_(std::move(body)), lifecycle_(std::move(lifecycle)), stageCount_(stageCount) {}

void Pipeline::run() const {
    std::size_t opened = 0;
    try {
        for (; opened < lifecycle_.size(); ++opened) {
            lifecycle_[opened].open();
        }
        body_();
    } catch (...) {
        // Something already failed: clean up what we opened, but keep the
        // original error as the one the caller sees.
        closeOpened(lifecycle_, opened, /*rethrow=*/false);
        throw;
    }
    // Normal completion: a failing close() is a real error.
    closeOpened(lifecycle_, opened, /*rethrow=*/true);
}

void Pipeline::closeOpened(const std::vector<detail::StageHooks>& lifecycle,
                           std::size_t opened, bool rethrow) {
    std::exception_ptr first;
    for (std::size_t i = opened; i > 0; --i) {  // reverse order
        try {
            lifecycle[i - 1].close();
        } catch (...) {
            if (!first) first = std::current_exception();
        }
    }
    if (rethrow && first) std::rethrow_exception(first);
}

}  // namespace logflow
