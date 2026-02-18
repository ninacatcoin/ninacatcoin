#pragma once

/**
 * @file ia_hashrate_recovery_examples.hpp
 * @brief Practical examples of using the IA Hashrate Recovery Monitor
 * 
 * This file contains 8 complete code examples showing how to use
 * the IA hashrate recovery monitoring system in practice.
 */

// =====================================================================
// EXAMPLE 1: Initialize IA on Daemon Startup
// =====================================================================
/*
 * Called once when the daemon/node starts up.
 * Sets up the IA knowledge base for the running session.
 */

#include "ai_hashrate_recovery_monitor.hpp"
#include "common/command_line.h"

void example1_initialize_ia_on_startup()
{
  // In main() or Blockchain::init():
  
  MGINFO("Starting ninacatcoin daemon...");
  
  // Initialize the IA system
  ia_initialize_hashrate_learning();
  
  MGINFO("IA Hashrate Recovery Monitor is now active");
  MGINFO("Monitoring network difficulty and recovery mechanisms");
}

// =====================================================================
// EXAMPLE 2: Learn Every New Block
// =====================================================================
/*
 * This is called inside blockchain.cpp whenever a new block
 * is validated and added to the main chain.
 */

void example2_learn_new_block()
{
  // Inside Blockchain::add_new_block(), after validation
  
  // Assume these are available from current block:
  uint64_t block_height = 4800;                    // Height of new block
  uint64_t block_solve_time = 125;                 // Time since previous block
  uint64_t new_difficulty = 85000;                 // Difficulty for this block
  bool eda_triggered = false;                      // Was EDA mechanism used?
  
  // The IA learns about this new block
  ia_learns_difficulty_state(
      block_height,
      new_difficulty,
      block_solve_time,
      eda_triggered
  );
  
  MGINFO("Block #" << block_height << " learned by IA: "
         << "diff=" << new_difficulty << ", "
         << "solve_time=" << block_solve_time << "s");
}

// =====================================================================
// EXAMPLE 3: Detect If Network is in Recovery
// =====================================================================
/*
 * Called periodically (every 10-20 blocks) to check if the network
 * is currently recovering from a hashrate drop.
 * 
 * Returns: true if recovery is happening, false otherwise
 */

void example3_detect_recovery()
{
  uint64_t recovery_blocks_eta = 0;
  double recovery_speed = 0.0;
  
  bool is_recovering = ia_detect_recovery_in_progress(
      recovery_blocks_eta,
      recovery_speed
  );
  
  if (is_recovering) {
    MGWARN("ALERT: Network is recovering from hashrate drop!");
    MGWARN("Estimated blocks until stability: " << recovery_blocks_eta);
    MGWARN("Recovery speed: " << (recovery_speed * 100.0) << "% per block");
  } else {
    MGINFO("Network state: STABLE (no recovery in progress)");
  }
}

// =====================================================================
// EXAMPLE 4: Log EDA (Emergency Difficulty Adjustment) Events
// =====================================================================
/*
 * When block solve time exceeds 720 seconds (6x target),
 * the EDA mechanism activates. We log this for the IA to learn.
 */

void example4_log_eda_event()
{
  // This block took unusually long time
  uint64_t block_height = 4726;
  uint64_t solve_time = 850;           // > 720 second threshold!
  uint64_t base_difficulty = 9150000;  // What LWMA calculated
  
  // Alert the IA about EDA activation
  ia_learn_eda_event(block_height, solve_time, base_difficulty);
  
  // Log will show:
  // "IA: EDA Event #1 at height 4726: Block took 7.1x target time..."
}

// =====================================================================
// EXAMPLE 5: Analyze LWMA Window Health
// =====================================================================
/*
 * Call this every 60 blocks to analyze the health of the LWMA
 * (Linear Weighted Moving Average) window.
 * 
 * Returns: Detailed analysis of the last 60 blocks
 */

void example5_analyze_lwma_window()
{
  uint64_t current_height = 4850;
  
  // Get detailed analysis of LWMA window
  LWMAWindowState window = ia_analyze_lwma_window(current_height);
  
  MGINFO("LWMA Window Analysis:");
  MGINFO("  Block range: " << window.window_start_height 
         << " to " << window.window_end_height);
  MGINFO("  Average block time: " << window.average_solve_time << "s");
  MGINFO("  Weighted average: " << window.weighted_solve_time << "s");
  MGINFO("  Window status: " << window.window_status);
  
  /*
   * Possible statuses:
   * - "FILLING": Less than 60 blocks collected
   * - "ACTIVE": Normal operation (< 1 minute adjustment)
   * - "ADJUSTING": Large swings in block times
   */
  
  if (window.window_status == "ADJUSTING") {
    MGWARN("Network is adjusting difficulty");
  }
}

// =====================================================================
// EXAMPLE 6: Detect Hashrate Anomalies
// =====================================================================
/*
 * Call this frequently to detect sudden, unusual changes in hashrate.
 * Helps identify attacks, miner departures, or gains.
 * 
 * Returns: true if anomaly detected, false if normal
 */

void example6_detect_anomaly()
{
  uint64_t current_difficulty = 120000;
  double change_percent = 0.0;
  
  bool anomaly = ia_detect_hashrate_anomaly(current_difficulty, change_percent);
  
  if (anomaly) {
    MGWARN("NETWORK ANOMALY DETECTED!");
    MGWARN("Difficulty change: " << std::fixed << std::setprecision(1) 
           << change_percent << "%");
    
    if (change_percent < -50.0) {
      MGWARN("Massive hashrate LOSS detected. EDA likely to activate.");
    } else if (change_percent > 50.0) {
      MGWARN("Massive hashrate GAIN detected. Difficulty will rise.");
    }
  }
}

