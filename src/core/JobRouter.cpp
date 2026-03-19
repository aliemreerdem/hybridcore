#include "JobRouter.h"
#include <iostream>

namespace core {

// --- WorkerQueue ---
WorkerQueue::WorkerQueue(const std::string& name, std::shared_ptr<ThreadSafeJobQueue> queue, std::function<void(const Job&)> executor) 
    : m_name(name), m_sharedQueue(queue), m_executor(executor), m_running(true) {
    m_thread = std::thread(&WorkerQueue::WorkerLoop, this);
}

WorkerQueue::~WorkerQueue() {
    Stop();
}

void WorkerQueue::Stop() {
    if (m_running) {
        m_running = false;
        m_sharedQueue->NotifyStop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

void WorkerQueue::WorkerLoop() {
    while (m_running) {
        auto currentJob = m_sharedQueue->PopJob(m_running);

        if (!currentJob) { continue; }

        if (currentJob) {
            // Execute the payload block via explicitly mapped runner
            if (m_executor) {
                m_executor(*currentJob); 
            }
            
            m_completedJobsCount++;

            // Notify router (Passes raw pointer for Arena releasing)
            if (onJobCompleted) {
                onJobCompleted(currentJob);
            }
        }
    }
    std::cout << "[" << m_name << "] Thread terminating." << std::endl;
}

// --- JobRouter ---
JobRouter::JobRouter() : m_jobPool(100000) {
    std::cout << "[JobRouter] Initializing Hybrid Hardware Queues (Work Stealing Pattern)" << std::endl;
    std::cout << "[JobRouter] Pre-allocating " << 100000 << " contiguous Job structs into Memory Arena." << std::endl;
    m_gpuJobQueue = std::make_shared<ThreadSafeJobQueue>();
    m_npuJobQueue = std::make_shared<ThreadSafeJobQueue>();
}

JobRouter::~JobRouter() {
    ClearJobs();
}

void JobRouter::RegisterGpuWorker(const std::string& name, std::function<void(const Job&)> executor) {
    auto q = std::make_unique<WorkerQueue>(name, m_gpuJobQueue, executor);
    q->onJobCompleted = [this](Job* job) { this->OnJobCompleted(job); };
    m_gpuWorkers.push_back(std::move(q));
}

void JobRouter::RegisterNpuWorker(const std::string& name, std::function<void(const Job&)> executor) {
    auto q = std::make_unique<WorkerQueue>(name, m_npuJobQueue, executor);
    q->onJobCompleted = [this](Job* job) { this->OnJobCompleted(job); };
    m_npuWorkers.push_back(std::move(q));
}

void JobRouter::SubmitJob(const Job& job) {
    std::lock_guard<std::mutex> lock(m_routerMutex);
    
    Job* allocJob = m_jobPool.Acquire();
    if (allocJob) {
        *allocJob = job; // Copy contents to contiguous arena block
        m_pendingJobs.push_back(allocJob);
    } else {
        std::cerr << "[JobRouter] FATAL OOM: Job Arena Pool Exhausted!" << std::endl;
    }
}

void JobRouter::OnJobCompleted(Job* job) {
    std::lock_guard<std::mutex> lock(m_routerMutex);
    m_completedJobs.insert(job->id);
    m_jobPool.Release(job); // Instant GC-free recovery
}

void JobRouter::Update() {
    std::lock_guard<std::mutex> lock(m_routerMutex);

    // Evaluate Directed Acyclic Graph (DAG) for unblocked jobs
    auto it = m_pendingJobs.begin();
    while (it != m_pendingJobs.end()) {
        auto job = *it;
        bool ready = true;

        for (const auto& dep : job->dependencies) {
            if (m_completedJobs.find(dep) == m_completedJobs.end()) {
                ready = false; 
                break;
            }
        }

        if (ready) {
            // Routing Logic (Global Queue Pushing)
            if (job->type == JobType::HEAVY_COMPUTE) {
                m_gpuJobQueue->PushJob(job);
            } else if (job->type == JobType::AI_INFERENCE) {
                m_npuJobQueue->PushJob(job);
            }
            // Remove from pending immediately after dispatch
            it = m_pendingJobs.erase(it); 
        } else {
            ++it;
        }
    }
}

size_t JobRouter::GetTotalJobCount() {
    std::lock_guard<std::mutex> lock(m_routerMutex);
    return m_pendingJobs.size() + m_gpuJobQueue->GetJobCount() + m_npuJobQueue->GetJobCount();
}

void JobRouter::ClearJobs() {
    std::lock_guard<std::mutex> lock(m_routerMutex);
    m_pendingJobs.clear();
    m_gpuJobQueue->Clear();
    m_npuJobQueue->Clear();
}

uint64_t JobRouter::GeteGpuCompleted() {
    uint64_t total = 0;
    for (auto& q : m_gpuWorkers) total += q->GetCompletedCount();
    return total;
}

uint64_t JobRouter::GetNpuCompleted() {
    uint64_t total = 0;
    for (auto& q : m_npuWorkers) total += q->GetCompletedCount();
    return total;
}

} // namespace core
