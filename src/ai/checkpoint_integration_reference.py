#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IA CHECKPOINT MONITORING - QUICK REFERENCE GUIDE
═════════════════════════════════════════════════════════════════

This is a quick reference for what functions to call and when, during
the implementation of checkpoint monitoring in the ninacatcoin daemon.

USAGE: Keep this nearby while implementing integration!
"""

# ═════════════════════════════════════════════════════════════════════════════
# CHECKLIST: Implementation Steps
# ═════════════════════════════════════════════════════════════════════════════

implementation_checklist = {
    "Phase 1: Code Structure": {
        "Create ai_checkpoint_monitor.hpp": "✓ DONE",
        "Create ai_checkpoint_monitor.cpp": "✓ DONE",
        "Create integration guide markdown": "✓ DONE",
        "Create examples file": "✓ DONE",
        "Add to CMakeLists.txt": "→ TODO"
    },
    
    "Phase 2: Daemon Integration": {
        "Add #include in daemon.cpp": "→ TODO",
        "Call ia_checkpoint_monitor_initialize()": "→ TODO",
        "Print startup analysis": "→ TODO"
    },
    
    "Phase 3: Checkpoint Loading": {
        "Track where checkpoints loaded (checkpoints.cpp)": "→ TODO",
        "Call ia_register_checkpoint() for each": "→ TODO",
        "Call ia_set_checkpoint_network() early": "→ TODO"
    },
    
    "Phase 4: Block Validation": {
        "In blockchain.cpp validate_block()": "→ TODO",
        "Call ia_verify_block_against_checkpoints()": "→ TODO",
        "Handle rejection if checkpoint mismatch": "→ TODO"
    },
    
    "Phase 5: P2P Verification": {
        "In p2p.cpp peer verification": "→ TODO",
        "Call ia_detect_checkpoint_fork()": "→ TODO",
        "Mark peer untrusted if fork detected": "→ TODO"
    },
    
    "Phase 6: Monitoring Loop": {
        "In AIModule::monitor_loop()": "→ TODO",
        "Add checkpoint health checks": "→ TODO",
        "Log status periodically": "→ TODO"
    },
    
    "Phase 7: Testing": {
        "Test checkpoint loading": "→ TODO",
        "Test block validation": "→ TODO",
        "Test fork detection": "→ TODO",
        "Test monitoring output": "→ TODO"
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# QUICK REFERENCE: Functions to Call
# ═════════════════════════════════════════════════════════════════════════════

function_reference = {
    
    # ─────────────────────────────────────────────────────────────────────────
    "INITIALIZATION FUNCTIONS":
    {
        "ia_checkpoint_monitor_initialize()": {
            "Location": "daemon.cpp main startup",
            "When": "Once at startup, after IA module init",
            "Does": "Initialize checkpoint monitor system",
            "Example": "ia_checkpoint_monitor_initialize();",
            "Returns": "void",
            "Called by": "Daemon constructor"
        },
        
        "ia_set_checkpoint_network(network_type)": {
            "Location": "daemon.cpp or checkpoints.cpp",
            "When": "When network type is known (mainnet/testnet/stagenet)",
            "Does": "Tell IA which network we're on",
            "Example": 'ia_set_checkpoint_network("mainnet");',
            "Args": '"mainnet" | "testnet" | "stagenet"',
            "Returns": "void",
            "Called by": "Daemon after network selection"
        }
    },
    
    # ─────────────────────────────────────────────────────────────────────────
    "CHECKPOINT REGISTRATION FUNCTIONS":
    {
        "ia_register_checkpoint(height, hash, difficulty, source)": {
            "Location": "checkpoints.cpp add_checkpoint() or when loading",
            "When": "Every time a checkpoint is loaded (compiled, JSON, DNS)",
            "Does": "Register checkpoint with IA for monitoring",
            "Example": '''ia_register_checkpoint(
    1000000,
    "abc123...",
    "difficulty_value",
    "json"
);''',
            "Args": [
                "height: uint64_t - block number",
                "hash: std::string - hex string",
                "difficulty: std::string - as string",
                "source: std::string - 'compiled', 'json', or 'dns'"
            ],
            "Returns": "void",
            "Called by": "Checkpoint loading code"
        }
    },
    
    # ─────────────────────────────────────────────────────────────────────────
    "VALIDATION FUNCTIONS":
    {
        "ia_verify_block_against_checkpoints(height, hash)": {
            "Location": "blockchain.cpp validate_block()",
            "When": "For every block that enters the system",
            "Does": "Verify block hash against ANY known checkpoint at that height",
            "Example": '''if (!ia_verify_block_against_checkpoints(block.height, block.hash)) {
    LOG(ERROR) << "Block rejected by checkpoint check";
    return false;
}''',
            "Args": [
                "height: uint64_t - block height",
                "hash: std::string - block hash (hex)"
            ],
            "Returns": "bool - true if valid, false if checkpoint mismatch",
            "Logic": {
                "No checkpoint at height": "return true (can't verify)",
                "Checkpoint exists, hash matches": "return true (valid)",
                "Checkpoint exists, hash different": "return false (REJECT)"
            },
            "Called by": "Block validation code"
        },
        
        "ia_detect_checkpoint_fork(peer_knowledge, &conflict_height)": {
            "Location": "p2p.cpp when verifying peer",
            "When": "When comparing with peer's checkpoint data",
            "Does": "Detect if peer is on a different fork (conflicting checkpoints)",
            "Example": '''uint64_t conflict = 0;
if (ia_detect_checkpoint_fork(peer.checkpoints, conflict)) {
    LOG(ERROR) << "Fork detected at height " << conflict;
    peer.mark_untrusted();
    return false;
}''',
            "Args": [
                "peer_knowledge: CheckpointKnowledge - peer's data",
                "conflict_height: uint64_t* - output param for conflict height"
            ],
            "Returns": "bool - true if fork detected, false if no conflict",
            "Called by": "P2P peer verification"
        }
    },
    
    # ─────────────────────────────────────────────────────────────────────────
    "INFORMATION FUNCTIONS":
    {
        "ia_get_checkpoint_knowledge()": {
            "Location": "Any code that needs checkpoint data",
            "When": "When you need to access IA's checkpoint knowledge",
            "Does": "Return current checkpoint database",
            "Example": '''auto knowledge = ia_get_checkpoint_knowledge();
MINFO("Total checkpoints: " << knowledge.total_checkpoints);''',
            "Returns": "CheckpointKnowledge struct",
            "Contains": {
                "checkpoints": "map[height → CheckpointData]",
                "earliest_checkpoint_height": "uint64_t",
                "latest_checkpoint_height": "uint64_t",
                "total_checkpoints": "uint64_t",
                "network_type": "std::string"
            }
        },
        
        "ia_get_checkpoint_count()": {
            "Location": "Debug/status reporting",
            "When": "When you need count of checkpoints",
            "Example": 'MINFO("Checkpoints: " << ia_get_checkpoint_count());',
            "Returns": "uint64_t - number of checkpoints"
        },
        
        "ia_get_latest_checkpoint_height()": {
            "Location": "Status queries",
            "When": "To get newest checkpoint height",
            "Example": 'auto h = ia_get_latest_checkpoint_height();',
            "Returns": "uint64_t"
        },
        
        "ia_get_earliest_checkpoint_height()": {
            "Location": "Status queries",
            "When": "To get oldest checkpoint height",
            "Example": 'auto h = ia_get_earliest_checkpoint_height();',
            "Returns": "uint64_t"
        }
    },
    
    # ─────────────────────────────────────────────────────────────────────────
    "ANALYSIS & REPORTING FUNCTIONS":
    {
        "ia_print_checkpoint_status()": {
            "Location": "Daemon startup or debug commands",
            "When": "To print full status to logs",
            "Does": "Output checkpoint monitoring status (to MINFO logs)",
            "Example": "ia_print_checkpoint_status();",
            "Output": "Multiline status report with statistics",
            "Returns": "void"
        },
        
        "ia_print_checkpoint_analysis()": {
            "Location": "Daemon startup or debug commands",
            "When": "To print distribution analysis",
            "Does": "Analyze spacing and distribution of checkpoints",
            "Example": "ia_print_checkpoint_analysis();",
            "Output": "ASCII table of checkpoint distribution",
            "Returns": "void"
        },
        
        "ia_print_checkpoint_details()": {
            "Location": "Debug commands",
            "When": "For detailed debugging",
            "Does": "Print EVERY checkpoint with full data",
            "Example": "ia_print_checkpoint_details();",
            "Output": "One line per checkpoint, very detailed",
            "Returns": "void"
        },
        
        "ia_get_checkpoint_optimization_recommendations()": {
            "Location": "Daemon startup",
            "When": "To get optimization suggestions",
            "Does": "Recommend checkpoint loading optimizations",
            "Example": '''MINFO(ia_get_checkpoint_optimization_recommendations());''',
            "Returns": "std::string - recommendations text"
        },
        
        "ia_get_verification_strategy()": {
            "Location": "Daemon startup",
            "When": "To get verification strategy",
            "Does": "Recommend optimal block verification approach",
            "Example": 'MINFO(ia_get_verification_strategy());',
            "Returns": "std::string - strategy text"
        }
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# LOCATION MAPPING: Where to integrate
# ═════════════════════════════════════════════════════════════════════════════

integration_locations = {
    
    "src/daemon/daemon.cpp": {
        "Section": "Constructor",
        "Add": [
            '#include "ai/ai_checkpoint_monitor.hpp"',
            'ia_checkpoint_monitor_initialize();',
            'ia_set_checkpoint_network("mainnet");',
            'ia_print_checkpoint_status();'
        ],
        "When": "During daemon startup, after IA initialization",
        "Order": [
            "1. IA Module init (already done)",
            "2. Checkpoint monitor init (NEW)",
            "3. Checkpoint load code",
            "4. Print status (NEW)"
        ]
    },
    
    "src/checkpoints/checkpoints.cpp": {
        "Section": "add_checkpoint() function",
        "Add": [
            'ia_register_checkpoint(height, hash, difficulty, "compiled");',
            '// or "json" or "dns" depending on source'
        ],
        "When": "Every time a checkpoint is added",
        "For JSON": "In JSON loading loop, use source='json'",
        "For DNS": "In DNS loading code, use source='dns'"
    },
    
    "src/checkpoints/checkpoints.cpp": {
        "Section": "Constructor or init_default_checkpoints()",
        "Add": 'ia_set_checkpoint_network(nettype);',
        "When": "When network type is determined"
    },
    
    "src/blockchain/blockchain.cpp": {
        "Section": "validate_block() function",
        "Add": [
            'if (!ia_verify_block_against_checkpoints(block.height, block.hash)) {',
            '    MERROR("Block rejected: checkpoint mismatch");',
            '    return false;',
            '}'
        ],
        "When": "First check in validate_block(), before other validations",
        "Order": "1. Checkpoint check (NEW), 2. PoW check, 3. Signatures, etc"
    },
    
    "src/p2p/p2p_peer.cpp": {
        "Section": "Peer verification function",
        "Add": [
            'uint64_t conflict = 0;',
            'if (ia_detect_checkpoint_fork(peer.checkpoints, conflict)) {',
            '    peer.set_untrusted();',
            '}'
        ],
        "When": "When verifying peer compatibility"
    },
    
    "src/ai/ai_module.cpp": {
        "Section": "AIModule::monitor_loop()",
        "Add": [
            'auto cp_knowledge = ia_get_checkpoint_knowledge();',
            'if (cp_knowledge.total_checkpoints > 0) {',
            '    MDEBUG("Checkpoints: " << cp_knowledge.total_checkpoints);',
            '}'
        ],
        "When": "In the continuous monitoring loop (~every 60 seconds)"
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# EXECUTION ORDER: When functions are called during lifecycle
# ═════════════════════════════════════════════════════════════════════════════

execution_flow = """
DAEMON STARTUP SEQUENCE
═══════════════════════

