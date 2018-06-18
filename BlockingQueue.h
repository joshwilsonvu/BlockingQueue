//
// Created by Josh Wilson on 6/26/17.
//

#ifndef BLOCKINGQUEUE_H
#define BLOCKINGQUEUE_H

#include <queue>
#include <condition_variable>

/*
 * BlockingQueue simply adapts a queue to provide thread-safe functionality.
 *
 * The internal queue is protected by a mutex for all operations to
 * ensure that no data races occur. In addition, threads attempting
 * to pop an item off of an empty queue using pop() will sleep until
 * an item has been pushed by another thread.
 *
 * To atomically push or pop multiple items in bulk, one can use lock()
 * and unlock() (or a std::unique_lock object) on the queue itself, and
 * other threads are guaranteed not to interfere.
 *
 * Queue must support push(), pop(), front(), empty(), and Queue(Queue&&).
 */
template <typename T, class Queue = std::queue<T>>
class BlockingQueue {
public:
    using value_type = T;

    BlockingQueue();
    ~BlockingQueue() = default;
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue(BlockingQueue&&) noexcept;

    // adds an item to the queue
    void push(T const& item);
    void push(T&& item);

    // removes an item from the queue and returns it as an rvalue
    T pop();
    T try_pop();

    // removes all elements from the queue
    void clear();

    // returns true if the queue currently contains no elements
    bool empty();

    // prevent and reallow modification of the queue
    void lock();
    bool try_lock();
    void unlock();

    // returns true if the calling thread has locked the queue
    bool owns_lock() const;

private:
    // If the calling thread already owns the lock, a lock initialized with
    // std::deferred_lock is returned.
    // Otherwise, the lock is initialized normally.
    std::unique_lock<BlockingQueue<T, Queue>> get_lock();

    // All impl methods assume the queue is already locked.
    void push_impl(T const& item);
    void push_impl(T&& item);
    T pop_impl();
    T try_pop_impl();
    void clear_impl();
    bool empty_impl();

    // the underlying container
    Queue q;
    // protects the queue
    std::mutex m;
    // notifies threads waiting on pop() that the queue is no longer empty
    std::condition_variable_any cv;
    // stores the thread id of the current owner
    std::atomic<std::thread::id> owningThread;
};

/*
 * A lightweight adaptor that makes std::priority_queue fit the std::queue interface
 * required by BlockingQueue. Because only named objects of this type will be used,
 * polymorphism (especially in the destructor) is not necessary.
 */
template <class PriorityQueue>
class PriorityQueueWrapper : public PriorityQueue
{
public:
    typedef typename PriorityQueue::value_type value_type;
    const value_type& front() const;
};

/*
 * Convenience typedef for using a priority queue in a BlockingQueue.
 */
template <typename T, class PriorityQueue = std::priority_queue<T>>
using BlockingPriorityQueue = BlockingQueue<T, PriorityQueueWrapper<PriorityQueue>>;


// BlockingQueue Implementation
template <typename T, class Queue>
BlockingQueue<T, Queue>::BlockingQueue()
        : owningThread(std::thread::id())
{
}

template <typename T, class Queue>
BlockingQueue<T, Queue>::BlockingQueue(BlockingQueue&& other) noexcept
        : owningThread(std::thread::id())
{
    auto locker = other.get_lock();
    q = std::move(other.q);
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::push(T const& item)
{
    auto locker = get_lock();
    push_impl(item);
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::push(T&& item)
{
    auto locker = get_lock();
    push_impl(std::forward<T>(item));
}

template <typename T, class Queue>
T BlockingQueue<T, Queue>::pop()
{
    auto locker = get_lock();
    return pop_impl();
}

template <typename T, class Queue>
T BlockingQueue<T, Queue>::try_pop()
{
    auto locker = get_lock();
    return try_pop_impl();
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::clear()
{
    auto locker = get_lock();
    clear_impl();
};

template <typename T, class Queue>
bool BlockingQueue<T, Queue>::empty()
{
    auto locker = get_lock();
    return empty_impl();
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::lock()
{
    m.lock();
    owningThread.store(std::this_thread::get_id());
}

template <typename T, class Queue>
bool BlockingQueue<T, Queue>::try_lock()
{
    if (m.try_lock()) {
        owningThread.store(std::this_thread::get_id());
        return true;
    }
    return false;
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::unlock()
{
    owningThread.store(std::thread::id());
    m.unlock();
}

template <typename T, class Queue>
bool BlockingQueue<T, Queue>::owns_lock() const
{
    return owningThread.load() == std::this_thread::get_id();
}

template <typename T, class Queue>
std::unique_lock<BlockingQueue<T, Queue>> BlockingQueue<T, Queue>::get_lock()
{
    using lock_t = std::unique_lock<BlockingQueue<T, Queue>>;

    if (owns_lock()) {
        // the queue is already locked, do not relock it or unlock it on destruction
        return lock_t(*this, std::defer_lock);
    } else {
        // lock the queue
        return lock_t(*this);
    }
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::push_impl(T const& item)
{
    // if the queue is empty, threads might be waiting to pop
    bool wasEmpty = empty_impl();
    q.push(item);
    if (wasEmpty) {
        // wake up a thread waiting to pop, if any
        cv.notify_one();
    }
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::push_impl(T&& item)
{
    // if the queue is empty, threads might be waiting to pop
    bool wasEmpty = empty_impl();
    q.push(std::forward<T>(item));
    if (wasEmpty) {
        // wake up a thread waiting to pop, if any
        cv.notify_one();
    }
}

template <typename T, class Queue>
T BlockingQueue<T, Queue>::pop_impl()
{
    // assuming *this is already locked, create a std::unique_lock for cv.wait()
    auto locker = std::unique_lock<typename std::remove_reference<decltype(*this)>::type>(*this,
            std::adopt_lock);

    // wait until the queue is not empty
    cv.wait(locker, [this] () {
        return !empty_impl();
    });

    // disassociate the queue from the lock without unlocking it
    locker.release();

    return try_pop_impl();
}

template <typename T, class Queue>
T BlockingQueue<T, Queue>::try_pop_impl()
{
    if (empty_impl()) {
        throw std::out_of_range("BlockingQueue is empty.");
    }
    // pop the front item off the queue and return it
    auto temp = std::move(q.front());
    q.pop();
    return std::move(temp);
}

template <typename T, class Queue>
void BlockingQueue<T, Queue>::clear_impl()
{
    // assuming *this is already locked, pop all of the queue's elements
    while (!empty_impl()) {
        pop_impl();
    }
}

template <typename T, class Queue>
bool BlockingQueue<T, Queue>::empty_impl()
{
    return q.empty();
}


// PriorityQueue Wrapper Implementation - simply adds front() method that uses top()
template <class PriorityQueue>
typename PriorityQueueWrapper<PriorityQueue>::value_type const&
PriorityQueueWrapper<PriorityQueue>::front() const
{
    return PriorityQueue::top();
}

#endif // BLOCKINGQUEUE_H
