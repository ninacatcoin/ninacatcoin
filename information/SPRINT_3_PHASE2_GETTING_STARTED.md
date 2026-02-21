# Sprint 3 Phase 2: P2P Integration - Getting Started

**Date:** 25 January 2026  
**Phase:** 3.2 (After 3.1 Core Integration)  
**Duration:** ~2-3 days  
**Objective:** Connect consensus queries to P2P network  

---

## 📋 What We Have

✅ **Core integration complete:**
- `checkpoints.h` - 8 new methods (initiate_consensus_query, handle_*, activate_quarantine, etc)
- `checkpoints.cpp` - Full implementation of consensus logic
- `security_query_tool.cpp` - Query creation, response handling, consensus calculation
- `reputation_manager.cpp` - Peer reputation tracking and persistence

✅ **15 integration tests** ready in `checkpoints_phase2.cpp`

❌ **Missing:** P2P message transmission (net_node integration)

---

## 🔍 What Needs to Be Done

### Step 1: Explore net_node.cpp Structure (1 hour)

**Goal:** Understand how messages are sent/received in P2P network

**File to review:** `src/p2p/net_node.cpp` (likely 3000+ lines)

**Key things to find:**
1. How message types are defined (look for `NOTIFY_` patterns)
2. How handlers are registered (search for `on_notify_` functions)
3. How messages are serialized (look for `BEGIN_KV_SERIALIZE_MAP`)
4. How messages are sent to peers (search for `send_to_peer` or similar)

**Example to look for:**
```cpp
// Message type definition
struct NOTIFY_SOME_MESSAGE {
  std::string some_field;
  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(some_field)
  END_KV_SERIALIZE_MAP()
};

// Handler
int netNode::handle_notify_some_message(int version, const NOTIFY_SOME_MESSAGE &arg, ...) {
  // Process message
  return 1;  // success
}
```

**Commands to search:**
```bash
grep -n "NOTIFY_" src/p2p/net_node.h
grep -n "struct NOTIFY" src/p2p/net_node.h
grep -n "handle_notify" src/p2p/net_node.cpp
grep -n "BEGIN_KV_SERIALIZE_MAP" src/p2p/net_node.cpp
```

---

### Step 2: Define Security Message Types (30 minutes)

**Goal:** Create message types for security queries and responses

**File to modify:** `src/p2p/net_node.h`

**Add after existing NOTIFY structures:**

```cpp
namespace cryptonote {
namespace p2p {

  // ========== PHASE 2: P2P Consensus Messages ==========
  
  /**
   * @brief Security query to peers: "Is this checkpoint hash valid?"
   */
  struct NOTIFY_SECURITY_QUERY {
    uint64_t height;                    // Block height
    std::string suspect_hash;           // Hash to validate
    std::string query_id;               // Unique query ID
    std::string requesting_node_id;     // Who's asking
    uint64_t timestamp;                 // When query was created
    
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(height)
      KV_SERIALIZE(suspect_hash)
      KV_SERIALIZE(query_id)
      KV_SERIALIZE(requesting_node_id)
      KV_SERIALIZE(timestamp)
    END_KV_SERIALIZE_MAP()
  };

  /**
   * @brief Security response from peers: "Yes/No, this hash is valid"
   */
  struct NOTIFY_SECURITY_RESPONSE {
    std::string query_id;               // Which query this responds to
    uint64_t height;                    // Block height
    std::string responder_node_id;      // Who's responding
    bool matches_local;                 // Does our local hash match?
    uint64_t timestamp;                 // When we responded
    
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(query_id)
      KV_SERIALIZE(height)
      KV_SERIALIZE(responder_node_id)
      KV_SERIALIZE(matches_local)
      KV_SERIALIZE(timestamp)
    END_KV_SERIALIZE_MAP()
  };

} // namespace p2p
} // namespace cryptonote
```

**File location:** Around line `src/p2p/net_node.h` after other NOTIFY structures

---

### Step 3: Add Message Handlers (1 hour)

**Goal:** Create handlers that process these messages in checkpoints

**File to modify:** `src/p2p/net_node.cpp`

**Add new handler functions (find a good spot, likely near other handle_notify functions):**

```cpp
// ========== PHASE 2: Security Query Handlers ==========

int net_node::handle_notify_security_query(int version, 
                                          const NOTIFY_SECURITY_QUERY &arg, 
                                          cryptonote_connection_context &context) {
  // 1. Extract information
  uint64_t height = arg.height;
  std::string query_id = arg.query_id;
  std::string suspect_hash = arg.suspect_hash;
  
  // 2. Forward to checkpoints system
  cryptonote::SecurityQuery query;
  query.id = query_id;
  query.height = height;
  query.suspect_hash = suspect_hash;
  query.source_peer_id = context.peer_id;
  
  // 3. Let checkpoints handle it
  bool result = m_checkpoints->handle_security_query(query);
  
  MINFO("Processed NOTIFY_SECURITY_QUERY: height=" << height 
        << ", query_id=" << query_id << ", result=" << result);
  
  return result ? 1 : 0;
}

int net_node::handle_notify_security_response(int version,
                                             const NOTIFY_SECURITY_RESPONSE &arg,
                                             cryptonote_connection_context &context) {
  // 1. Extract information
  std::string query_id = arg.query_id;
  std::string responder = arg.responder_node_id;
  bool matches = arg.matches_local;
  
  // 2. Create response object
  cryptonote::SecurityResponse response;
  response.query_id = query_id;
  response.node_id = responder;
  response.height = arg.height;
  response.matches_local = matches;
  response.timestamp = arg.timestamp;
  
  // 3. Let checkpoints handle it
  bool result = m_checkpoints->handle_security_response(response);
  
  MINFO("Processed NOTIFY_SECURITY_RESPONSE: query_id=" << query_id
        << ", responder=" << responder << ", matches=" << matches);
  
  return result ? 1 : 0;
}
```

