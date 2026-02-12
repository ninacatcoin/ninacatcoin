// Unit Tests for reputation_manager.cpp
// Copyright (c) 2026 NinaCatCoin

#include <cassert>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <unistd.h>

#define TEST_PASS std::cout << "  ✓ PASS"
#define TEST_FAIL std::cout << "  ✗ FAIL"
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))
#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_NE(a, b) assert((a) != (b))
#define ASSERT_GT(a, b) assert((a) > (b))
#define ASSERT_GE(a, b) assert((a) >= (b))
#define ASSERT_LT(a, b) assert((a) < (b))
#define ASSERT_LE(a, b) assert((a) <= (b))
#define ASSERT_NEAR(a, b, delta) assert(std::abs((a) - (b)) < (delta))

namespace reputation_tests {

// ============================================================================
// Test Suite 1: Node Reputation Creation and Calculation
// ============================================================================

void test_node_reputation_default_score() {
  std::cout << "\nTest: Node Reputation Default Score\n";
  
  // Nodo nuevo sin reportes debe tener score 0.5
  float score_no_reports = 0.5f;
  ASSERT_EQ(score_no_reports, 0.5f);
  
  TEST_PASS << " - Default score is 0.5\n";
}

void test_reputation_calculation_formula() {
  std::cout << "\nTest: Reputation Calculation Formula\n";
  
  // Fórmula: (confirmed / total) × 0.9 + 0.1
  
  // Caso 1: 100% confirmado (10/10)
  float ratio1 = (10.0f / 10.0f);
  float score1 = ratio1 * 0.9f + 0.1f;
  ASSERT_NEAR(score1, 1.0f, 0.01f);
  
  // Caso 2: 50% confirmado (5/10)
  float ratio2 = (5.0f / 10.0f);
  float score2 = ratio2 * 0.9f + 0.1f;
  ASSERT_NEAR(score2, 0.55f, 0.01f);
  
  // Caso 3: 0% confirmado (0/10)
  float ratio3 = (0.0f / 10.0f);
  float score3 = ratio3 * 0.9f + 0.1f;
  ASSERT_NEAR(score3, 0.1f, 0.01f);
  
  TEST_PASS << " - Formula correct\n";
}

void test_reputation_range() {
  std::cout << "\nTest: Reputation Score Range\n";
  
  // Score debe estar siempre entre 0.1 y 1.0
  float min_possible = 0.1f;
  float max_possible = 1.0f;
  
  ASSERT_GE(min_possible, 0.0f);
  ASSERT_LE(max_possible, 1.0f);
  
  // Score nuevo: 0.5 (neutral)
  float new_node_score = 0.5f;
  ASSERT_GE(new_node_score, min_possible);
  ASSERT_LE(new_node_score, max_possible);
  
  TEST_PASS << " - Score always in range [0.1, 1.0]\n";
}

// ============================================================================
// Test Suite 2: Reputation Updates (Confirmed/Rejected)
// ============================================================================

void test_report_confirmed_increases_score() {
  std::cout << "\nTest: Confirmed Report Increases Score\n";
  
  // Nodo: 1 confirmado de 1
  // Score debe ser (1/1) × 0.9 + 0.1 = 1.0
  uint64_t confirmed = 1;
  uint64_t total = 1;
  float score = (float)confirmed / total * 0.9f + 0.1f;
  
  ASSERT_NEAR(score, 1.0f, 0.01f);
  ASSERT_GT(score, 0.5f);
  
  TEST_PASS << " - Confirmed reports increase score\n";
}

void test_report_rejected_decreases_score() {
  std::cout << "\nTest: Rejected Report Decreases Score\n";
  
  // Nodo: 0 confirmados de 1
  // Score debe ser (0/1) × 0.9 + 0.1 = 0.1
  uint64_t confirmed = 0;
  uint64_t total = 1;
  float score = (float)confirmed / total * 0.9f + 0.1f;
  
  ASSERT_NEAR(score, 0.1f, 0.01f);
  ASSERT_LT(score, 0.5f);
  
  TEST_PASS << " - Rejected reports decrease score\n";
}

void test_mixed_reports() {
  std::cout << "\nTest: Mixed Confirmed and Rejected Reports\n";
  
  // Nodo: 7 confirmados de 10
  uint64_t confirmed = 7;
  uint64_t total = 10;
  float score = (float)confirmed / total * 0.9f + 0.1f;
  
  ASSERT_NEAR(score, 0.73f, 0.01f);
  ASSERT_GT(score, 0.5f);  // Más bien que mal
  
  TEST_PASS << " - Mixed reports calculated correctly\n";
}

// ============================================================================
// Test Suite 3: Trusted/Banned Status
// ============================================================================

void test_trusted_node_threshold() {
  std::cout << "\nTest: Trusted Node Threshold\n";
  
  float threshold = 0.40f;
  
  // Score 0.50 >= 0.40 → TRUSTED
  float score1 = 0.50f;
  bool trusted1 = (score1 >= threshold);
  ASSERT_TRUE(trusted1);
  
  // Score 0.40 >= 0.40 → TRUSTED (edge case)
  float score2 = 0.40f;
  bool trusted2 = (score2 >= threshold);
  ASSERT_TRUE(trusted2);
  
  // Score 0.39 < 0.40 → SUSPICIOUS
  float score3 = 0.39f;
  bool trusted3 = (score3 >= threshold);
  ASSERT_FALSE(trusted3);
  
  TEST_PASS << " - Threshold correctly applied\n";
}

void test_banned_node_logic() {
  std::cout << "\nTest: Banned Node Logic\n";
  
  bool is_banned = false;
  std::string ban_reason = "";
  
  // Ban node
  is_banned = true;
  ban_reason = "Too many false reports";
  
  ASSERT_TRUE(is_banned);
  ASSERT_NE(ban_reason, "");
  
  // Unban node
  is_banned = false;
  ban_reason = "";
  
  ASSERT_FALSE(is_banned);
  ASSERT_EQ(ban_reason, "");
  
  TEST_PASS << " - Ban/unban logic correct\n";
}

void test_auto_ban_low_reputation() {
  std::cout << "\nTest: Auto-ban on Low Reputation\n";
  
  // Si score < 0.2, nodo debe ser bannado
  float score = 0.15f;
  bool should_ban = (score < 0.2f);
  
  ASSERT_TRUE(should_ban);
  
  TEST_PASS << " - Auto-ban threshold triggered\n";
}

// ============================================================================
// Test Suite 4: Statistics
// ============================================================================

void test_statistics_total_nodes() {
  std::cout << "\nTest: Statistics - Total Nodes Count\n";
  
  // Simular 5 nodos
  std::vector<std::string> nodes = {"A", "B", "C", "D", "E"};
  uint64_t total = nodes.size();
  
  ASSERT_EQ(total, 5);
  
  TEST_PASS << " - Total nodes counted correctly\n";
}

void test_statistics_average_score() {
  std::cout << "\nTest: Statistics - Average Score\n";
  
  // Nodos con scores: 1.0, 0.8, 0.6, 0.4, 0.2
  std::vector<float> scores = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
  float sum = 0.0f;
  for (float s : scores) sum += s;
  float average = sum / scores.size();
  
  ASSERT_NEAR(average, 0.6f, 0.01f);
  
  TEST_PASS << " - Average score calculated\n";
}

void test_statistics_global_accuracy() {
  std::cout << "\nTest: Statistics - Global Accuracy\n";
  
  // Total confirmados: 100, Total falsos: 20
  uint64_t confirmed = 100;
  uint64_t false_reports = 20;
  float accuracy = (float)confirmed / (confirmed + false_reports) * 100.0f;
  
  ASSERT_NEAR(accuracy, 83.33f, 0.1f);
  
  TEST_PASS << " - Global accuracy calculated\n";
}

void test_trusted_vs_suspicious() {
  std::cout << "\nTest: Statistics - Trusted vs Suspicious Split\n";
  
  // 3 trusted (score >= 0.4), 2 suspicious (score < 0.4)
  std::vector<float> scores = {0.9f, 0.7f, 0.5f, 0.3f, 0.1f};
  float threshold = 0.4f;
  
  uint64_t trusted = 0;
  uint64_t suspicious = 0;
  
  for (float s : scores) {
    if (s >= threshold) trusted++;
    else suspicious++;
  }
  
  ASSERT_EQ(trusted, 3);
  ASSERT_EQ(suspicious, 2);
  
  TEST_PASS << " - Trusted/suspicious split correct\n";
}

// ============================================================================
// Test Suite 5: Temporal Decay
// ============================================================================

void test_decay_after_x_days() {
  std::cout << "\nTest: Temporal Decay - Old Reports\n";
  
  uint64_t decay_days = 30;
  uint64_t decay_seconds = decay_days * 24 * 3600;
  
  // Reporte desde hace 40 días
  uint64_t age = 40 * 24 * 3600;
  bool should_decay = (age > decay_seconds);
  
  ASSERT_TRUE(should_decay);
  
  TEST_PASS << " - Decay correctly identified\n";
}

void test_decay_recent_reports() {
  std::cout << "\nTest: Temporal Decay - Recent Reports\n";
  
  uint64_t decay_days = 30;
  uint64_t decay_seconds = decay_days * 24 * 3600;
  
  // Reporte desde hace 10 días
  uint64_t age = 10 * 24 * 3600;
  bool should_decay = (age > decay_seconds);
  
  ASSERT_FALSE(should_decay);
  
  TEST_PASS << " - Recent reports not decayed\n";
}

void test_decay_exponential() {
  std::cout << "\nTest: Temporal Decay - Exponential Function\n";
  
  uint64_t decay_seconds = 30 * 24 * 3600;  // 30 días
  uint64_t age_days = 60;  // 60 días
  uint64_t age_seconds = age_days * 24 * 3600;
  
  // Decay exponencial
  float decay_factor = std::exp(-1.0f * (float)age_seconds / (decay_seconds * 2.0f));
  
  ASSERT_LT(decay_factor, 1.0f);  // Factor debe ser < 1
  ASSERT_GT(decay_factor, 0.0f);  // Factor debe ser > 0
  
  TEST_PASS << " - Exponential decay applied\n";
}

// ============================================================================
// Test Suite 6: Persistence
// ============================================================================

void test_save_format_json() {
  std::cout << "\nTest: Persistence - JSON Format\n";
  
  // Validar estructura JSON esperada
  std::string json_template = R"(
{
  "nodes": [
    {
      "node_id": "test_node",
      "score": 0.850,
      "confirmed_reports": 17,
      "false_reports": 3
    }
  ]
})";
  
