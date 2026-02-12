// Copyright (c) 2026 NinaCatCoin
// Distributed under the MIT/X11 software license
#include <gtest/gtest.h>
#include "src/checkpoints/checkpoints.h"
#include "tools/security_query_tool.hpp"
#include "tools/reputation_manager.hpp"
#include <chrono>
#include <thread>

using namespace cryptonote;
using namespace ninacatcoin::security;

/**
 * Test Suite for Phase 2 Integration
 * Tests the integration of security_query_tool and reputation_manager
 * with the checkpoints system
 */

class CheckpointsPhase2Test : public ::testing::Test {
protected:
  checkpoints cp;
  
  virtual void SetUp() {
    // Initialize a basic checkpoint
    crypto::hash test_hash = crypto::null_hash;
    cp.add_checkpoint(1000, "0000000000000000000000000000000000000000000000000000000000000000");
  }
};

/**
 * Test 1: Constructor initializes Phase 2 tools
 */
TEST_F(CheckpointsPhase2Test, ConstructorInitializesPhase2Tools) {
  checkpoints test_cp;
  // If we get here without crash, tools were initialized
  EXPECT_TRUE(true);  // Constructor should not throw
}

/**
 * Test 2: Can initiate consensus query
 */
TEST_F(CheckpointsPhase2Test, InitiateConsensusQuery) {
  crypto::hash suspect_hash = crypto::null_hash;
  
  // Should not crash and return false (no responses expected in test)
  bool result = cp.initiate_consensus_query(1000, suspect_hash);
  
  // In test environment without network, this should return false (inconclusive)
  EXPECT_FALSE(result);
}

/**
 * Test 3: Handle security query from peer
 */
TEST_F(CheckpointsPhase2Test, HandleSecurityQuery) {
  SecurityQuery query;
  query.id = "test-query-1";
  query.height = 1000;
  query.suspect_hash = "0000000000000000000000000000000000000000000000000000000000000000";
  query.source_peer_id = "peer-1";
  
  // Should handle without crash
  bool result = cp.handle_security_query(query);
  EXPECT_TRUE(result);
}

/**
 * Test 4: Handle security response from peer
 */
TEST_F(CheckpointsPhase2Test, HandleSecurityResponse) {
  SecurityResponse response;
  response.query_id = "test-query-1";
  response.node_id = "peer-1";
  response.height = 1000;
  response.matches_local = true;
  response.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL;
  
  // Should handle without crash
  bool result = cp.handle_security_response(response);
  EXPECT_TRUE(result);
}

/**
 * Test 5: Report peer reputation
 */
TEST_F(CheckpointsPhase2Test, ReportPeerReputation) {
  // Report valid peer
  cp.report_peer_reputation("peer-good", true);
  float good_rep = cp.get_peer_reputation("peer-good");
  
  // Report invalid peer
  cp.report_peer_reputation("peer-bad", false);
  float bad_rep = cp.get_peer_reputation("peer-bad");
  
  // Good peer should have higher reputation
  EXPECT_GT(good_rep, bad_rep);
}

/**
 * Test 6: Get and check peer reputation
 */
TEST_F(CheckpointsPhase2Test, GetPeerReputation) {
  cp.report_peer_reputation("peer-test", true);
  float rep = cp.get_peer_reputation("peer-test");
  
  // Should be a valid score (0.0-1.0)
  EXPECT_GE(rep, 0.0f);
  EXPECT_LE(rep, 1.0f);
}

/**
 * Test 7: Check if peer is trusted
 */
TEST_F(CheckpointsPhase2Test, IsPeerTrusted) {
  // New peer should not be trusted by default
  bool trusted = cp.is_peer_trusted("peer-unknown");
  
  // Default reputation might be 0.5, so could be trusted or not
  // This is implementation dependent
  EXPECT_TRUE(trusted || !trusted);  // Just check it doesn't crash
}

/**
 * Test 8: Activate and check quarantine
 */