---

### Step 4: Register Handlers (30 minutes)

**Goal:** Tell net_node to call these handlers when messages arrive

**File to modify:** `src/p2p/net_node.cpp` (in constructor or message handler registration)

**Find where handlers are registered:**
```bash
grep -n "NOTIFY_" src/p2p/net_node.cpp | grep "register"
grep -n "m_protocol_handler_map" src/p2p/net_node.cpp
```

**Add handler registration (exact location depends on code structure):**

```cpp
// In the message handler map/registration section:
REGISTER_HANDLER(NOTIFY_SECURITY_QUERY, handle_notify_security_query);
REGISTER_HANDLER(NOTIFY_SECURITY_RESPONSE, handle_notify_security_response);

// Or similar, depending on the actual registration mechanism
```

---

### Step 5: Send Queries to Peers (1-2 hours)

**Goal:** Modify `checkpoints::initiate_consensus_query()` to actually send messages

**Current implementation (in checkpoints.cpp):**
```cpp
// Currently just waits and processes local state
std::this_thread::sleep_for(std::chrono::seconds(5));
```

**What needs to happen:**
```cpp
// 1. Get list of trusted peers from net_node
auto peers = m_net_node->get_peer_list();

// 2. For each trusted peer:
for (const auto& peer : peers) {
  // 3. Create NOTIFY_SECURITY_QUERY message
  cryptonote::NOTIFY_SECURITY_QUERY msg;
  msg.query_id = query.id;
  msg.height = query.height;
  msg.suspect_hash = hash_str;
  msg.requesting_node_id = get_my_node_id();
  msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ULL;
  
  // 4. Send to peer
  m_net_node->send_to_peer(peer, msg);
}

// 5. Wait for responses (30-60 seconds depending on network)
std::this_thread::sleep_for(std::chrono::seconds(30));

// 6. Calculate consensus
auto consensus = m_security_query_tool->calculate_consensus();
```

---

## 📝 Implementation Order

1. **Review net_node.cpp** (1 hour)
   - Understand message structure
   - Find handler registration pattern
   - Identify peer list/communication methods

2. **Add message types** (30 min)
   - NOTIFY_SECURITY_QUERY struct
   - NOTIFY_SECURITY_RESPONSE struct
   - Serialization maps

3. **Implement handlers** (1 hour)
   - handle_notify_security_query()
   - handle_notify_security_response()
   - Error handling and logging

4. **Register handlers** (30 min)
   - Find registration mechanism
   - Add message type registration
   - Test handler lookup

5. **Connect to transmission** (1-2 hours)
   - Modify initiate_consensus_query()
   - Send queries to selected peers
   - Gather responses
   - Calculate consensus

6. **Testing** (1-2 hours)
   - Compile with net_node changes
   - Run integration tests
   - Multi-node testing

---

## 🎯 Checkpoints for Success

✅ **After Step 1:**
- Understand net_node architecture
- Know where to add new message types

✅ **After Step 2:**
- NOTIFY_SECURITY_QUERY and RESPONSE defined
- Compiles without errors

✅ **After Step 3:**
- Handlers implemented
- Ready to receive messages

✅ **After Step 4:**
- Handlers registered
- System recognizes new message types

✅ **After Step 5:**
- Queries sent to peers
- Responses received and processed
- Consensus calculated

✅ **After Step 6:**
- Full P2P system operational
- All integration tests passing

---

## 💡 Tips

1. **Use grep to navigate:** `grep -rn "NOTIFY_" src/p2p/` to find patterns
2. **Follow existing code:** Use existing message types as templates
3. **Check headers first:** net_node.h has message structure; net_node.cpp has implementation
4. **Test incrementally:** Compile after each step
5. **Use logging:** Add MINFO statements to track message flow

---

## 🚀 Ready to Start?

When you're ready, begin with:

```bash
# 1. Explore net_node structure
grep -n "struct NOTIFY_" src/p2p/net_node.h | head -20
grep -n "handle_notify" src/p2p/net_node.cpp | head -10

# 2. Count existing handlers
grep -c "handle_notify" src/p2p/net_node.cpp

# 3. Find where to add new messages
grep -n "END_KV_SERIALIZE_MAP" src/p2p/net_node.h | tail -5
```

---

**Next session:** Execute these 5 steps to complete Sprint 3 Phase 2

**Estimated completion:** 26-28 January  
**Then:** Sprint 4 (Full E2E Testing)  
**Final target:** Phase 2 complete by 8 February  

---

**Current Status:** 🟢 Phase 1 READY → Phase 2 READY TO START