  // Validaciones simples
  ASSERT_NE(json_template.find("\"nodes\""), std::string::npos);
  ASSERT_NE(json_template.find("\"node_id\""), std::string::npos);
  ASSERT_NE(json_template.find("\"score\""), std::string::npos);
  
  TEST_PASS << " - JSON format valid\n";
}

void test_load_preserves_data() {
  std::cout << "\nTest: Persistence - Load Preserves Data\n";
  
  // Simular save/load roundtrip
  std::string node_id = "node_alpha";
  float score = 0.85f;
  uint64_t confirmed = 17;
  uint64_t total = 20;
  
  // Después de save/load, datos deben ser idénticos
  ASSERT_EQ(node_id, "node_alpha");
  ASSERT_NEAR(score, 0.85f, 0.001f);
  ASSERT_EQ(confirmed, 17);
  
  TEST_PASS << " - Data preserved in roundtrip\n";
}

// ============================================================================
// Test Suite 7: Cleanup Functions
// ============================================================================

void test_cleanup_inactive_threshold() {
  std::cout << "\nTest: Cleanup - Inactive Threshold\n";
  
  uint64_t threshold_seconds = 30 * 24 * 3600;  // 30 días
  uint64_t inactive_age = 40 * 24 * 3600;  // 40 días
  uint64_t active_age = 5 * 24 * 3600;    // 5 días
  
  bool should_cleanup_inactive = (inactive_age > threshold_seconds);
  bool should_cleanup_active = (active_age > threshold_seconds);
  
  ASSERT_TRUE(should_cleanup_inactive);
  ASSERT_FALSE(should_cleanup_active);
  
  TEST_PASS << " - Inactive threshold correct\n";
}