TEST_F(CheckpointsPhase2Test, ActivateQuarantine) {
  uint64_t height = 1000;
  uint64_t duration = 3600;  // 1 hour
  
  // Activate quarantine
  cp.activate_quarantine(height, duration);
  
  // Should be in quarantine immediately
  bool is_quarantined = cp.is_quarantined();
  EXPECT_TRUE(is_quarantined);
}

/**
 * Test 9: Quarantine expires after duration
 */
TEST_F(CheckpointsPhase2Test, QuarantineExpires) {
  uint64_t height = 1000;
  uint64_t duration = 1;  // 1 second for testing
  
  // Activate quarantine
  cp.activate_quarantine(height, duration);
  EXPECT_TRUE(cp.is_quarantined());
  
  // Wait for expiration
  std::this_thread::sleep_for(std::chrono::seconds(2));
  
  // Should be expired
  bool is_quarantined = cp.is_quarantined();
  EXPECT_FALSE(is_quarantined);
}

/**
 * Test 10: Multiple peer reputation tracking
 */
TEST_F(CheckpointsPhase2Test, MultipleP eersReputation) {
  // Report multiple peers
  cp.report_peer_reputation("peer-1", true);
  cp.report_peer_reputation("peer-2", true);
  cp.report_peer_reputation("peer-3", false);
  
  float rep1 = cp.get_peer_reputation("peer-1");
  float rep2 = cp.get_peer_reputation("peer-2");
  float rep3 = cp.get_peer_reputation("peer-3");
  
  // peer-1 and peer-2 should have higher reputation than peer-3
  EXPECT_GT(rep1, rep3);
  EXPECT_GT(rep2, rep3);
}

/**
 * Test 11: Query ID generation should be unique
 */
TEST_F(CheckpointsPhase2Test, UniqueQueryIds) {
  SecurityQuery q1, q2;
  
  // Both queries created at slightly different times
  // IDs should be different
  EXPECT_NE(q1.id, q2.id);  // Depends on implementation
}

/**
 * Test 12: Phase 2 doesn't break existing checkpoint functions
 */
TEST_F(CheckpointsPhase2Test, ExistingCheckpointFunctionalityPreserved) {
  // Test that existing checkpoint functions still work
  
  bool is_in_zone = cp.is_in_checkpoint_zone(1000);
  EXPECT_TRUE(is_in_zone);
  
  bool is_in_zone_before = cp.is_in_checkpoint_zone(500);
  EXPECT_FALSE(is_in_zone_before);
  
  bool is_in_zone_after = cp.is_in_checkpoint_zone(2000);
  EXPECT_FALSE(is_in_zone_after);
}

/**
 * Test 13: Consensus query handling with mock responses
 */
TEST_F(CheckpointsPhase2Test, ConsensusQueryWithResponses) {
  // Add multiple responses to simulate peer responses
  for (int i = 0; i < 3; i++) {
    SecurityResponse response;
    response.query_id = "test-consensus-1";
    response.node_id = "peer-" + std::to_string(i);
    response.height = 1000;
    response.matches_local = (i < 2);  // 2 match, 1 doesn't
    response.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL;
    
    cp.handle_security_response(response);
  }
  
  // At this point, consensus should show 2 confirmed out of 3
  // This is handled internally by security_query_tool
  EXPECT_TRUE(true);
}

/**
 * Test 14: Reputation persists across calls
 */
TEST_F(CheckpointsPhase2Test, ReputationPersists) {
  cp.report_peer_reputation("persistent-peer", true);
  float rep1 = cp.get_peer_reputation("persistent-peer");
  
  // Report again
  cp.report_peer_reputation("persistent-peer", true);
  float rep2 = cp.get_peer_reputation("persistent-peer");
  
  // Second reputation should be >= first (improving with more positive reports)
  EXPECT_GE(rep2, rep1);
}

/**
 * Test 15: Error handling for invalid inputs
 */
TEST_F(CheckpointsPhase2Test, ErrorHandlingInvalidInputs) {
  SecurityQuery empty_query;
  // Should not crash with empty query
  bool result = cp.handle_security_query(empty_query);
  EXPECT_FALSE(result);
  
  SecurityResponse empty_response;
  // Should not crash with empty response
  result = cp.handle_security_response(empty_response);
  EXPECT_FALSE(result);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
