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

#include "include_base_utils.h"
using namespace epee;

#include "cryptonote_basic_impl.h"
#include "string_tools.h"
#include "serialization/binary_utils.h"
#include "cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "misc_language.h"
#include "common/base58.h"

#include <array>
#include <cstring>
#include <functional>
#include "crypto/hash.h"
#include "int-util.h"
#include "common/dns_utils.h"
#include <cstring>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "cn"

namespace cryptonote {

  struct integrated_address {
    account_public_address adr;
    crypto::hash8 payment_id;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(adr)
      FIELD(payment_id)
    END_SERIALIZE()

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(adr)
      KV_SERIALIZE(payment_id)
    END_KV_SERIALIZE_MAP()
  };

  /************************************************************************/
  /* Cryptonote helper functions                                          */
  /************************************************************************/
  //-----------------------------------------------------------------------------------------------
  size_t get_min_block_weight(uint8_t version)
  {
    if (version < 2)
      return CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
    if (version < 5)
      return CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2;
    return CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5;
  }
  //-----------------------------------------------------------------------------------------------
  size_t get_max_tx_size()
  {
    return CRYPTONOTE_MAX_TX_SIZE;
  }
  //-----------------------------------------------------------------------------------------------
  bool get_block_reward(
    size_t median_weight,
    size_t current_block_weight,
    uint64_t already_generated_coins,
    uint64_t &reward,
    uint8_t version,
    size_t height,
    const crypto::hash *prev_block_hash,
    bool events_frozen
) {
    if (height == 0)
    {
        reward = GENESIS_REWARD;
        return true;
    }

    uint64_t halvings = height / ninacatcoin_HALVING_INTERVAL_BLOCKS;
    
    // ===== VALIDACIÓN DE MÁXIMO HALVING =====
    // Limita halvings a MAX_HALVINGS (3 = hasta 2 NINA)
    if (halvings > ninacatcoin_MAX_HALVINGS)
      halvings = ninacatcoin_MAX_HALVINGS;

    // Calcula dinámicamente el máximo de halvings basado en MIN_BLOCK_REWARD
    // Evita que los bit shifts produzcan valores menores al mínimo
    uint64_t max_halvings_by_min = 0;
    while (max_halvings_by_min < 1 &&
           (BASE_BLOCK_REWARD >> max_halvings_by_min) > ninacatcoin_MIN_BLOCK_REWARD)
    {
      ++max_halvings_by_min;
    }
    if (halvings > max_halvings_by_min)
      halvings = max_halvings_by_min;

    // Recompensa base con halving
    uint64_t base_reward = BASE_BLOCK_REWARD >> halvings;

    // ===== NINA FIXED MINIMUM BLOCK REWARD (2 NINA) =====
    if (base_reward < ninacatcoin_MIN_BLOCK_REWARD)
    {
        base_reward = ninacatcoin_MIN_BLOCK_REWARD;
    }

    // Supply restante
    uint64_t remaining = 0;
    if (already_generated_coins < MONEY_SUPPLY)
        remaining = MONEY_SUPPLY - already_generated_coins;
    else
    {
        reward = 0;
        return true;
    }

    // ===== FRENO SUAVE =====
    const bool events_allowed = (remaining > ninacatcoin_FINAL_BRAKE_REMAINING);

    // =====================================================================
    // UNIFIED EVENT SYSTEM (X2 / X200)
    //
    // Legacy  (height < V2_HEIGHT): prev_block_hash = single prev hash
    //   → backward compatible, predictable 1 block ahead
    //
    // V2 (height >= V2_HEIGHT): prev_block_hash = multi-hash seed
    //   → caller combines last MULTI_HASH_DEPTH (10) block hashes via
    //     cn_fast_hash(h_{N-1} || h_{N-2} || ... || h_{N-10})
    //   → anti-manipulation: attacker must control 10+ consecutive blocks
    //   → events_frozen: spike protection suppresses events during abuse
    //
    // In both modes the miner gets the full prize directly:
    //   X2  →  base_reward × 2   (e.g. 4 NINA → 8 NINA)
    //   X200 → base_reward × 200 (e.g. 4 NINA → 800 NINA)
    // =====================================================================

    // Determine if events can fire
    bool can_fire_events = events_allowed;
    if (height >= NINA_EVENT_V2_HEIGHT && events_frozen)
        can_fire_events = false;

    auto get_event_roll = [&](uint8_t tag, uint64_t &out) {
        if (prev_block_hash)
        {
            std::array<uint8_t, sizeof(crypto::hash) + 1> buf{};
            std::memcpy(buf.data(), prev_block_hash, sizeof(crypto::hash));
            buf[sizeof(crypto::hash)] = tag;
            const crypto::hash h = crypto::cn_fast_hash(buf.data(), buf.size());
            std::memcpy(&out, &h, sizeof(uint64_t));
        }
        else
        {
            const crypto::hash h = crypto::cn_fast_hash(&height, sizeof(height));
            std::memcpy(&out, &h, sizeof(uint64_t));
        }
    };

    // ===== X2 =====
    if (can_fire_events && height >= 100)
    {
        uint64_t reward_x2;
        bool should_apply_x2 = false;

        if (height == 100)
        {
            should_apply_x2 = (base_reward <= std::numeric_limits<uint64_t>::max() / 2);
        }
        else
        {
            uint64_t rnd2;
            get_event_roll(2, rnd2);
            if ((rnd2 % BLOCKS_PER_YEAR) < X2_TIMES_PER_YEAR)
                should_apply_x2 = (base_reward <= std::numeric_limits<uint64_t>::max() / 2);
        }

        if (should_apply_x2)
        {
            reward_x2 = base_reward * 2;
            if (already_generated_coins + reward_x2 <= MONEY_SUPPLY)
                base_reward = reward_x2;
        }
    }

    // ===== X200 =====
    if (can_fire_events)
    {
        uint64_t rnd200;
        get_event_roll(200, rnd200);
        if ((rnd200 % BLOCKS_PER_YEAR) < X200_TIMES_PER_YEAR)
        {
            if (base_reward <= std::numeric_limits<uint64_t>::max() / 200)
            {
                uint64_t reward_x200 = base_reward * 200;
                if (already_generated_coins + reward_x200 <= MONEY_SUPPLY)
                    base_reward = reward_x200;
            }
        }
    }

    // ===== FRENO DURO =====
    // Protección: Si no hay supply restante o es negativo, pagar 0
    if (remaining <= 0) {
        reward = 0;
        return true;
    }

    // Ajustar base_reward si excede el supply restante
    // PERO: Garantizar que siempre paguemos al menos la recompensa mínima si hay supply
    if (base_reward > remaining) {
        // Si remaining < mínimo, aún no pagar 0: pagar lo que queda
        // (La red continuará hasta que no haya nada)
        if (remaining >= ninacatcoin_MIN_BLOCK_REWARD) {
            // Hay suficiente para pagar la recompensa ajustada
            base_reward = remaining;
        } else if (remaining > 0) {
            // Hay algo pero menos que el mínimo: pagar lo que queda
            // Esto permite que la red continúe con pagos micros en los últimos bloques
            base_reward = remaining;
        } else {
            // No hay nada: pagar 0
            reward = 0;
            return true;
        }
    }

    reward = base_reward;
    return true;
  }

