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

#include <list>
#include "serialization/keyvalue_serialization.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/blobdatatype.h"

namespace cryptonote
{


#define BC_COMMANDS_POOL_BASE 2000

  /************************************************************************/
  /* P2P connection info, serializable to json                            */
  /************************************************************************/
  struct connection_info
  {
    bool incoming;
    bool localhost;
    bool local_ip;
    bool ssl;

    std::string address;
    std::string host;
    std::string ip;
    std::string port;
    uint16_t rpc_port;
    uint32_t rpc_credits_per_hash;

    std::string peer_id;

    uint64_t recv_count;
    uint64_t recv_idle_time;

    uint64_t send_count;
    uint64_t send_idle_time;

    std::string state;

    uint64_t live_time;

	uint64_t avg_download;
	uint64_t current_download;
	
	uint64_t avg_upload;
	uint64_t current_upload;
  
	uint32_t support_flags;

	std::string connection_id;

    uint64_t height;

    uint32_t pruning_seed;

    uint8_t address_type;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(incoming)
      KV_SERIALIZE(localhost)
      KV_SERIALIZE(local_ip)
      KV_SERIALIZE(address)
      KV_SERIALIZE(host)
      KV_SERIALIZE(ip)
      KV_SERIALIZE(port)
      KV_SERIALIZE(rpc_port)
      KV_SERIALIZE(rpc_credits_per_hash)
      KV_SERIALIZE(peer_id)
      KV_SERIALIZE(recv_count)
      KV_SERIALIZE(recv_idle_time)
      KV_SERIALIZE(send_count)
      KV_SERIALIZE(send_idle_time)
      KV_SERIALIZE(state)
      KV_SERIALIZE(live_time)
      KV_SERIALIZE(avg_download)
      KV_SERIALIZE(current_download)
      KV_SERIALIZE(avg_upload)
      KV_SERIALIZE(current_upload)
      KV_SERIALIZE(support_flags)
      KV_SERIALIZE(connection_id)
      KV_SERIALIZE(height)
      KV_SERIALIZE(pruning_seed)
      KV_SERIALIZE(address_type)
    END_KV_SERIALIZE_MAP()
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct tx_blob_entry
  {
    blobdata blob;
    crypto::hash prunable_hash;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(blob)
      KV_SERIALIZE_VAL_POD_AS_BLOB(prunable_hash)
    END_KV_SERIALIZE_MAP()

    tx_blob_entry(const blobdata &bd = {}, const crypto::hash &h = crypto::null_hash): blob(bd), prunable_hash(h) {}
  };
  struct block_complete_entry
  {
    bool pruned;
    blobdata block;
    uint64_t block_weight;
    std::vector<tx_blob_entry> txs;
    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE_OPT(pruned, false)
      KV_SERIALIZE(block)
      KV_SERIALIZE_OPT(block_weight, (uint64_t)0)
      if (this_ref.pruned)
      {
        KV_SERIALIZE(txs)
      }
      else
      {
        std::vector<blobdata> txs;
        if (is_store)
        {
          txs.reserve(this_ref.txs.size());
          for (const auto &e: this_ref.txs) txs.push_back(e.blob);
        }
        epee::serialization::selector<is_store>::serialize(txs, stg, hparent_section, "txs");
        if (!is_store)
        {
          block_complete_entry &self = const_cast<block_complete_entry&>(this_ref);
          self.txs.clear();
          self.txs.reserve(txs.size());
          for (auto &e: txs) self.txs.push_back({std::move(e), crypto::null_hash});
        }
      }
    END_KV_SERIALIZE_MAP()

    block_complete_entry(): pruned(false), block_weight(0) {}
  };


  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_NEW_BLOCK
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 1;

