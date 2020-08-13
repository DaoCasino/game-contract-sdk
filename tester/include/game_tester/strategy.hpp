#pragma once

#include <game_tester/game_tester.hpp>

namespace testing::strategy {

enum Result {
    End,
    Abort,
    Continue,
};

using session_id_t = uint32_t;
using condition_t = std::function<bool(const game_tester&)>;
using action_t = std::function<Result(game_tester&, const session_id_t)>;

class Node {
  public:
    explicit Node(action_t&& action) : _action(std::move(action)) {}

    std::shared_ptr<Node> traversal(const game_tester& tester) {
        for (const auto& [condition, child] : _children) {
            if (condition(tester)) {
                return child;
            }
        }

        return nullptr;
    }

    std::shared_ptr<Node> push_child(condition_t&& condition, action_t&& action) {
        push_child(std::move(condition), std::make_shared<Node>(std::move(action)));
        return _children.back().second;
    }

    void push_child(condition_t&& condition, const std::shared_ptr<Node>& new_child) {
        _children.emplace_back(std::move(condition), new_child);
    }

    const action_t& get_action() { return _action; }

  private:
    action_t _action;

    using edge_t = std::pair<condition_t, std::shared_ptr<Node>>;
    std::vector<edge_t> _children;
};

struct Graph {
    explicit Graph(action_t&& root_action) : root(std::make_shared<strategy::Node>(std::move(root_action))) {}

    std::shared_ptr<Node> root;
};

class Executor {
  public:
    explicit Executor(Graph&& graph) : _graph(graph) {}

    uint process_strategy(game_tester& tester,
                          const uint run_count,
                          const uint limit_per_run,
                          std::function<session_id_t(game_tester&, const uint)>&& session_create,
                          std::function<void(game_tester&, const session_id_t)>&& session_close) {

        for (uint run = 0; run != run_count; ++run) {
            const auto session_id = session_create(tester, run);

            if (!execute_to_end(tester, _graph.root, session_id, limit_per_run)) {
                return run;
            }

            session_close(tester, session_id);

            if (run % 500 == 0) {
                BOOST_TEST_MESSAGE(run << " rounds passed");
            }
        }

        return run_count;
    }

  private:
    static bool
    execute_to_end(game_tester& tester, std::shared_ptr<Node> current, const session_id_t session_id, uint limit) {

        while (limit-- != 0) {
            switch (process_next_step(tester, session_id, current)) {
            case Result::Continue:
                continue;
            case Result::End:
                return true;
            case Result::Abort:
                return false;
            }
        }

        return false;
    }

    static strategy::Result
    process_next_step(game_tester& tester, const session_id_t session_id, std::shared_ptr<Node>& current) {

        if (current != nullptr) {
            const auto result = current->get_action()(tester, session_id);

            if (result == Result::Continue) {
                if (current = current->traversal(tester); current == nullptr) {
                    return Result::End;
                }
            }

            return result;
        }

        return Result::Abort;
    }

  private:
    Graph _graph;
};
} // namespace testing::strategy
