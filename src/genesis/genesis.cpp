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

#include <iostream>
#include <sstream>
#include <string>

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_config.h"
#include "serialization/string.h"
#include "string_tools.h"

namespace {
const char kDefaultGenesisMsg[] =
  "Ninacatcoin 25/05/2014 to 16/02/2019 GOODBYE  "
  "No pude protegerte entonces, pero ahora voy a crear algo que se defienda solo y sea justo. "
  "I couldn't protect you back then, but now I am creating something that can defend itself and be fair.";

const char *nettype_label(cryptonote::network_type nettype)
{
  switch (nettype)
  {
    case cryptonote::TESTNET: return "testnet";
    case cryptonote::STAGENET: return "stagenet";
    default: return "mainnet";
  }
}

void print_usage(const char *prog)
{
  std::cout
    << "Usage:\n"
    << "  " << prog << " [--testnet|--stagenet] [--message \"...\"] [--burn-address <Wk...>] [--hard-fork-version <n>]\n"
    << "  " << prog << " --nettype mainnet|testnet|stagenet [--message \"...\"] [--burn-address <Wk...>] [--hard-fork-version <n>]\n"
    << "\n"
    << "If --burn-address is omitted, a new burn address is generated and only its\n"
    << "public keys are printed (no private keys are output).\n";
}
} // namespace

int main(int argc, char **argv)
{
  std::string message = kDefaultGenesisMsg;
  std::string burn_address_str;
  cryptonote::network_type nettype = cryptonote::MAINNET;
  uint8_t hard_fork_version = CURRENT_BLOCK_MAJOR_VERSION;

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if ((arg == "-h") || (arg == "--help"))
    {
      print_usage(argv[0]);
      return 0;
    }
    if ((arg == "-m") || (arg == "--message"))
    {
      if (i + 1 >= argc)
      {
        std::cerr << "Missing value for --message\n";
        return 1;
      }
      message = argv[++i];
      continue;
    }
    if (arg == "--testnet")
    {
      nettype = cryptonote::TESTNET;
      continue;
    }
    if (arg == "--stagenet")
    {
      nettype = cryptonote::STAGENET;
      continue;
    }
    if (arg == "--nettype")
    {
      if (i + 1 >= argc)
      {
        std::cerr << "Missing value for --nettype\n";
        return 1;
      }
      const std::string value = argv[++i];
      if (value == "mainnet")
        nettype = cryptonote::MAINNET;
      else if (value == "testnet")
        nettype = cryptonote::TESTNET;
      else if (value == "stagenet")
        nettype = cryptonote::STAGENET;
      else
      {
        std::cerr << "Unknown nettype: " << value << "\n";
        return 1;
      }
      continue;
    }
    if (arg == "--hard-fork-version")
    {
      if (i + 1 >= argc)
      {
        std::cerr << "Missing value for --hard-fork-version\n";
        return 1;
      }
      const int value = std::stoi(argv[++i]);
      if (value <= 0 || value > 255)
      {
        std::cerr << "Invalid hard fork version: " << value << "\n";
        return 1;
      }
      hard_fork_version = static_cast<uint8_t>(value);
      continue;
    }
    if ((arg == "-a") || (arg == "--burn-address"))
    {
      if (i + 1 >= argc)
      {
        std::cerr << "Missing value for --burn-address\n";
        return 1;
      }
      burn_address_str = argv[++i];
      continue;
    }

    std::cerr << "Unknown argument: " << arg << "\n";
    print_usage(argv[0]);
    return 1;
  }

  cryptonote::account_public_address burn_address = {};
  if (!burn_address_str.empty())
  {
    cryptonote::address_parse_info info;
    if (!cryptonote::get_account_address_from_str(info, nettype, burn_address_str))
    {
      std::cerr << "Invalid burn address for " << nettype_label(nettype) << ".\n";
      return 1;
    }
    if (info.is_subaddress || info.has_payment_id)
    {
      std::cerr << "Burn address must be a standard " << nettype_label(nettype)
                << " address (no subaddress, no payment id).\n";
      return 1;
    }
    burn_address = info.address;
  }
  else
  {
    cryptonote::account_base acc;
    acc.generate();
    burn_address = acc.get_keys().m_account_address;
    burn_address_str = cryptonote::get_account_address_as_str(nettype, false, burn_address);
  }

  cryptonote::transaction tx;
  const cryptonote::blobdata extra_nonce;
  const bool tx_ok = cryptonote::construct_miner_tx(
    0,
    0,
    0,
    0,
    0,
    burn_address,
    tx,
    extra_nonce,
    1,
    hard_fork_version,
    nullptr);

  if (!tx_ok)
  {
    std::cerr << "Failed to construct genesis tx.\n";
    return 1;
  }

  {
    std::ostringstream oss;
    binary_archive<true> ar(oss);
    cryptonote::tx_extra_field field = cryptonote::tx_extra_nonce{message};
    if (!::do_serialize(ar, field))
    {
      std::cerr << "Failed to serialize genesis message.\n";
      return 1;
    }
    const std::string field_str = oss.str();
    tx.extra.insert(tx.extra.end(), field_str.begin(), field_str.end());
    if (!cryptonote::sort_tx_extra(tx.extra, tx.extra))
    {
      std::cerr << "Failed to sort tx extra.\n";
      return 1;
    }
    tx.invalidate_hashes();
  }

  const uint64_t reward = cryptonote::get_outs_money_amount(tx);
  if (reward != GENESIS_REWARD)
  {
    std::cerr << "Genesis reward mismatch: got " << reward
              << ", expected " << GENESIS_REWARD << "\n";
    return 1;
  }

  const std::string tx_blob = cryptonote::tx_to_blob(tx);
  const std::string tx_hex = epee::string_tools::buff_to_hex_nodelimer(tx_blob);

  cryptonote::block bl;
  if (!cryptonote::generate_genesis_block(bl, tx_hex, 0))
  {
    std::cerr << "Failed to generate genesis block.\n";
    return 1;
  }

  const crypto::hash gen_hash = cryptonote::get_block_hash(bl);

  std::cout << "BURN_ADDRESS: " << burn_address_str << "\n";
  std::cout << "BURN_SPEND_PUBKEY: " << burn_address.m_spend_public_key << "\n";
  std::cout << "BURN_VIEW_PUBKEY: " << burn_address.m_view_public_key << "\n";
  std::cout << "GENESIS_MESSAGE: " << message << "\n";
  std::cout << "GENESIS_TX: " << tx_hex << "\n";
  std::cout << "GENESIS_NONCE: " << bl.nonce << "\n";
  std::cout << "GENESIS_HASH: " << epee::string_tools::pod_to_hex(gen_hash) << "\n";
  std::cout << "GENESIS_REWARD: " << cryptonote::print_money(reward) << "\n";

  return 0;
}
