// Copyright (c) 2026, The ninacatcoin Project
//
// NINA Network Optimizer — Implementation
//
// Pure observation and recommendation engine.
// Never forces disconnections or modifies protocol rules.

#include "nina_network_optimizer.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <set>

namespace ninacatcoin_ai {

// =========================================================================
// CONSTRUCTION
// =========================================================================

NINANetworkOptimizer::NINANetworkOptimizer()
    : m_smoothed_upload_kbps(0.0)
    , m_smoothed_download_kbps(0.0)
    , m_last_health{}
{
    MINFO("NINA Network Optimizer initialized");
}

// =========================================================================
// HELPER: current timestamp
// =========================================================================
static uint64_t now_epoch() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

// =========================================================================
// PEER INTERACTION RECORDING
// =========================================================================

void NINANetworkOptimizer::record_peer_interaction(
    const std::string& peer_id,
    double latency_ms,
    bool data_valid,
    uint64_t bytes_received,
    uint64_t peer_height,
    const std::string& peer_version,
    bool is_pruned)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_peers.find(peer_id);
    if (it == m_peers.end()) {
        PeerData pd;
        pd.peer_id = peer_id;
        pd.blocks_received = 0;
        pd.invalid_blocks = 0;
        pd.total_bytes_received = 0;
        pd.peer_height = 0;
        pd.is_pruned = false;
        pd.connected_since = now_epoch();
        pd.last_activity = now_epoch();
        pd.active = true;
        m_peers[peer_id] = pd;
        it = m_peers.find(peer_id);
    }

    PeerData& pd = it->second;
    pd.active = true;
    pd.last_activity = now_epoch();
    pd.peer_height = peer_height;
    pd.version = peer_version;
    pd.is_pruned = is_pruned;
    pd.total_bytes_received += bytes_received;

    // Latency tracking
    if (latency_ms > 0.0) {
        pd.latency_samples.push_back(latency_ms);
        if (pd.latency_samples.size() > PeerData::MAX_LATENCY_SAMPLES) {
            pd.latency_samples.pop_front();
        }
    }

    // Block validity
    if (bytes_received > 0) {
        pd.blocks_received++;
        if (!data_valid) {
            pd.invalid_blocks++;
        }
    }
}

// =========================================================================
// PEER QUALITY CALCULATION
// =========================================================================

double NINANetworkOptimizer::calculate_peer_quality(
    const PeerData& data,
    uint64_t our_height)
{
    // Components:
    //   1. Latency score      (0-30 pts)  — lower = better
    //   2. Reliability score   (0-30 pts)  — fewer invalid blocks = better
    //   3. Sync score          (0-20 pts)  — closer to our height = better
    //   4. Uptime score        (0-10 pts)  — longer connection = better
    //   5. Full node bonus     (0-10 pts)  — non-pruned nodes preferred

    double score = 0.0;

    // --- 1. Latency (30 pts) ---
    if (!data.latency_samples.empty()) {
        double avg_latency = 0.0;
        for (double l : data.latency_samples) avg_latency += l;
        avg_latency /= static_cast<double>(data.latency_samples.size());

        // 0-100ms: 30 pts, 100-500ms: 20-30 pts, 500-2000ms: 5-20 pts, >2000ms: 0-5 pts
        if (avg_latency <= 100.0) {
            score += 30.0;
        } else if (avg_latency <= 500.0) {
            score += 20.0 + 10.0 * (1.0 - (avg_latency - 100.0) / 400.0);
        } else if (avg_latency <= 2000.0) {
            score += 5.0 + 15.0 * (1.0 - (avg_latency - 500.0) / 1500.0);
        } else {
            score += std::max(0.0, 5.0 * (1.0 - (avg_latency - 2000.0) / 8000.0));
        }
    } else {
        score += 15.0; // No data yet — neutral
    }

    // --- 2. Reliability (30 pts) ---
    if (data.blocks_received > 0) {
        double validity_ratio = 1.0 -
            static_cast<double>(data.invalid_blocks) /
            static_cast<double>(data.blocks_received);
        score += 30.0 * validity_ratio;
    } else {
        score += 15.0; // No data yet — neutral
    }

    // --- 3. Sync proximity (20 pts) ---
    if (our_height > 0 && data.peer_height > 0) {
        if (data.peer_height >= our_height) {
            score += 20.0; // Peer is ahead or at same height — perfect
        } else {
            uint64_t behind = our_height - data.peer_height;
            if (behind <= 2) {
                score += 18.0; // 1-2 blocks behind is fine
            } else if (behind <= 10) {
                score += 10.0 + 8.0 * (1.0 - static_cast<double>(behind - 2) / 8.0);
            } else if (behind <= 100) {
                score += 5.0 * (1.0 - static_cast<double>(behind - 10) / 90.0);
            }
            // >100 blocks behind = 0 pts
        }
    } else {
        score += 10.0; // Unknown — neutral
    }

    // --- 4. Uptime (10 pts) ---
    {
        uint64_t uptime = now_epoch() - data.connected_since;
        // Longer connections are more valuable (up to 1 hour = max pts)
        double hours = static_cast<double>(uptime) / 3600.0;
        score += std::min(10.0, hours * 10.0); // 1h+ = 10 pts
    }

    // --- 5. Full node bonus (10 pts) ---
    if (!data.is_pruned) {
        score += 10.0;
    }

    return std::min(100.0, std::max(0.0, score));
}