1. daemon.cpp main()
2. daemon.cpp constructor
3.   └─ AIModule::getInstance().initialize()
4.   └─ ia_checkpoint_monitor_initialize()  [NEW]
5.   └─ ia_set_checkpoint_network("mainnet")  [NEW]
6.   └─ checkpoints::instance().init_default_checkpoints()
         └─ For each compiled checkpoint:
         └─   ia_register_checkpoint(...)  [NEW]
7.   └─ checkpoints::instance().load_from_json()
         └─ For each JSON checkpoint:
         └─   ia_register_checkpoint(..., "json")  [NEW]
8.   └─ checkpoints::instance().load_from_dns()
         └─ For each DNS checkpoint:
         └─   ia_register_checkpoint(..., "dns")  [NEW]
9.   └─ ia_print_checkpoint_status()  [NEW]
10.  └─ p2p_network::init()
11.  └─ blockchain::start_sync()


BLOCK VALIDATION SEQUENCE (for each block received)
════════════════════════════════════════════════════

1. blockchain::validate_block(block)
2.   └─ ia_verify_block_against_checkpoints(block.height, block.hash)  [NEW]
         └─ If checkpoint: verify hash
         └─ If no checkpoint: return true
         └─ If mismatch: return false (REJECT)
3.   └─ If checkpoint check failed: return false
4.   └─ validate_pow(block)
5.   └─ validate_timestamp(block)
6.   └─ All OK: return true


