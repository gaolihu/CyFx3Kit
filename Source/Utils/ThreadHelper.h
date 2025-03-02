// ThreadHelper.h
#pragma once

#include <thread>
#include <future>
#include <chrono>
#include <functional>
#include <QString>
#include "Logger.h"

class ThreadHelper {
public:
    // 带有超时的线程回收函数
    static bool joinThreadWithTimeout(std::thread& thread,
        int timeoutMs = 2000,
        const QString& threadName = "thread") {

        if (!thread.joinable()) {
            return true;
        }

        if (thread.get_id() == std::this_thread::get_id()) {
            LOG_WARN(QString("Cannot join %1 from itself").arg(threadName));
            return false;
        }

        // 使用C++11 future机制实现超时
        auto futureObj = std::async(std::launch::async, [&thread]() {
            try {
                thread.join();
                return true;
            }
            catch (...) {
                return false;
            }
            });

        // 等待指定的超时时间
        std::future_status status = futureObj.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status == std::future_status::timeout) {
            LOG_WARN(QString("%1 join timed out after %2ms, detaching thread")
                .arg(threadName).arg(timeoutMs));

            // 线程超时后，我们没有好的方法终止它，只能分离
            thread.detach();
            return false;
        }

        // 获取join的结果
        try {
            return futureObj.get();
        }
        catch (const std::exception& e) {
            LOG_ERROR(QString("Exception in thread join: %1").arg(e.what()));
            return false;
        }
    }

    // 添加一个方法处理Qt线程
    static bool joinQtThreadWithTimeout(QThread* thread,
        int timeoutMs = 2000,
        const QString& threadName = "Qt thread") {
        if (!thread || !thread->isRunning()) {
            return true;
        }

        LOG_INFO(QString("Waiting for %1 to finish...").arg(threadName));
        return thread->wait(timeoutMs);
    }
};