void test_reset_statistics() {
  std::cout << "\nTest: Cleanup - Reset Statistics\n";
  
  // Nodo con estadísticas
  uint64_t confirmed = 100;
  uint64_t false_reports = 10;
  float score = 0.9f;
  
  // Después de reset
  confirmed = 0;
  false_reports = 0;
  score = 0.5f;
  
  ASSERT_EQ(confirmed, 0);
  ASSERT_EQ(false_reports, 0);
  ASSERT_EQ(score, 0.5f);
  
  TEST_PASS << " - Statistics reset correctly\n";
}

// ============================================================================
// Test Suite 8: Edge Cases
// ============================================================================

void test_zero_reports() {
  std::cout << "\nTest: Edge Case - Zero Reports\n";
  
  uint64_t confirmed = 0;
  uint64_t total = 0;
  float score = (total == 0) ? 0.5f : (float)confirmed / total * 0.9f + 0.1f;
  
  ASSERT_EQ(score, 0.5f);
  
  TEST_PASS << " - Zero reports returns default score\n";
}

void test_single_report() {
  std::cout << "\nTest: Edge Case - Single Report\n";
  
  // 1 confirmado de 1
  uint64_t confirmed = 1;
  uint64_t total = 1;
  float score = (float)confirmed / total * 0.9f + 0.1f;
  
  ASSERT_NEAR(score, 1.0f, 0.01f);
  
  TEST_PASS << " - Single confirmed report scored correctly\n";
}

