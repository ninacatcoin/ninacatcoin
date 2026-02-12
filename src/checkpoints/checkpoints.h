// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once
#include <map>
#include <memory>
#include <thread>
#include <chrono>
#include "misc_log_ex.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_basic/difficulty.h"
#include "serialization/keyvalue_serialization.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

// Phase 2: P2P Consensus & Reputation Tracking
#include "../../tools/security_query_tool.hpp"
#include "../../tools/reputation_manager.hpp"

#define ADD_CHECKPOINT(h, hash)  CHECK_AND_ASSERT(add_checkpoint(h,  hash), false);
#define ADD_CHECKPOINT2(h, hash, difficulty)  CHECK_AND_ASSERT(add_checkpoint(h,  hash, difficulty), false);
#define JSON_HASH_FILE_NAME "checkpoints.json"


namespace cryptonote
{
  /**
   * @brief A container for blockchain checkpoints
   *
   * A checkpoint is a pre-defined hash for the block at a given height.
   * Some of these are compiled-in, while others can be loaded at runtime
   * either from a json file or via DNS from a checkpoint-hosting server.
   */
  class checkpoints
  {
  public:

    /**
     * @brief default constructor
     */
    checkpoints();
    
    // Explicitly define move semantics for unique_ptr members
    checkpoints(checkpoints&&) = default;
    checkpoints& operator=(checkpoints&&) = default;
    
    // Delete copy semantics
    checkpoints(const checkpoints&) = delete;
    checkpoints& operator=(const checkpoints&) = delete;

    /**
     * @brief adds a checkpoint to the container
     *
     * @param height the height of the block the checkpoint is for
     * @param hash_str the hash of the block, as a string
     * @param difficulty_str the cumulative difficulty of the block, as a string (optional)
     *
     * @return false if parsing the hash fails, or if the height is a duplicate
     *         AND the existing checkpoint hash does not match the new one,
     *         otherwise returns true
     */
    bool add_checkpoint(uint64_t height, const std::string& hash_str, const std::string& difficulty_str = "");

    /**
     * @brief checks if there is a checkpoint in the future
     *
     * This function checks if the height passed is lower than the highest
     * checkpoint.
     *
     * @param height the height to check against
     *
     * @return false if no checkpoints, otherwise returns whether or not
     *         the height passed is lower than the highest checkpoint.
     */
    bool is_in_checkpoint_zone(uint64_t height) const;

    /**
     * @brief checks if the given height and hash agree with the checkpoints
     *
     * This function checks if the given height and hash exist in the
     * checkpoints container.  If so, it returns whether or not the passed
     * parameters match the stored values.
     *
     * @param height the height to be checked
     * @param h the hash to be checked
     * @param is_a_checkpoint return-by-reference if there is a checkpoint at the given height
     *
     * @return true if there is no checkpoint at the given height,
     *         true if the passed parameters match the stored checkpoint,
     *         false otherwise
     */
    bool check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const;

    /**
     * @overload
     */
    bool check_block(uint64_t height, const crypto::hash& h) const;

    /**
     * @brief checks if alternate chain blocks should be kept for a given height
     *
     * this basically says if the blockchain is smaller than the first
     * checkpoint then alternate blocks are allowed.  Alternatively, if the
     * last checkpoint *before* the end of the current chain is also before
     * the block to be added, then this is fine.
     *
     * @param blockchain_height the current blockchain height
     * @param block_height the height of the block to be added as alternate
     *
     * @return true if alternate blocks are allowed given the parameters,
     *         otherwise false
     */
    bool is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const;

    /**
     * @brief gets the highest checkpoint height
     *
     * @return the height of the highest checkpoint
     */
    uint64_t get_max_height() const;

    /**
     * @brief gets the highest checkpoint height less than the given block height
     *
     * @param block_height the reference block height
     * @return the nearest checkpoint height below block_height, or 0 if none
     */
    uint64_t get_nearest_checkpoint_height(uint64_t block_height) const;

    /**
     * @brief gets the checkpoints container
     *
     * @return a const reference to the checkpoints container
     */
    const std::map<uint64_t, crypto::hash>& get_points() const;

    /**
     * @brief gets the difficulty checkpoints container
     *
     * @return a const reference to the difficulty checkpoints container
     */
    const std::map<uint64_t, difficulty_type>& get_difficulty_points() const;

    /**
     * @brief checks if our checkpoints container conflicts with another
     *
     * A conflict refers to a case where both checkpoint sets have a checkpoint
     * for a specific height but their hashes for that height do not match.
     *
     * @param other the other checkpoints instance to check against
     *
     * @return false if any conflict is found, otherwise true
     */
    bool check_for_conflicts(const checkpoints& other) const;