// =========================================================================
// PEER SCORE RETRIEVAL
// =========================================================================

NINANetworkOptimizer::PeerScore NINANetworkOptimizer::get_peer_score(
    const std::string& peer_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    PeerScore ps{};
    ps.peer_id = peer_id;

    auto it = m_peers.find(peer_id);
    if (it == m_peers.end()) {
        ps.quality_score = 0.0;
        ps.recommendation = "unknown";
        return ps;
    }

    const PeerData& pd = it->second;

    // Compute average latency
    double avg_lat = 0.0;
    if (!pd.latency_samples.empty()) {
        for (double l : pd.latency_samples) avg_lat += l;
        avg_lat /= static_cast<double>(pd.latency_samples.size());
    }

    ps.latency_ms = avg_lat;
    ps.blocks_received = pd.blocks_received;
    ps.invalid_blocks = pd.invalid_blocks;
    ps.peer_height = pd.peer_height;
    ps.is_full_node = !pd.is_pruned;
    ps.is_synced = true; // We don't know our height here — caller should check
    ps.version = pd.version;
    ps.connected_since = pd.connected_since;
    ps.last_activity = pd.last_activity;

    ps.reliability = (pd.blocks_received > 0)
        ? 1.0 - static_cast<double>(pd.invalid_blocks) / static_cast<double>(pd.blocks_received)
        : 1.0;

    ps.quality_score = calculate_peer_quality(pd, 0); // 0 = don't penalize for sync

    // Recommendation
    if (ps.quality_score >= 60.0) {
        ps.recommendation = "keep";
    } else if (ps.quality_score >= 30.0) {
        ps.recommendation = "deprioritize";
    } else {
        ps.recommendation = "disconnect_candidate";
    }

    return ps;
}

std::vector<NINANetworkOptimizer::PeerScore> NINANetworkOptimizer::get_all_peer_scores()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<PeerScore> result;
    result.reserve(m_peers.size());

    for (const auto& kv : m_peers) {
        if (!kv.second.active) continue;

        PeerScore ps{};
        ps.peer_id = kv.first;
        const PeerData& pd = kv.second;

        double avg_lat = 0.0;
        if (!pd.latency_samples.empty()) {
            for (double l : pd.latency_samples) avg_lat += l;
            avg_lat /= static_cast<double>(pd.latency_samples.size());
        }

        ps.latency_ms = avg_lat;
        ps.blocks_received = pd.blocks_received;
        ps.invalid_blocks = pd.invalid_blocks;
        ps.peer_height = pd.peer_height;
        ps.is_full_node = !pd.is_pruned;
        ps.is_synced = false; // Caller should set
        ps.version = pd.version;
        ps.connected_since = pd.connected_since;
        ps.last_activity = pd.last_activity;
        ps.reliability = (pd.blocks_received > 0)
            ? 1.0 - static_cast<double>(pd.invalid_blocks) / static_cast<double>(pd.blocks_received)
            : 1.0;
        ps.quality_score = calculate_peer_quality(pd, 0);

        if (ps.quality_score >= 60.0) ps.recommendation = "keep";
        else if (ps.quality_score >= 30.0) ps.recommendation = "deprioritize";
        else ps.recommendation = "disconnect_candidate";

        result.push_back(ps);
    }

    std::sort(result.begin(), result.end(), [](const PeerScore& a, const PeerScore& b) {
        return a.quality_score > b.quality_score;
    });

    return result;
}

