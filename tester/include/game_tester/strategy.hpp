#pragma once

#include <game_tester/game_tester.hpp>

namespace testing::strategy {

    enum Result {
        End,
        Abort,
        Continue,
    };

    using session_id_t = uint32_t;
    using condition_t = std::function<bool(const game_tester &)>;
    using action_t = std::function<Result(game_tester &, const session_id_t)>;

    class Node {
    public:
        explicit Node(action_t && action) : _action(std::move(action)) {
        }

        std::shared_ptr<Node> traversal(const game_tester & tester) {
            for (const auto&[condition, child] : _children) {
                if (condition(tester)) {
                    return child;
                }
            }

            return nullptr;
        }

        std::shared_ptr<Node> push_child(condition_t && condition, action_t && action) {
            push_child(std::move(condition), std::make_shared<Node>(std::move(action)));
            return _children.back().second;
        }

        void push_child(condition_t && condition, const std::shared_ptr<Node> & new_child) {
            _children.emplace_back(std::move(condition), new_child);
        }

        const action_t & get_action() {
            return _action;
        }

    private:
        action_t _action;

        using edge_t = std::pair<condition_t, std::shared_ptr<Node>>;
        std::vector<edge_t> _children;
    };

    struct Graph {
        std::shared_ptr<Node> _root;
    };

    class Executor {
    public:
        explicit Executor(Graph && graph) : _graph(graph) {
            _current = graph._root;
        }

        uint process_strategy(game_tester & tester,
                              const uint run_count,
                              const uint limit_per_run,
                              std::function<session_id_t(game_tester &, const uint)> && pre_run_callback,
                              std::function<void(const game_tester &)> && post_run_callback) {

            for (uint run = 0; run != run_count; ++run) {
                const auto session_id = pre_run_callback(tester, run);

                if (!process_run(tester, limit_per_run))
                    return run;

                post_run_callback(tester);
            }

            return run_count;
        }

    private:
        bool process_run(game_tester & tester, const session_id_t session_id, uint limit) {
            while (limit-- != 0) {
                switch (process_next_step(tester, session_id)) {
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

        strategy::Result process_next_step(game_tester & tester, const session_id_t session_id) {
            if (_current != nullptr) {
                const auto result = _current->get_action()(tester, session_id);

                if (result == Result::Continue) {
                    if (_current = _current->traversal(tester); _current == nullptr) {
                        _current = _graph._root;
                        return Result::End;
                    }
                }

                return result;
            }

            return Result::Abort;
        }

    private:
        Graph _graph;
        std::shared_ptr<Node> _current;
    };
}