  //------------------------------------------------------------------------------------
  uint8_t get_account_address_checksum(const public_address_outer_blob& bl)
  {
    const unsigned char* pbuf = reinterpret_cast<const unsigned char*>(&bl);
    uint8_t summ = 0;
    for(size_t i = 0; i!= sizeof(public_address_outer_blob)-1; i++)
      summ += pbuf[i];

    return summ;
  }
  //------------------------------------------------------------------------------------
  uint8_t get_account_integrated_address_checksum(const public_integrated_address_outer_blob& bl)
  {
    const unsigned char* pbuf = reinterpret_cast<const unsigned char*>(&bl);
    uint8_t summ = 0;
    for(size_t i = 0; i!= sizeof(public_integrated_address_outer_blob)-1; i++)
      summ += pbuf[i];

    return summ;
  }
  //-----------------------------------------------------------------------
  std::string get_account_address_as_str(
      network_type nettype
    , bool subaddress
    , account_public_address const & adr
    )
  {
    uint64_t address_prefix = subaddress ? get_config(nettype).CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX : get_config(nettype).CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;

    return tools::base58::encode_addr(address_prefix, t_serializable_object_to_blob(adr));
  }
  //-----------------------------------------------------------------------
  std::string get_account_integrated_address_as_str(
      network_type nettype
    , account_public_address const & adr
    , crypto::hash8 const & payment_id
    )
  {
    uint64_t integrated_address_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;

    integrated_address iadr = {
      adr, payment_id
    };
    return tools::base58::encode_addr(integrated_address_prefix, t_serializable_object_to_blob(iadr));
  }
  //-----------------------------------------------------------------------
  bool is_coinbase(const transaction_prefix& tx)
  {
    if(tx.vin.size() != 1)
      return false;

    if(tx.vin[0].type() != typeid(txin_gen))
      return false;

    return true;
  }
  //-----------------------------------------------------------------------
  bool get_account_address_from_str(
      address_parse_info& info
    , network_type nettype
    , std::string const & str
    )
  {
    uint64_t address_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    uint64_t integrated_address_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
    uint64_t subaddress_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;

    if (2 * sizeof(public_address_outer_blob) != str.size())
    {
      blobdata data;
      uint64_t prefix;
      if (!tools::base58::decode_addr(str, prefix, data))
      {
        LOG_PRINT_L2("Invalid address format");
        return false;
      }

      if (integrated_address_prefix == prefix)
      {
        info.is_subaddress = false;
        info.has_payment_id = true;
      }
      else if (address_prefix == prefix)
      {
        info.is_subaddress = false;
        info.has_payment_id = false;
      }
      else if (subaddress_prefix == prefix)
      {
        info.is_subaddress = true;
        info.has_payment_id = false;
      }
      else {
        LOG_PRINT_L1("Wrong address prefix: " << prefix << ", expected " << address_prefix 
          << " or " << integrated_address_prefix
          << " or " << subaddress_prefix);
        return false;
      }

      if (info.has_payment_id)
      {
        integrated_address iadr;
        if (!::serialization::parse_binary(data, iadr))
        {
          LOG_PRINT_L1("Account public address keys can't be parsed");
          return false;
        }
        info.address = iadr.adr;
        info.payment_id = iadr.payment_id;
      }
      else
      {
        if (!::serialization::parse_binary(data, info.address))
        {
          LOG_PRINT_L1("Account public address keys can't be parsed");
          return false;
        }
      }

      if (!crypto::check_key(info.address.m_spend_public_key) || !crypto::check_key(info.address.m_view_public_key))
      {
        LOG_PRINT_L1("Failed to validate address keys");
        return false;
      }
    }
    else
    {
      // Old address format
      std::string buff;
      if(!string_tools::parse_hexstr_to_binbuff(str, buff))
        return false;

      if(buff.size()!=sizeof(public_address_outer_blob))
      {
        LOG_PRINT_L1("Wrong public address size: " << buff.size() << ", expected size: " << sizeof(public_address_outer_blob));
        return false;
      }

      public_address_outer_blob blob = *reinterpret_cast<const public_address_outer_blob*>(buff.data());


      if(blob.m_ver > CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER)
      {
        LOG_PRINT_L1("Unknown version of public address: " << blob.m_ver << ", expected " << CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER);
        return false;
      }

      if(blob.check_sum != get_account_address_checksum(blob))
      {
        LOG_PRINT_L1("Wrong public address checksum");
        return false;
      }

      //we success
      info.address = blob.m_address;
      info.is_subaddress = false;
      info.has_payment_id = false;
    }

    return true;
  }
  //--------------------------------------------------------------------------------
  bool get_account_address_from_str_or_url(
      address_parse_info& info
    , network_type nettype
    , const std::string& str_or_url
    , std::function<std::string(const std::string&, const std::vector<std::string>&, bool)> dns_confirm
    )
  {
    if (get_account_address_from_str(info, nettype, str_or_url))
      return true;
    bool dnssec_valid;
    std::string address_str = tools::dns_utils::get_account_address_as_str_from_url(str_or_url, dnssec_valid, dns_confirm);
    return !address_str.empty() &&
      get_account_address_from_str(info, nettype, address_str);
  }
  //--------------------------------------------------------------------------------
  bool operator ==(const cryptonote::transaction& a, const cryptonote::transaction& b) {
    return cryptonote::get_transaction_hash(a) == cryptonote::get_transaction_hash(b);
  }