// =====================================================================
// EXAMPLE 7: Get Recovery Recommendations
// =====================================================================
/*
 * Ask the IA for recommendations about network health and recovery.
 * This analyzes the current situation and suggests actions.
 * 
 * Returns: String with detailed recommendations
 */

void example7_get_recommendations()
{
  std::string recommendations = ia_recommend_hashrate_recovery();
  
  MGINFO("Network Recovery Recommendations:");
  MGINFO(recommendations);
  
  /*
   * Example outputs:
   * - "RECOVERY IN PROGRESS: Estimated 15 blocks to stability..."
   * - "NETWORK STABLE: No significant difficulty changes..."
   * - "ANOMALY DETECTED: 75% change"
   * - "RECOMMENDATION: Massive hashrate loss. EDA should activate..."
   */
}

// =====================================================================
// EXAMPLE 8: Complete Monitoring Loop (Full Integration)
// =====================================================================
/*
 * This shows a complete example of how all functions work together
 * in the daemon's block processing loop.
 * 
 * This would be integrated into blockchain.cpp
 */

void example8_complete_monitoring_loop()
{
  // Simulating 3 new blocks being processed
  
  struct NewBlock {
    uint64_t height;
    uint64_t solve_time;
    uint64_t difficulty;
  };
  
  std::vector<NewBlock> incoming_blocks = {
    {4850, 125, 90000},    // Normal block
    {4851, 450, 85000},    // EDA triggered (450 > 120)
    {4852, 200, 70000},    // Recovery block
  };
  
  // Initialize on startup (done once)
  ia_initialize_hashrate_learning();
  
  for (const auto& block : incoming_blocks) {
    // 1. Learn the block
    bool eda_triggered = (block.solve_time > 720);
    ia_learns_difficulty_state(
        block.height,
        block.difficulty,
        block.solve_time,
        eda_triggered
    );
    
    MGINFO("Block #" << block.height << " learned");
    
    // 2. Analytics every 60 blocks
    if (block.height % 60 == 0) {
      auto window = ia_analyze_lwma_window(block.height);
      MGINFO("LWMA status: " << window.window_status);
    }
    
    // 3. Recovery check every 10 blocks
    if (block.height % 10 == 0) {
      uint64_t recover_blocks = 0;
      double speed = 0.0;
      if (ia_detect_recovery_in_progress(recover_blocks, speed)) {
        MGINFO("Recovering... ETA: " << recover_blocks << " blocks");
      }
    }
    
    // 4. Anomaly detection
    double anomaly = 0.0;
    if (ia_detect_hashrate_anomaly(block.difficulty, anomaly)) {
      MGWARN("Anomaly: " << anomaly << "%");
    }
    
    // 5. Detailed log every 100 blocks
    if (block.height % 100 == 0) {
      ia_log_hashrate_status();
    }
  }
  
  // 6. Get recommendations
  std::string rec = ia_recommend_hashrate_recovery();
  MGINFO("IA Recommendations:\n" << rec);
}

// =====================================================================
// BONUS: Estimate Network Hashrate from Difficulty
// =====================================================================
/*
 * Convert current difficulty into estimated network hashrate.
 * Useful for monitoring and statistics.
 */

void bonus_estimate_hashrate()
{
  uint64_t current_difficulty = 100000;
  
  double hashrate = ia_estimate_network_hashrate(current_difficulty);
  
  MGINFO("Estimated network hashrate: " 
         << std::fixed << std::setprecision(2) 
         << hashrate << " KH/s");
}

// =====================================================================
// BONUS: Predict Next Difficulty
// =====================================================================
/*
 * The IA predicts what the next difficulty will be based on
 * recent trend analysis.
 */

void bonus_predict_next_difficulty()
{
  uint64_t base_difficulty = 100000;
  
  uint64_t predicted = ia_predict_next_difficulty(base_difficulty);
  
  MGINFO("Current difficulty: " << base_difficulty);
  MGINFO("Predicted next difficulty: " << predicted);
  
  double change = (static_cast<double>(predicted) / base_difficulty - 1.0) * 100.0;
  MGINFO("Expected change: " << std::fixed << std::setprecision(1) 
         << change << "%");
}

// =====================================================================
// TESTING: Reset IA for Fresh Start
// =====================================================================
/*
 * In test environments, reset the IA to start monitoring from scratch
 */

void testing_reset_ia()
{
  MGINFO("Resetting IA for clean test...");
  ia_reset_hashrate_learning();
  ia_initialize_hashrate_learning();
  MGINFO("IA reset complete");
}

// =====================================================================
// SUMMARY OF USAGE
// =====================================================================
/*
 * STARTUP (once):
 *   - ia_initialize_hashrate_learning()
 * 
 * PER BLOCK:
 *   - ia_learns_difficulty_state(...)
 * 
 * PERIODIC (every 10 blocks):
 *   - ia_detect_recovery_in_progress()
 *   - ia_detect_hashrate_anomaly()
 * 
 * PERIODIC (every 60 blocks):
 *   - ia_analyze_lwma_window()
 * 
 * PERIODIC (every 100 blocks):
 *   - ia_log_hashrate_status()
 *   - ia_recommend_hashrate_recovery()
 * 
 * ON DEMAND:
 *   - ia_predict_next_difficulty()
 *   - ia_estimate_network_hashrate()
 *   - ia_get_hashrate_knowledge()
 * 
 * WHEN EDA TRIGGERS:
 *   - ia_learn_eda_event()
 * 
 * TESTING:
 *   - ia_reset_hashrate_learning()
 */

#endif // IA_HASHRATE_RECOVERY_EXAMPLES_HPP