NINANetworkOptimizer::PeerScore NINANetworkOptimizer::get_worst_peer()
{
    auto all = get_all_peer_scores();
    if (all.empty()) {
        PeerScore empty{};
        empty.recommendation = "none";
        return empty;
    }
    return all.back(); // Sorted best-first, worst is last
}

std::vector<NINANetworkOptimizer::PeerScore> NINANetworkOptimizer::get_best_sync_peers(
    size_t count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // For sync, we want: full nodes, synced, low latency
    struct SyncCandidate {
        std::string peer_id;
        double sync_score;
    };

    std::vector<SyncCandidate> candidates;

    for (const auto& kv : m_peers) {
        if (!kv.second.active) continue;

        const PeerData& pd = kv.second;
        double s = 0.0;

        // Full node bonus: huge advantage for sync
        if (!pd.is_pruned) s += 50.0;

        // Low latency
        if (!pd.latency_samples.empty()) {
            double avg = 0.0;
            for (double l : pd.latency_samples) avg += l;
            avg /= static_cast<double>(pd.latency_samples.size());
            s += std::max(0.0, 30.0 * (1.0 - avg / 1000.0));
        } else {
            s += 15.0;
        }

        // High reliability
        if (pd.blocks_received > 5) {
            double rel = 1.0 - static_cast<double>(pd.invalid_blocks) / static_cast<double>(pd.blocks_received);
            s += 20.0 * rel;
        } else {
            s += 10.0;
        }

        candidates.push_back({kv.first, s});
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const SyncCandidate& a, const SyncCandidate& b) {
            return a.sync_score > b.sync_score;
        });

    std::vector<PeerScore> result;
    for (size_t i = 0; i < std::min(count, candidates.size()); ++i) {
        // Get full score (without lock — we already hold it directly)
        auto it = m_peers.find(candidates[i].peer_id);
        if (it == m_peers.end()) continue;

        PeerScore ps{};
        ps.peer_id = candidates[i].peer_id;
        ps.quality_score = candidates[i].sync_score;
        ps.is_full_node = !it->second.is_pruned;
        ps.peer_height = it->second.peer_height;
        ps.recommendation = "sync_preferred";
        result.push_back(ps);
    }

    return result;
}

void NINANetworkOptimizer::notify_peer_disconnected(const std::string& peer_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_peers.find(peer_id);
    if (it != m_peers.end()) {
        it->second.active = false;
        MDEBUG("Peer disconnected: " << peer_id);
    }
}

// =========================================================================
// BANDWIDTH MEASUREMENT
// =========================================================================

void NINANetworkOptimizer::record_bandwidth_sample(
    uint64_t bytes_sent,
    uint64_t bytes_received,
    double period_seconds)
{
    if (period_seconds <= 0.0) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    double up_kbps = static_cast<double>(bytes_sent) / period_seconds / 1024.0;
    double down_kbps = static_cast<double>(bytes_received) / period_seconds / 1024.0;

    BandwidthSample sample;
    sample.upload_kbps = up_kbps;
    sample.download_kbps = down_kbps;
    sample.timestamp = now_epoch();

    m_bandwidth_history.push_back(sample);
    if (m_bandwidth_history.size() > MAX_BANDWIDTH_SAMPLES) {
        m_bandwidth_history.pop_front();
    }

    // Exponential moving average
    constexpr double alpha = 0.1;
    if (m_smoothed_upload_kbps <= 0.0) {
        m_smoothed_upload_kbps = up_kbps;
        m_smoothed_download_kbps = down_kbps;
    } else {
        m_smoothed_upload_kbps = alpha * up_kbps + (1.0 - alpha) * m_smoothed_upload_kbps;
        m_smoothed_download_kbps = alpha * down_kbps + (1.0 - alpha) * m_smoothed_download_kbps;
    }
}

