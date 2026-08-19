
ThreadPool(int numThreads) : stop(false)
初始化列表（member initializer list），在构造函数体执行前初始化成员变量，比在函数体里赋值更高效。
threads.emplace_back([this]{ ... })
emplace_back：直接在容器内构造对象，避免多余的拷贝/移动（对比 push_back）
[this]：lambda 按引用捕获 this 指针，让 lambda 内部能访问外层类的成员（tasks、mtx、condition）
这个 lambda 本身会被隐式转换成 std::function<void()>（因为线程构造函数需要可调用对象）
std::unique_lock<std::mutex> lock(mtx)
unique_lock：比 lock_guard 更灵活的锁包装器，支持手动 unlock()、配合条件变量使用。lock_guard 不支持这些操作，只能锁到作用域结束自动解锁。
 condition.wait(lock, [this]{ return !tasks.empty() || stop; })
条件变量的谓词版本 wait：等价于 while(!pred()) wait(lock);，用来防止虚假唤醒（spurious wakeup）——即使没人 notify，操作系统偶尔也会莫名唤醒等待线程，谓词能保证唤醒后重新检查条件。
std::function<void()> task(std::move(tasks.front()));
std::move：这里是左值转右值引用，告诉编译器"这个对象我不再需要了，可以把资源移走"，避免深拷贝 std::function 内部保存的可调用对象。
tasks.front() 本身是左值（有名字、可取地址），但用 std::move 强制把它当右值处理，触发移动构造而非拷贝构造。
lock.unlock();
手动提前解锁：缩短临界区（critical section），执行 task() 时不占用锁，让其他线程能同时访问队列，这是 unique_lock 相对 lock_guard 的优势体现。
析构函数里的 { ... } 大括号
限定作用域（scope block）：让 lock 在离开这段代码时立刻自动析构解锁（RAII：Resource Acquisition Is Initialization，资源获取即初始化，锁的生命周期绑定对象生命周期），这样后面调用 notify_all() 时锁已经释放，避免死锁风险。
template<class F, class ...Args>
可变参数模板（variadic template）：...Args 表示可以接受任意数量、任意类型的参数包。
 void enqueue(F &&f, Args &&...args)
万能引用 / 转发引用（universal reference / forwarding reference）：注意这里的 && 不是右值引用，而是在模板参数推导上下文里的"万能引用"，既能绑定左值也能绑定右值。区分方法：只有形如 T&&（T 是模板参数）才是万能引用，具体类型的 &&（比如 int&&）才是真正的右值引用。
std::forward<F>(f)
完美转发（perfect forwarding）：保持参数原本的左值/右值属性传递下去。如果调用时传入的是左值，forward 后还是左值；如果是右值，转发后依然是右值，避免不必要的拷贝。这跟 std::move 不同——move 无条件转成右值，forward 是有条件的（依赖模板推导出的类型）。
std::bind(std::forward<F>(f), std::forward<Args>(args)...)
std::bind：把函数和参数打包成一个可调用对象（函数适配器），返回类型能隐式转换成 std::function<void()>。
tasks.emplace(std::move(task));
同第 5 点，避免拷贝 std::function，直接移动进队列。
 condition.notify_one();
 只唤醒一个等待中的线程（对比 notify_all() 唤醒所有线程），因为只有一个新任务，没必要全部叫醒再抢。
[i] in main 里的 lambda
按值捕获：每次循环把当前 i 的值拷贝进 lambda，避免所有 lambda 共享同一个 i 引用导致的经典 bug（如果写成 [&i]，等循环结束后所有 lambda 里的 i 可能都变成同一个最终值）。
 std::lock_guard<std::mutex> g(print_mtx);
lock_guard：最简单的 RAII 锁，构造时加锁，析构（作用域结束）时自动解锁，没有手动 unlock 的能力，比 unique_lock 更轻量。
