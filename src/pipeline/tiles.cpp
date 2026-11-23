#include "pipeline/tiles.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace {

// D7：常驻线程池。
//
// 为什么不每帧 std::thread 一批了事：1080p 下单帧只有几毫秒，创建/销毁十个线程
// （8 MB 栈的映射、首次触碰的页错误）能占到 10% 以上，而且抖动很大。
// 每个线程数各养一个固定大小的池子，帧与帧之间用条件变量交接，测出来的才是渲染成本。
class Pool {
public:
    explicit Pool(int workers) {
        for (int i = 1; i <= workers; ++i) m_threads.emplace_back([this, i] { loop(i); });
    }

    ~Pool() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (std::thread& th : m_threads)
            if (th.joinable()) th.join();
    }

    // 主线程算 0 号，后台线程 1..n；返回时这一批活全部干完
    void run(const std::function<void(int)>& body) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_body = &body;
        m_remaining = int(m_threads.size());
        ++m_generation;
        m_cv.notify_all();

        lock.unlock();
        body(0);  // 主线程也干活，省一次线程启动
        lock.lock();

        m_doneCv.wait(lock, [&] { return m_remaining == 0; });
        m_body = nullptr;
    }

private:
    void loop(int id) {
        unsigned long long seen = 0;
        for (;;) {
            const std::function<void(int)>* body = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [&] { return m_stop || m_generation != seen; });
                if (m_stop) return;
                seen = m_generation;
                body = m_body;
            }
            if (body) (*body)(id);

            // 收工计数必须和主线程的 wait 用同一把锁：否则"store 到 0 + notify"可能
            // 正好落在主线程"谓词为假 → 进入阻塞"之间，那一次唤醒就丢了，直接死锁。
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (--m_remaining == 0) m_doneCv.notify_one();
            }
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;      // 派活
    std::condition_variable m_doneCv;  // 收工
    std::vector<std::thread> m_threads;
    const std::function<void(int)>* m_body = nullptr;
    int m_remaining = 0;  // 只有持锁时才读写
    unsigned long long m_generation = 0;
    bool m_stop = false;
};

// 每个线程数一个独立的池：线程数固定，不存在"池里有 9 个线程却只派 1 个"的错配
Pool& poolFor(int threads) {
    static std::mutex guard;
    static std::map<int, std::unique_ptr<Pool>> pools;
    std::lock_guard<std::mutex> lock(guard);
    std::unique_ptr<Pool>& slot = pools[threads];
    if (!slot) slot = std::make_unique<Pool>(threads - 1);
    return *slot;
}

}  // namespace

TileLists binTriangles(const std::vector<ScreenTri>& tris, int width, int height, int tileSize) {
    TileLists lists;
    lists.tileSize = std::max(1, tileSize);
    lists.cols = (width + lists.tileSize - 1) / lists.tileSize;
    lists.rows = (height + lists.tileSize - 1) / lists.tileSize;

    const int nTiles = lists.tileCount();
    std::vector<int> counts(size_t(nTiles) + 1, 0);

    // 每个三角形的 tile 矩形：按包围盒挑 tile，tile 内部再用半开区间裁像素，
    // 这样边界三角形不会漏、同一像素也不会被两个 tile 各算一次
    const auto forEachTile = [&](const ScreenTri& t, auto&& fn) {
        const float fminX = std::min({t.a.x, t.b.x, t.c.x});
        const float fmaxX = std::max({t.a.x, t.b.x, t.c.x});
        const float fminY = std::min({t.a.y, t.b.y, t.c.y});
        const float fmaxY = std::max({t.a.y, t.b.y, t.c.y});

        // 整片在屏幕外就不登记了
        if (fmaxX < 0.0f || fmaxY < 0.0f || fminX >= float(width) || fminY >= float(height))
            return;

        const int tx0 = std::clamp(int(std::floor(fminX)) / lists.tileSize, 0, lists.cols - 1);
        const int tx1 = std::clamp(int(std::floor(fmaxX)) / lists.tileSize, 0, lists.cols - 1);
        const int ty0 = std::clamp(int(std::floor(fminY)) / lists.tileSize, 0, lists.rows - 1);
        const int ty1 = std::clamp(int(std::floor(fmaxY)) / lists.tileSize, 0, lists.rows - 1);

        for (int ty = ty0; ty <= ty1; ++ty)
            for (int tx = tx0; tx <= tx1; ++tx) fn(ty * lists.cols + tx);
    };

    // 第一遍数数，前缀和成 offsets，第二遍填 indices —— 两次遍历换来零碎 vector 的省去
    for (const ScreenTri& t : tris)
        forEachTile(t, [&](int tile) { ++counts[size_t(tile)]; });

    lists.offsets.assign(size_t(nTiles) + 1, 0);
    for (int i = 0; i < nTiles; ++i)
        lists.offsets[size_t(i) + 1] = lists.offsets[size_t(i)] + counts[size_t(i)];

    lists.indices.resize(size_t(lists.offsets.back()));
    std::vector<int> cursor(lists.offsets.begin(), lists.offsets.end() - 1);

    for (size_t i = 0; i < tris.size(); ++i)
        forEachTile(tris[i],
                    [&](int tile) { lists.indices[size_t(cursor[size_t(tile)]++)] = int(i); });

    return lists;
}

void renderTiles(Framebuffer& fb, const std::vector<ScreenTri>& tris, const TileLists& tiles,
                 const RasterOptions& baseOpt, int threads, RasterCounters* counters) {
    const int nTiles = tiles.tileCount();
    if (nTiles <= 0) return;

    threads = std::max(1, std::min(threads, nTiles));

    // 动态任务队列：谁抢到谁做，负载不均时不会有一个线程拖尾
    std::atomic<int> nextTile{0};
    const size_t nWorkers = static_cast<size_t>(threads);
    std::vector<RasterCounters> perThread(nWorkers);

    const auto body = [&](int me) {
        RasterCounters local;
        for (;;) {
            const int t = nextTile.fetch_add(1, std::memory_order_relaxed);
            if (t >= nTiles) break;

            RasterOptions opt = baseOpt;
            opt.x0 = tiles.tileX0(t);
            opt.y0 = tiles.tileY0(t);
            opt.x1 = std::min(opt.x0 + tiles.tileSize, fb.width());
            opt.y1 = std::min(opt.y0 + tiles.tileSize, fb.height());

            for (int k = tiles.offsets[size_t(t)]; k < tiles.offsets[size_t(t) + 1]; ++k) {
                const ScreenTri& tri = tris[size_t(tiles.indices[size_t(k)])];
                drawTriangle(fb, tri.a, tri.b, tri.c, opt, *tri.shader, &local);
            }
        }
        perThread[size_t(me)] = local;  // 每个线程只写自己那一格，不存在竞争
    };

    if (threads == 1)
        body(0);  // 单线程基线：同一份代码路径，只是不进池
    else
        poolFor(threads).run(body);

    if (counters) {
        for (const RasterCounters& c : perThread) {
            counters->covered += c.covered;
            counters->shaded += c.shaded;
        }
    }
}
