## BlockingQueue

BlockingQueue simply adapts a queue to provide thread-safe functionality.

The internal queue is protected by a mutex for all operations to
ensure that no data races occur. In addition, threads attempting
to pop an item off of an empty queue using `pop()` will sleep until
an item has been pushed onto the queue by another thread.

To atomically push or pop multiple items in bulk, one can use `lock()`
and `unlock()` (or a `std::unique_lock` object) on the queue itself, and
other threads are guaranteed not to interfere.

Queue must support `push()`, `pop()`, `front()`, `empty()`, and `Queue(Queue&&)`.

To use, simply include `BlockingQueue.h` in your project.
