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

#include "hardforks.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "blockchain.hardforks"

// ===== NINACATCOIN HARD FORK SCHEDULE =====
// v1: Genesis (CryptoNight)
// v2-v16: All features activated at block 3000
//   v2:  Decomposed outputs, min mixin 2
//   v4:  Dynamic fee, RingCT allowed, min mixin 4
//   v6:  RingCT enforced, min mixin 6
//   v7:  CryptoNight v1
//   v8:  Bulletproofs, per-byte fee
//   v10: Bulletproof2, long-term block weight, min mixin 10
//   v12: RandomX PoW, fixed mixin, coinbase v2
//   v13: CLSAG, exact coinbase
//   v15: Bulletproof+, View Tags, 2021 scaling, min mixin 15
//   v16: Only Bulletproof+ allowed

const hardfork_t mainnet_hard_forks[] = {
  // version, height, threshold, time
  // v1: Genesis — CryptoNight PoW
  { 1,  1,    0, 1706313600 },
  // v16: All features activated — RandomX PoW, RingCT, CLSAG, Bulletproof+, View Tags
  // Jumping directly from v1 to v16 is safe: all HF checks use >= comparisons,
  // so v16 satisfies all intermediate version requirements (v2-v15).
  // add_fork() requires strictly increasing (version, height, time).
  { 16, 3000, 0, 1706400000 },
};
const size_t num_mainnet_hard_forks = sizeof(mainnet_hard_forks) / sizeof(mainnet_hard_forks[0]);
const uint64_t mainnet_hard_fork_version_1_till = 2999;

const hardfork_t testnet_hard_forks[] = {
  { 1,  1,    0, 1706313600 },
  { 16, 3000, 0, 1706400000 },
};
const size_t num_testnet_hard_forks = sizeof(testnet_hard_forks) / sizeof(testnet_hard_forks[0]);
const uint64_t testnet_hard_fork_version_1_till = 2999;

const hardfork_t stagenet_hard_forks[] = {
  { 1,  1,    0, 1706313600 },
  { 16, 3000, 0, 1706400000 },
};
const size_t num_stagenet_hard_forks = sizeof(stagenet_hard_forks) / sizeof(stagenet_hard_forks[0]);