PEER VERIFICATION SEQUENCE (when peer connects)
════════════════════════════════════════════════

1. p2p::verify_peer(peer)
2.   └─ ia_detect_checkpoint_fork(peer.checkpoints, &conflict)  [NEW]
         └─ Compare all checkpoints
         └─ If conflict: return true
         └─ If OK: return false
3.   └─ If fork detected: mark untrusted, disconnect
4.   └─ If OK: mark trusted, continue


IA MONITORING LOOP (every 60 seconds)
═════════════════════════════════════

1. AIModule::monitor_loop()
2.   └─ Every iteration:
         └─ ia_get_checkpoint_knowledge()  [NEW]
         └─ Check for anomalies  [NEW]
3.   └─ Every 10 iterations (10 min):
         └─ ia_print_checkpoint_analysis()  [NEW]
"""

# ═════════════════════════════════════════════════════════════════════════════
# CONFIGURATION: What to customize
# ═════════════════════════════════════════════════════════════════════════════

customization_options = {
    "Network Type": {
        "Options": ["mainnet", "testnet", "stagenet"],
        "Set in": "ia_set_checkpoint_network()",
        "Default": "mainnet",
        "When": "During daemon startup"
    },
    
    "Checkpoint Sources": {
        "Options": ["compiled", "json", "dns"],
        "Register with": "ia_register_checkpoint(..., source)",
        "Priority": "compiled > json > dns",
        "Default": "Load all available"
    },
    
    "Verification Strategy": {
        "Options": "IA recommends based on checkpoint count",
        "Few checkpoints": "Verify all blocks",
        "Many checkpoints": "Trust checkpoints, spot-check",
        "Query with": "ia_get_verification_strategy()"
    },
    
    "Monitoring Frequency": {
        "Default": "Every 60 seconds",
        "Detailed analysis": "Every 10 monitoring cycles (10 min)",
        "Customize in": "AIModule::monitor_loop()"
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# RETURN VALUES: What each function returns
# ═════════════════════════════════════════════════════════════════════════════

return_values = {
    "ia_verify_block_against_checkpoints()": {
        "true": "Block is valid (checkpoint matches or no checkpoint exists)",
        "false": "Block is INVALID (checkpoint mismatch - MUST REJECT)"
    },
    
    "ia_detect_checkpoint_fork()": {
        "true": "FORK DETECTED (peer is on different blockchain)",
        "false": "No fork (peer checkpoints compatible)"
    },
    
    "ia_get_checkpoint_count()": {
        "0": "No checkpoints loaded (unusual)",
        ">0": "Number of checkpoints being monitored"
    },
    
    "String functions": {
        "Empty": "No data available",
        "Non-empty": "Contains analysis/recommendation text"
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# DEBUGGING: How to verify integration works
# ═════════════════════════════════════════════════════════════════════════════

debugging_checklist = {
    "Startup": [
        "√ See 'ia_checkpoint_monitor_initialize()' in logs",
        "√ See 'Total checkpoints: X' in logs",
        "√ See checkpoint distribution analysis",
        "√ See verification strategy recommendation"
    ],
    
    "Block Validation": [
        "√ Enable MDEBUG logging",
        "√ Look for '[CHECKPOINTS] Verified block' messages",
        "√ Any checkpoint mismatch → '[CHECKPOINTS] MISMATCH' error",
        "√ Try sending block with wrong hash at checkpoint height"
    ],
    
    "Fork Detection": [
        "√ Connect to peer with conflicting checkpoint",
        "√ Should see '[P2P] ✗ FORK DETECTED' message",
        "√ Peer should be marked untrusted",
        "√ Conflict height should be logged"
    ],
    
    "Monitoring": [
        "√ Every 60 seconds should see checkpoint monitor output",
        "√ Every 10 minutes detailed analysis output",
        "√ No errors in logs",
        "√ Alerts for any anomalies"
    ]
}

# ═════════════════════════════════════════════════════════════════════════════
# SUMMARY: One-page implementation guide
# ═════════════════════════════════════════════════════════════════════════════

quick_implementation_guide = """
QUICK IMPLEMENTATION GUIDE
══════════════════════════════════════════════════════════════════════