std::string NINANetworkOptimizer::classify_bandwidth_tier(
    double upload_kbps,
    double download_kbps)
{
    double effective = std::min(upload_kbps, download_kbps);
    if (effective >= 10000.0)  return "datacenter";  // >= 10 MB/s
    if (effective >= 1000.0)   return "high";         // >= 1 MB/s
    if (effective >= 200.0)    return "medium";        // >= 200 KB/s
    return "low";                                       // < 200 KB/s
}

void NINANetworkOptimizer::calculate_optimal_peers(
    const std::string& tier,
    double upload_kbps,
    double /* download_kbps */,
    uint32_t& out_peers,
    uint32_t& in_peers)
{
    //
    // Peer count recommendations:
    //
    //   Low bandwidth:      out=4,  in=4    (preserve bandwidth)
    //   Medium bandwidth:   out=8,  in=8    (default-like)
    //   High bandwidth:     out=12, in=16   (help the network)
    //   Datacenter:         out=16, in=32   (be a super-node)
    //
    // Upload is the bottleneck for serving peers (in_peers).
    // Download is the bottleneck for syncing (out_peers).
    //

    if (tier == "datacenter") {
        out_peers = 16;
        in_peers = 32;
    } else if (tier == "high") {
        out_peers = 12;
        in_peers = 16;
    } else if (tier == "medium") {
        out_peers = 8;
        in_peers = 8;
    } else {
        out_peers = 4;
        in_peers = 4;
    }

    // Fine-tune: if upload is very constrained relative to tier
    if (upload_kbps > 0 && upload_kbps < 100.0) {
        in_peers = std::min(in_peers, static_cast<uint32_t>(4));
    }
}

NINANetworkOptimizer::NetworkConfig NINANetworkOptimizer::get_recommended_config()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    NetworkConfig cfg{};
    cfg.measured_bandwidth_up = m_smoothed_upload_kbps;
    cfg.measured_bandwidth_down = m_smoothed_download_kbps;

    if (m_bandwidth_history.size() < 3) {
        // Not enough data — return defaults
        cfg.recommended_out_peers = 8;
        cfg.recommended_in_peers = 8;
        cfg.recommended_rate_up = 2048;   // 2 MB/s
        cfg.recommended_rate_down = 8192; // 8 MB/s
        cfg.bandwidth_tier = "unknown";
        cfg.explanation = "Insufficient bandwidth data — using defaults. "
                          "Need at least 3 measurement samples.";
        return cfg;
    }

    cfg.bandwidth_tier = classify_bandwidth_tier(
        m_smoothed_upload_kbps, m_smoothed_download_kbps
    );

    calculate_optimal_peers(
        cfg.bandwidth_tier,
        m_smoothed_upload_kbps,
        m_smoothed_download_kbps,
        cfg.recommended_out_peers,
        cfg.recommended_in_peers
    );

    // Rate limits: allow 70% of measured bandwidth to be used by daemon
    cfg.recommended_rate_up = static_cast<uint32_t>(m_smoothed_upload_kbps * 0.70);
    cfg.recommended_rate_down = static_cast<uint32_t>(m_smoothed_download_kbps * 0.70);

    // Ensure minimum
    cfg.recommended_rate_up = std::max(cfg.recommended_rate_up, static_cast<uint32_t>(64));
    cfg.recommended_rate_down = std::max(cfg.recommended_rate_down, static_cast<uint32_t>(128));

    std::ostringstream oss;
    oss << "Bandwidth tier: " << cfg.bandwidth_tier
        << " (up: " << std::fixed << std::setprecision(1)
        << m_smoothed_upload_kbps << " KB/s, down: "
        << m_smoothed_download_kbps << " KB/s). "
        << "Recommended: out-peers=" << cfg.recommended_out_peers
        << ", in-peers=" << cfg.recommended_in_peers;
    cfg.explanation = oss.str();

    return cfg;
}