  bool operator ==(const cryptonote::block& a, const cryptonote::block& b) {
    return cryptonote::get_block_hash(a) == cryptonote::get_block_hash(b);
  }
  //--------------------------------------------------------------------------------
  int compare_hash32_reversed_nbits(const crypto::hash& ha, const crypto::hash& hb, unsigned int nbits)
  {
    static_assert(sizeof(uint64_t) * 4 == sizeof(crypto::hash), "hash is wrong size");

    // We have to copy these buffers b/c of the strict aliasing rule
    uint64_t va[4];
    memcpy(va, &ha, sizeof(crypto::hash));
    uint64_t vb[4];
    memcpy(vb, &hb, sizeof(crypto::hash));

    for (int n = 3; n >= 0 && nbits; --n)
    {
      const unsigned int msb_nbits = std::min<unsigned int>(64, nbits);
      const uint64_t lsb_nbits_dropped = static_cast<uint64_t>(64 - msb_nbits);
      const uint64_t van = SWAP64LE(va[n]) >> lsb_nbits_dropped;
      const uint64_t vbn = SWAP64LE(vb[n]) >> lsb_nbits_dropped;
      nbits -= msb_nbits;

      if (van < vbn) return -1; else if (van > vbn) return 1;
    }

    return 0;
  }