FILE CHANGES NEEDED:

1. src/CMakeLists.txt
   ├─ Add ai_checkpoint_monitor.hpp to sources
   └─ Add ai_checkpoint_monitor.cpp to sources

2. src/daemon/daemon.cpp
   ├─ #include "ai/ai_checkpoint_monitor.hpp"
   ├─ In constructor: ia_checkpoint_monitor_initialize()
   └─ In constructor: ia_set_checkpoint_network("mainnet")

3. src/checkpoints/checkpoints.cpp
   ├─ In add_checkpoint(): ia_register_checkpoint(...)
   ├─ In init_default_checkpoints(): ia_set_checkpoint_network()
   └─ In load_from_json(): ia_register_checkpoint(..., "json")

4. src/blockchain/blockchain.cpp
   ├─ In validate_block(): ia_verify_block_against_checkpoints()
   └─ Handle false return (reject block)

5. src/p2p/p2p_peer.cpp
   ├─ In verify_peer(): ia_detect_checkpoint_fork()
   └─ If true, mark peer untrusted

6. src/ai/ai_module.cpp
   └─ In monitor_loop(): checkpoint health checks


FUNCTION CALLS TO MAKE:

Initialization:
  └─ ia_checkpoint_monitor_initialize()
  └─ ia_set_checkpoint_network("mainnet")