// =========================================================================
// NETWORK HEALTH ASSESSMENT
// =========================================================================

NINANetworkOptimizer::NetworkHealth NINANetworkOptimizer::assess_network_health(
    uint64_t our_height)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    NetworkHealth health{};

    uint32_t active = 0;
    uint32_t synced = 0;
    uint32_t full_nodes = 0;
    double total_latency = 0.0;
    double total_quality = 0.0;
    uint32_t latency_count = 0;

    // Collect IP prefix diversity (first 3 octets of peer_id if IP-based)
    std::set<std::string> ip_prefixes;

    for (const auto& kv : m_peers) {
        if (!kv.second.active) continue;
        active++;

        const PeerData& pd = kv.second;

        // Sync check
        if (our_height > 0 && pd.peer_height > 0) {
            if (pd.peer_height + 5 >= our_height) {
                synced++;
            }
        }

        // Full node
        if (!pd.is_pruned) {
            full_nodes++;
        }

        // Latency
        if (!pd.latency_samples.empty()) {
            double avg = 0.0;
            for (double l : pd.latency_samples) avg += l;
            avg /= static_cast<double>(pd.latency_samples.size());
            total_latency += avg;
            latency_count++;
        }

        // Quality
        total_quality += calculate_peer_quality(pd, our_height);

        // Diversity: use peer_id prefix as crude IP prefix
        if (kv.first.size() >= 8) {
            ip_prefixes.insert(kv.first.substr(0, 8));
        }
    }

    health.total_peers = active;
    health.synced_peers = synced;
    health.full_node_peers = full_nodes;
    health.average_latency_ms = (latency_count > 0) ? total_latency / latency_count : 0.0;
    health.average_peer_quality = (active > 0) ? total_quality / active : 0.0;

    // Diversity: ratio of unique prefixes to total peers
    health.network_diversity = (active > 1)
        ? static_cast<double>(ip_prefixes.size()) / static_cast<double>(active)
        : 0.0;

    // Calculate health score (0-100)
    double h = 0.0;

    // Peer count (25 pts): ideal is 8+
    if (active >= 8) h += 25.0;
    else if (active >= 4) h += 15.0;
    else if (active >= 2) h += 8.0;
    else if (active >= 1) h += 3.0;

    // Synced peers ratio (25 pts)
    if (active > 0) {
        h += 25.0 * static_cast<double>(synced) / static_cast<double>(active);
    }

    // Average quality (25 pts)
    h += 0.25 * health.average_peer_quality;

    // Diversity (15 pts)
    h += 15.0 * health.network_diversity;

    // Full node ratio (10 pts)
    if (active > 0) {
        h += 10.0 * static_cast<double>(full_nodes) / static_cast<double>(active);
    }

    health.health_score = std::min(100.0, std::max(0.0, h));

    // Status classification
    if (health.health_score >= 80.0) {
        health.status = "EXCELLENT";
        health.recommended_action = 0;
        health.action_detail = "Network connectivity is optimal.";
    } else if (health.health_score >= 60.0) {
        health.status = "GOOD";
        health.recommended_action = 0;
        health.action_detail = "Network is healthy.";
    } else if (health.health_score >= 35.0) {
        health.status = "DEGRADED";
        if (active < 4) {
            health.recommended_action = 1;
            health.action_detail = "Low peer count. Consider adding seed nodes or checking firewall.";
        } else {
            health.recommended_action = 2;
            health.action_detail = "Peer quality is low. Consider cycling connections.";
        }
    } else {
        health.status = "CRITICAL";
        health.recommended_action = 1;
        health.action_detail = "Very few or very poor peers. Network reliability at risk.";
    }

    m_last_health = health;
    return health;
}