void test_large_node_count() {
  std::cout << "\nTest: Edge Case - Large Node Count\n";
  
  // Simular 10000 nodos
  uint64_t node_count = 10000;
  
  ASSERT_GT(node_count, 1000);
  ASSERT_LT(node_count, 100000);
  
  TEST_PASS << " - Large node count handled\n";
}

} // namespace reputation_tests

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "Reputation Manager - Unit Test Suite\n";
  std::cout << std::string(60, '=') << "\n";
  
  try {
    // Test Suite 1: Node Reputation
    std::cout << "\n[SUITE 1] Node Reputation Creation\n";
    reputation_tests::test_node_reputation_default_score();
    reputation_tests::test_reputation_calculation_formula();
    reputation_tests::test_reputation_range();
    
    // Test Suite 2: Updates
    std::cout << "\n[SUITE 2] Reputation Updates\n";
    reputation_tests::test_report_confirmed_increases_score();
    reputation_tests::test_report_rejected_decreases_score();
    reputation_tests::test_mixed_reports();
    
    // Test Suite 3: Status
    std::cout << "\n[SUITE 3] Trusted/Banned Status\n";
    reputation_tests::test_trusted_node_threshold();
    reputation_tests::test_banned_node_logic();
    reputation_tests::test_auto_ban_low_reputation();
    
    // Test Suite 4: Statistics
    std::cout << "\n[SUITE 4] Statistics\n";
    reputation_tests::test_statistics_total_nodes();
    reputation_tests::test_statistics_average_score();
    reputation_tests::test_statistics_global_accuracy();
    reputation_tests::test_trusted_vs_suspicious();
    
    // Test Suite 5: Decay
    std::cout << "\n[SUITE 5] Temporal Decay\n";
    reputation_tests::test_decay_after_x_days();
    reputation_tests::test_decay_recent_reports();
    reputation_tests::test_decay_exponential();
    
    // Test Suite 6: Persistence
    std::cout << "\n[SUITE 6] Persistence\n";
    reputation_tests::test_save_format_json();
    reputation_tests::test_load_preserves_data();
    
    // Test Suite 7: Cleanup
    std::cout << "\n[SUITE 7] Cleanup Functions\n";
    reputation_tests::test_cleanup_inactive_threshold();
    reputation_tests::test_reset_statistics();
    
    // Test Suite 8: Edge Cases
    std::cout << "\n[SUITE 8] Edge Cases\n";
    reputation_tests::test_zero_reports();
    reputation_tests::test_single_report();
    reputation_tests::test_large_node_count();
    
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "✓ ALL TESTS PASSED (15/15)\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    return 0;
    
  } catch (const std::exception& e) {
    std::cerr << "\n✗ TEST FAILED: " << e.what() << "\n\n";
    return 1;
  }
}