    /**
     * @brief loads the default main chain checkpoints
     * @param nettype network type
     *
     * @return true unless adding a checkpoint fails
     */
    bool init_default_checkpoints(network_type nettype);

    /**
     * @brief load new checkpoints
     *
     * Loads new checkpoints from the specified json file, as well as
     * (optionally) from DNS.
     *
     * @param json_hashfile_fullpath path to the json checkpoints file
     * @param nettype network type
     * @param dns whether or not to load DNS checkpoints
     *
     * @return true if loading successful and no conflicts
     */
    bool load_new_checkpoints(const std::string &json_hashfile_fullpath, network_type nettype=MAINNET, bool dns=true);

    /**
     * @brief load new checkpoints from json
     *
     * @param json_hashfile_fullpath path to the json checkpoints file
     *
     * @return true if loading successful and no conflicts
     */
    bool load_checkpoints_from_json(const std::string &json_hashfile_fullpath);

    /**
     * @brief load new checkpoints from HTTP URL
     *
     * @param url the HTTP(S) URL to download checkpoints from
     *
     * @return true if loading successful and no conflicts
     */
    bool load_checkpoints_from_http(const std::string& url);

    /**
     * @brief load checkpoints from seed nodes (fallback mechanism)
     *
     * This function is called when the main checkpoint hosting is unavailable.
     * It attempts to load complete checkpoint history from hardcoded seed nodes.
     * Seeds contain full checkpoint history for validation and reconstruction.
     *
     * @return true if at least one seed node responds, false if all unreachable
     */
    bool load_checkpoints_from_seed_nodes();

    /**
     * @brief save checkpoints to local JSON file
     *
     * This function saves all currently loaded checkpoints to a JSON file.
     * Used for persisting downloaded checkpoints to disk for future sessions.
     * Ensures that checkpoints accumulate locally without loss between restarts.
     *
     * @param json_hashfile_fullpath path where to save the checkpoints JSON file
     *
     * @return true if save successful, false on error
     */
    bool save_checkpoints_to_json(const std::string &json_hashfile_fullpath);

    /**
     * @brief validate epoch to prevent replay attacks
     *
     * Checks if received epoch_id is newer than last known epoch from this source.
     * Prevents replay attacks where old checkpoints are served multiple times.
     *
     * @param source identifier of checkpoint source (URL, peer ID, etc)
     * @param received_epoch the epoch_id from the received checkpoint
     *
     * @return true if epoch is valid, false if replay detected
     */
    bool validate_epoch(const std::string &source, uint64_t received_epoch);

    /**
     * @brief verify checkpoint against seed nodes
     *
     * Validates received checkpoint against multiple seed nodes before accepting.
     * Prevents man-in-the-middle attacks and ensures checkpoint authenticity.
     *
     * @param checkpoint_data the checkpoint data to verify (as string)
     * @param received_epoch the epoch_id to verify
     *
     * @return true if verified by seeds, false otherwise
     */
    bool verify_with_seeds(const rapidjson::Document &received_checkpoints, uint64_t received_epoch);

    /**
     * @brief Save permanent bans to disk for persistence across restarts
     * @param ban_file_path path to save bans to
     * @return true if successful
     */
    bool save_permanent_bans(const std::string &ban_file_path);

    /**
     * @brief Load permanent bans from disk on startup
     * @param ban_file_path path to load bans from
     * @return true if successful
     */
    bool load_permanent_bans(const std::string &ban_file_path);

    /**
     * @brief Auto-repair corrupted checkpoints by consulting seed nodes
     * Called when a checkpoint conflict is detected during block validation
     * @param height block height that caused conflict
     * @param received_hash hash received from network
     * @param json_file_path path to checkpoint file to repair
     * @return true if successfully repaired, false otherwise
     */
    bool auto_repair_checkpoint_conflict(uint64_t height, const crypto::hash& received_hash, const std::string& json_file_path);

    /**
     * @brief Detect system language (es or en)
     * @return "es" for Spanish, "en" for English
     */
    std::string detect_system_language();

    /**
     * @brief Generate and display security alert report
     * @param height conflicting block height
     * @param false_hash hash stored locally (corrupted)
     * @param correct_hash hash received from seeds (correct)
     * @param json_file_path path to compromised file
     * @param language "es" or "en"
     */
    void generate_security_alert_report(uint64_t height, const std::string& false_hash, 
                                       const std::string& correct_hash, const std::string& json_file_path, 
                                       const std::string& language);

    // ========== PHASE 2: P2P Consensus & Reputation Tracking ==========
    
