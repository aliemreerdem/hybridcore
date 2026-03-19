#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <atomic>

namespace core {

enum class JobType {
    HEAVY_COMPUTE, // Routed to eGPU (DX12 Compute)
    AI_INFERENCE   // Routed to NPU (ONNX Runtime / DirectML)
};

struct Job {
    std::string id;
    std::string name;
    JobType type;
    std::vector<std::string> dependencies;
};

class JobPool {
public:
    JobPool(size_t capacity) : m_capacity(capacity) {
        m_pool.resize(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            m_freeIndices.push(i);
        }
    }
    
    Job* Acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_freeIndices.empty()) return nullptr;
        size_t idx = m_freeIndices.front();
        m_freeIndices.pop();
        return &m_pool[idx];
    }
    
    void Release(Job* job) {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t idx = job - m_pool.data();
        if (idx < m_capacity) {
            m_freeIndices.push(idx);
        }
    }

    size_t GetAvailableCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_freeIndices.size();
    }

private:
    size_t m_capacity;
    std::vector<Job> m_pool;
    std::queue<size_t> m_freeIndices;
    mutable std::mutex m_mutex;
};

class ThreadSafeJobQueue {
public:
    void PushJob(Job* job) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.push(job);
        m_cv.notify_one();
    }

    Job* PopJob(const std::atomic<bool>& running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this, &running] { return !m_jobs.empty() || !running; });
        if (!running && m_jobs.empty()) return nullptr;
        auto job = m_jobs.front();
        m_jobs.pop();
        return job;
    }

    void NotifyStop() {
        m_cv.notify_all();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<Job*> empty;
        std::swap(m_jobs, empty);
    }

    size_t GetJobCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_jobs.size();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<Job*> m_jobs;
};

class WorkerQueue {
public:
    WorkerQueue(const std::string& name, std::shared_ptr<ThreadSafeJobQueue> queue, std::function<void(const Job&)> executor);
    ~WorkerQueue();

    void Stop();
    uint64_t GetCompletedCount() const { return m_completedJobsCount.load(); }

    // Invoked safely from worker thread upon task completion
    std::function<void(Job*)> onJobCompleted;

private:
    void WorkerLoop();

    std::string m_name;
    std::shared_ptr<ThreadSafeJobQueue> m_sharedQueue;
    std::thread m_thread;
    std::atomic<bool> m_running;
    std::atomic<uint64_t> m_completedJobsCount{0};
    
    std::function<void(const Job&)> m_executor;
};

class JobRouter {
public:
    JobRouter();
    ~JobRouter();

    void RegisterGpuWorker(const std::string& name, std::function<void(const Job&)> executor);
    void RegisterNpuWorker(const std::string& name, std::function<void(const Job&)> executor);

    void SubmitJob(const Job& job);
    void Update(); // Called per engine tick

    size_t GetTotalJobCount();
    void ClearJobs();
    
    uint64_t GeteGpuCompleted();
    uint64_t GetNpuCompleted();

private:
    void OnJobCompleted(Job* job);

    JobPool m_jobPool;

    std::shared_ptr<ThreadSafeJobQueue> m_gpuJobQueue;
    std::shared_ptr<ThreadSafeJobQueue> m_npuJobQueue;

    std::vector<std::unique_ptr<WorkerQueue>> m_gpuWorkers;
    std::vector<std::unique_ptr<WorkerQueue>> m_npuWorkers;
    
    std::vector<Job*> m_pendingJobs;
    std::unordered_set<std::string> m_completedJobs;

    std::mutex m_routerMutex;
};

} // namespace core
