/**
 * @file event_handler_list.h
 * @author Marcel Flottmann
 * @date 2020-09-05
 */

#pragma once

#include <functional>
#include <list>
#include <mutex>

namespace fastsense::util
{

/**
 * @brief Handle to remove event handler when destroyed or passed to the remove method
 *
 * @tparam T function type of the evnt handler
 */
template<typename T>
class EventHandlerHandle
{
private:
    /// Allow private access only from EventHandlerList
    template<typename T_>
    friend class EventHandlerList;

    /// Iterator of added event handler
    typename std::list<std::function<T>>::iterator iterator;
public:
    /**
     * @brief Create new handle
     *
     * @param it Iterator of the associated event handler
     */
    explicit EventHandlerHandle(typename std::list<std::function<T>>::iterator it);

    /**
     * @brief Create new handle from moved object
     *
     * @param rhs Object to move from
     */
    EventHandlerHandle(EventHandlerHandle&& rhs);

    /**
     * @brief Move assign with other object
     *
     * @param rhs Object to move from
     * @return EventHandlerHandle<T>& Reference to *this
     */
    EventHandlerHandle<T>& operator=(EventHandlerHandle&& rhs);
};

/**
 * @brief Manage a list of event handlers
 *
 * @tparam T Function type
 */
template<typename T>
class EventHandlerList
{
    /// List with handlers
    std::list<std::function<T>> callbacks;

    /// Mutex to access list
    std::mutex mtx;
public:
    EventHandlerList() = default;
    ~EventHandlerList() = default;
    
    /**
     * @brief Add event handler
     *
     * @param callback Handler to add
     * @return EventHandlerHandle<T> Handle of the registered event handler
     */
    EventHandlerHandle<T> add(const std::function<T>& callback);

    /**
     * @brief Remove event handler
     *
     * @param handle Handle of the event handler that was created by add
     */
    void remove(EventHandlerHandle<T>&& handle);

    /**
     * @brief Invoke all event handlers
     *
     * @tparam Args Types of the arguments
     * @param args Argument list to call handlers with
     */
    template<typename ...Args>
    void invoke(Args&& ... args);
};

} // namespace fastsense::util

#include "event_handler_list.tcc"