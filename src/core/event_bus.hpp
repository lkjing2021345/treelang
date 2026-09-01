#ifndef INCLUDE_TREELANG_CORE_EVENT_BUS_HPP
#define INCLUDE_TREELANG_CORE_EVENT_BUS_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "core/event.hpp"
#include "core/marco.hpp"

namespace treelang
{
    template <typename T>
    class CommonEventBus
    {
    public:
        using Key = std::type_index;
        using Callback = std::function<void(const std::shared_ptr<T> &)>;
        using CallbackList = std::vector<Callback>;
        using ListenerTable = std::unordered_map<Key, CallbackList>;

    private:
        ListenerTable listeners;
        std::uint64_t m_sequence = 0; /**< 已分配的最大事件序号 */

    public:
        DEFAULT_CONSTRUCTOR(CommonEventBus)

    public:
        /** @brief 当前已分配的最大事件序号。 */
        std::uint64_t sequence() const noexcept { return m_sequence; }

    public:
        template <typename U, typename Func>
            requires(std::is_base_of_v<T, U> && std::is_invocable_v<Func, U *>)
        void subscribe(Func &&func)
        {
            listeners[typeid(U)].push_back([func](const std::shared_ptr<T> &event)
                                           { func(static_cast<U *>(event.get())); });
        }

        template <typename U>
            requires(std::is_base_of_v<T, U> && requires(T &t) { t.set_sequence(std::uint64_t{}); })
        void publish(const std::shared_ptr<U> &event)
        {
            event->set_sequence(++m_sequence);
            auto it = listeners.find(typeid(U));
            if (it == listeners.end())
                return;
            for (auto &listener : it->second) listener(event);
        }
    };

    using EventBus = CommonEventBus<Event>;
}

#endif  // INCLUDE_TREELANG_CORE_EVENT_BUS_HPP