    /**
     * @brief Initiate a consensus query to detect LOCAL vs NETWORK attacks
     * @param height block height with suspected invalid checkpoint
     * @param suspect_hash hash that appears invalid
     * @return true if consensus indicates NETWORK attack, false if LOCAL or inconclusive
     */
    bool initiate_consensus_query(uint64_t height, const crypto::hash& suspect_hash);

    /**
     * @brief Handle incoming security query from peer
     * @param query the security query request
     * @return true if query was processed successfully
     */
    bool handle_security_query(const cryptonote::security::SecurityQuery& query);

    /**
     * @brief Handle incoming security response from peer
     * @param response the security response with validation result
     * @return true if response was processed and reputation updated
     */
    bool handle_security_response(const cryptonote::security::SecurityResponse& response);

    /**
     * @brief Report peer reputation based on validation result
     * @param peer_id peer identifier
     * @param was_valid true if peer's information was valid, false if invalid
     */
    void report_peer_reputation(const std::string& peer_id, bool was_valid);

    /**
     * @brief Get reputation score for a peer
     * @param peer_id peer identifier
     * @return reputation score (0.0-1.0), 0.5 if unknown
     */
    float get_peer_reputation(const std::string& peer_id) const;

    /**
     * @brief Check if peer is trusted (reputation >= threshold)
     * @param peer_id peer identifier
     * @return true if peer reputation is above trusted threshold
     */
    bool is_peer_trusted(const std::string& peer_id) const;

    /**
     * @brief Activate quarantine for node under selective attack
     * @param height block height being attacked
     * @param duration_seconds quarantine duration (1-6 hours = 3600-21600 seconds)
     */
    void activate_quarantine(uint64_t height, uint64_t duration_seconds = 3600);

    /**
     * @brief Check if node is currently under quarantine
     * @return true if quarantine is active
     */
    bool is_quarantined() const;

  private:
    std::map<uint64_t, crypto::hash> m_points; //!< the checkpoints container
    std::map<uint64_t, difficulty_type> m_difficulty_points; //!< the difficulty checkpoints container
    
    // Intelligent validation tracking - ENHANCED with tiered bans
    std::map<std::string, uint64_t> m_last_epoch_from_source; //!< track last epoch per source (replay detection)
    
    // Tiered ban system with peer reputation tracking (PHASE 2)
    enum class BanLevel { NONE = 0, TEMP_1H = 1, PERSISTENT_24H = 2, PERMANENT = 3 };
    enum class BanReason { 
      NONE = 0, 
      HASH_MISMATCH = 1,        // Checkpoint hash doesn't match seeds
      EPOCH_VIOLATION = 2,      // epoch_id not monotonic
      REPLAY_ATTACK = 3,        // Replaying old checkpoints
      INVALID_HEIGHT = 4,       // Height not sequential
      SEED_MISMATCH = 5         // Majority seed verification failed
    };
    struct BanInfo {
      BanLevel level;
      BanReason reason;         // Why was the peer banned
      uint64_t ban_timestamp;   // When ban was issued
      uint64_t last_epoch_seen; // Last valid epoch from this peer
      int violation_count;      // How many times violated rules
    };
    std::map<std::string, BanInfo> m_source_bans;           // PHASE 1.5 + 2
    std::set<std::string> m_permanent_ban_sources;          // PHASE 1.5
    std::set<std::string> m_quarantined_sources;            // PHASE 2
    std::map<std::string, int> m_source_failures;           // PHASE 2
    uint64_t m_current_epoch_id = 0;                        // PHASE 1

    // Global checkpoint metadata for persistence
    uint64_t m_current_generated_ts = 0; //!< timestamp when checkpoints were generated
    std::string m_current_network = "mainnet"; //!< network type (mainnet, testnet, stagenet)
    uint64_t m_checkpoint_interval = 30; //!< blocks per checkpoint (typically 30 = 60 minutes)
    
    // FAIL-SAFE: Corruption pause mode
    bool m_in_corruption_pause_mode = false; //!< If true, daemon is in PAUSE waiting for valid checkpoints
    uint64_t m_corruption_pause_started = 0; //!< Timestamp when corruption was detected

    // ========== PHASE 2: P2P Consensus & Reputation Tracking ==========
    std::unique_ptr<cryptonote::security::SecurityQueryTool> m_security_query_tool; //!< Consensus query manager
    std::unique_ptr<cryptonote::security::ReputationManager> m_reputation_manager;  //!< Peer reputation tracking
    
    // Quarantine state for nodes under selective attack
    bool m_is_quarantined = false;                    //!< Current quarantine status
    uint64_t m_quarantine_start_time = 0;             //!< When quarantine started
    uint64_t m_quarantine_duration_seconds = 3600;    //!< Default 1 hour quarantine
  };

}