// =========================================================================
// REPORT GENERATION
// =========================================================================

std::string NINANetworkOptimizer::generate_network_report(uint64_t our_height)
{
    auto health = assess_network_health(our_height);

    // Get peer list (need separate lock scope — assess already released)
    auto peers = get_all_peer_scores();

    std::ostringstream rpt;
    rpt << "===== NINA Network Optimizer Report =====\n\n";

    // Health summary
    rpt << "NETWORK STATUS: " << health.status
        << " (score: " << std::fixed << std::setprecision(1)
        << health.health_score << "/100)\n\n";

    rpt << "Peers: " << health.total_peers
        << " (synced: " << health.synced_peers
        << ", full nodes: " << health.full_node_peers << ")\n";
    rpt << "Average latency: " << std::setprecision(0) << health.average_latency_ms << " ms\n";
    rpt << "Average quality: " << std::setprecision(1) << health.average_peer_quality << "/100\n";
    rpt << "Diversity index: " << std::setprecision(2) << health.network_diversity << "\n\n";

    // Bandwidth
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        rpt << "Bandwidth (smoothed): up=" << std::setprecision(1)
            << m_smoothed_upload_kbps << " KB/s, down="
            << m_smoothed_download_kbps << " KB/s\n\n";
    }

    // Configuration recommendation
    auto cfg = get_recommended_config();
    rpt << "RECOMMENDED CONFIG:\n"
        << "  out-peers: " << cfg.recommended_out_peers << "\n"
        << "  in-peers:  " << cfg.recommended_in_peers << "\n"
        << "  rate-up:   " << cfg.recommended_rate_up << " KB/s\n"
        << "  rate-down: " << cfg.recommended_rate_down << " KB/s\n"
        << "  tier:      " << cfg.bandwidth_tier << "\n\n";

    // Peer table (top 10)
    rpt << "TOP PEERS (by quality):\n";
    rpt << std::setw(20) << std::left << "PEER"
        << std::setw(8) << "SCORE"
        << std::setw(10) << "LATENCY"
        << std::setw(8) << "BLOCKS"
        << std::setw(10) << "RELIABLE"
        << std::setw(6) << "FULL"
        << "STATUS\n";
    rpt << std::string(72, '-') << "\n";

    size_t count = 0;
    for (const auto& ps : peers) {
        if (count >= 10) break;
        rpt << std::setw(20) << std::left << ps.peer_id.substr(0, 18)
            << std::setw(8) << std::setprecision(1) << std::fixed << ps.quality_score
            << std::setw(10) << std::setprecision(0) << ps.latency_ms
            << std::setw(8) << ps.blocks_received
            << std::setw(10) << std::setprecision(2) << ps.reliability
            << std::setw(6) << (ps.is_full_node ? "YES" : "no")
            << ps.recommendation << "\n";
        count++;
    }

    rpt << "\n";

    // Action
    if (health.recommended_action != 0) {
        rpt << "RECOMMENDED ACTION: " << health.action_detail << "\n";
    }

    rpt << "=========================================\n";

    return rpt.str();
}

// =========================================================================
// PERIODIC MAINTENANCE
// =========================================================================

void NINANetworkOptimizer::periodic_maintenance()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t now = now_epoch();
    constexpr uint64_t STALE_THRESHOLD = 3600; // 1 hour without activity

    // Remove stale inactive peers
    for (auto it = m_peers.begin(); it != m_peers.end(); ) {
        if (!it->second.active && (now - it->second.last_activity) > STALE_THRESHOLD) {
            MDEBUG("Removing stale peer data: " << it->first);
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }

    // Trim old bandwidth samples (keep last 2 hours)
    constexpr uint64_t MAX_BW_AGE = 7200;
    while (!m_bandwidth_history.empty() &&
           (now - m_bandwidth_history.front().timestamp) > MAX_BW_AGE) {
        m_bandwidth_history.pop_front();
    }
}

} // namespace ninacatcoin_ai