Registration (for each checkpoint):
  └─ ia_register_checkpoint(height, hash, difficulty, source)

Validation (for each block):
  └─ if (!ia_verify_block_against_checkpoints(height, hash)) reject;

Fork Detection (for each peer):
  └─ if (ia_detect_checkpoint_fork(peer.cp, &conflict)) untrust;

Status Reporting:
  └─ ia_print_checkpoint_status()
  └─ ia_print_checkpoint_analysis()


TESTING:

1. Daemon starts without errors
2. See checkpoint status in logs
3. Blocks validated correctly
4. Fork detection works
5. Monitoring loop reports status


DEBUGGING:

Use ia_print_checkpoint_details() to see all checkpoint data
Use ia_get_checkpoint_count() to verify count
Use ia_get_latest_checkpoint_height() to check range
Enable MDEBUG to see every operation
"""

if __name__ == "__main__":
    print(quick_implementation_guide)
    print("\n" + "="*80)
    print("For detailed reference, see:")
    print("  - IA_CHECKPOINT_INTEGRATION.md (integration guide)")
    print("  - IA_CHECKPOINT_INTEGRATION_EXAMPLES.hpp (code examples)")
    print("  - CHECKPOINTS_IA_COMPLETE_UNDERSTANDING.md (conceptual guide)")
    print("  - CHECKPOINTS_IA_RESUMEN_COMPLETO.md (complete summary)")
