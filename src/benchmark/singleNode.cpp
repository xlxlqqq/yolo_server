#include <chrono>
#include <atomic>
#include <thread>
#include <vector>


struct Stat {
    std::atomic<uint64_t> ok{0};
    std::atomic<uint64_t> fail{0};
};

void worker(int tid, int seconds, Stat& stat) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    while (std::chrono::steady_clock::now() < end) {
        std::string msg = makeStoreMsg(tid);

        if (sendAndRecv(msg)) {
            stat.ok++;
        } else {
            stat.fail++;
        }
    }
}



int main() {
    int threads = 8;
    int duration = 10;

    Stat stat;
    std::vector<std::thread> workers;

    for (int i = 0; i < threads; ++i) {
        workers.emplace_back(worker, i, duration, std::ref(stat));
    }

    for (auto& t : workers) t.join();

    double kps = stat.ok / duration;

    std::cout << "Total OK: " << stat.ok.load() << std::endl;
    std::cout << "Total Fail: " << stat.fail.load() << std::endl;
    std::cout << "Throughput: " << kps << " KPS" << std::endl;

    return 0;
}