  crypto::hash make_hash32_loose_template(unsigned int nbits, const crypto::hash& h)
  {
    static_assert(sizeof(uint64_t) * 4 == sizeof(crypto::hash), "hash is wrong size");

    // We have to copy this buffer b/c of the strict aliasing rule
    uint64_t vh[4];
    memcpy(vh, &h, sizeof(crypto::hash));

    for (int n = 3; n >= 0; --n)
    {
      const unsigned int msb_nbits = std::min<unsigned int>(64, nbits);
      const uint64_t mask = msb_nbits ? (~((std::uint64_t(1) << (64 - msb_nbits)) - 1)) : 0;
      nbits -= msb_nbits;

      vh[n] &= SWAP64LE(mask);
    }

    crypto::hash res;
    memcpy(&res, vh, sizeof(crypto::hash));
    return res;
  }
  //--------------------------------------------------------------------------------
}

//--------------------------------------------------------------------------------
bool parse_hash256(const std::string &str_hash, crypto::hash& hash)
{
  std::string buf;
  bool res = epee::string_tools::parse_hexstr_to_binbuff(str_hash, buf);
  if (!res || buf.size() != sizeof(crypto::hash))
  {
    MERROR("invalid hash format: " << str_hash);
    return false;
  }
  else
  {
    buf.copy(reinterpret_cast<char *>(&hash), sizeof(crypto::hash));
    return true;
  }
}