    struct request_t
    {
      block_complete_entry b;
      uint64_t current_blockchain_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(b)
        KV_SERIALIZE(current_blockchain_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_NEW_TRANSACTIONS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 2;

    struct request_t
    {
      std::vector<blobdata>   txs;
      std::string _; // padding
      bool dandelionpp_fluff; //zero initialization defaults to stem mode

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs)
        KV_SERIALIZE(_)
        KV_SERIALIZE_OPT(dandelionpp_fluff, true) // backwards compatible mode is fluff
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_REQUEST_GET_OBJECTS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 3;

    struct request_t
    {
      std::vector<crypto::hash> blocks;
      bool prune;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(blocks)
        KV_SERIALIZE_OPT(prune, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };

  struct NOTIFY_RESPONSE_GET_OBJECTS
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 4;

    struct request_t
    {
      std::vector<block_complete_entry>  blocks;
      std::vector<crypto::hash>          missed_ids;
      uint64_t                         current_blockchain_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blocks)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(missed_ids)
        KV_SERIALIZE(current_blockchain_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };


  struct CORE_SYNC_DATA
  {
    uint64_t current_height;
    uint64_t cumulative_difficulty;
    uint64_t cumulative_difficulty_top64;
    crypto::hash  top_id;
    uint8_t top_version;
    uint32_t pruning_seed;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(current_height)
      KV_SERIALIZE(cumulative_difficulty)
      if (is_store)
        KV_SERIALIZE(cumulative_difficulty_top64)
      else
        KV_SERIALIZE_OPT(cumulative_difficulty_top64, (uint64_t)0)
      KV_SERIALIZE_VAL_POD_AS_BLOB(top_id)
      KV_SERIALIZE_OPT(top_version, (uint8_t)0)
      KV_SERIALIZE_OPT(pruning_seed, (uint32_t)0)
    END_KV_SERIALIZE_MAP()
  };

  struct NOTIFY_REQUEST_CHAIN
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 6;

    struct request_t
    {
      std::list<crypto::hash> block_ids; /*IDs of the first 10 blocks are sequential, next goes with pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */
      bool prune;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
        KV_SERIALIZE_OPT(prune, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };

  struct NOTIFY_RESPONSE_CHAIN_ENTRY
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 7;

    struct request_t
    {
      uint64_t start_height;
      uint64_t total_height;
      uint64_t cumulative_difficulty;
      uint64_t cumulative_difficulty_top64;
      std::vector<crypto::hash> m_block_ids;
      std::vector<uint64_t> m_block_weights;
      cryptonote::blobdata first_block;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(total_height)
        KV_SERIALIZE(cumulative_difficulty)
        if (is_store)
          KV_SERIALIZE(cumulative_difficulty_top64)
        else
          KV_SERIALIZE_OPT(cumulative_difficulty_top64, (uint64_t)0)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(m_block_ids)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(m_block_weights)
        KV_SERIALIZE(first_block)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };
  
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_NEW_FLUFFY_BLOCK
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 8;

    struct request_t
    {
      block_complete_entry b;
      uint64_t current_blockchain_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(b)
        KV_SERIALIZE(current_blockchain_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };  

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_REQUEST_FLUFFY_MISSING_TX
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 9;

    struct request_t
    {
      crypto::hash block_hash;
      uint64_t current_blockchain_height;      
      std::vector<uint64_t> missing_tx_indices;
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(block_hash)
        KV_SERIALIZE(current_blockchain_height)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(missing_tx_indices)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  }; 

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct NOTIFY_GET_TXPOOL_COMPLEMENT
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 10;

    struct request_t
    {
      std::vector<crypto::hash> hashes;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(hashes)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };

  /************************************************************************/
  /* NINA AI Intelligence Sharing between nodes                          */
  /************************************************************************/
  struct NOTIFY_NINA_INTELLIGENCE
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 11;

    struct nina_intel_entry_t
    {
      std::string intel_type;       // "ATTACK_PATTERN", "ANOMALY", "PEER_REPUTATION", "THREAT_SIGNATURE"
      uint64_t    height;           // Block height where detected
      uint64_t    timestamp;        // When detected (unix epoch)
      std::string pattern_id;       // Unique identifier for dedup
      std::string data;             // Serialized details (pipe-delimited)
      double      confidence;       // 0.0 - 1.0 how confident
      std::string threat_level;     // "SAFE", "SUSPICIOUS", "DANGEROUS"
      uint8_t     hops;             // TTL: decremented each relay, drop at 0

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(intel_type)
        KV_SERIALIZE(height)
        KV_SERIALIZE(timestamp)
        KV_SERIALIZE(pattern_id)
        KV_SERIALIZE(data)
        KV_SERIALIZE(confidence)
        KV_SERIALIZE(threat_level)
        KV_SERIALIZE(hops)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<nina_intel_entry_t> nina_intel_entry;

    struct request_t
    {
      std::vector<nina_intel_entry_t> entries;
      uint64_t sender_height;       // Sender's current blockchain height

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(entries)
        KV_SERIALIZE(sender_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };

  /************************************************************************/
  /* NINA AI Model Sharing — distribute trained ML models between nodes  */
  /* Allows new nodes to receive pre-trained models immediately instead  */
  /* of waiting for 1000+ blocks to accumulate for local training.       */
  /* Models are serialized as base64-encoded blobs with version tracking */
  /************************************************************************/
  struct NOTIFY_NINA_MODEL_SHARE
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 12;

    struct nina_model_entry_t
    {
      std::string model_name;       // "phase1_anomaly_detector", "phase2_difficulty", etc.
      std::string model_version;    // SHA-256 of model bytes (for dedup + integrity)
      uint64_t    training_height;  // Blockchain height model was trained up to
      uint64_t    training_rows;    // Number of training samples used
      uint64_t    timestamp;        // When model was trained (unix epoch)
      double      accuracy;         // Reported model accuracy / R² score
      std::string model_data;       // Base64-encoded serialized model blob
      uint32_t    data_size;        // Original (unencoded) size in bytes
      uint8_t     hops;             // TTL: decremented each relay, drop at 0

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(model_name)
        KV_SERIALIZE(model_version)
        KV_SERIALIZE(training_height)
        KV_SERIALIZE(training_rows)
        KV_SERIALIZE(timestamp)
        KV_SERIALIZE(accuracy)
        KV_SERIALIZE(model_data)
        KV_SERIALIZE(data_size)
        KV_SERIALIZE(hops)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<nina_model_entry_t> nina_model_entry;

    struct request_t
    {
      std::vector<nina_model_entry_t> models;
      uint64_t sender_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(models)
        KV_SERIALIZE(sender_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };

  /************************************************************************/
  /* NINA State Sync — share NINA's data.mdb state between nodes         */
  /* Works like blockchain sync: nodes exchange their NINA learning       */
  /* state so new nodes can bootstrap NINA intelligence immediately      */
  /* instead of learning from scratch over 1000+ blocks.                 */
  /************************************************************************/
  struct NOTIFY_NINA_STATE_SYNC
  {
    const static int ID = BC_COMMANDS_POOL_BASE + 13;

    struct request_t
    {
      uint64_t    sender_height;       // Sender's blockchain height
      uint64_t    nina_last_height;    // Last height NINA processed
      uint64_t    nina_block_records;  // Number of block records in NINA DB
      uint64_t    nina_session_count;  // Number of sessions NINA has run
      uint64_t    timestamp;           // When this was generated
      std::string state_data;          // Serialized NINA state (export_nina_state_for_p2p)
      std::string state_hash;          // SHA-256 of state_data for integrity
      uint8_t     hops;                // TTL: decremented each relay

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(sender_height)
        KV_SERIALIZE(nina_last_height)
        KV_SERIALIZE(nina_block_records)
        KV_SERIALIZE(nina_session_count)
        KV_SERIALIZE(timestamp)
        KV_SERIALIZE(state_data)
        KV_SERIALIZE(state_hash)
        KV_SERIALIZE(hops)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
  };
    
}
