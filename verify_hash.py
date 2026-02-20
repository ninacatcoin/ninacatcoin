#!/usr/bin/env python3
import hashlib
import os

ai_dir = r"I:\ninacatcoin\src\ai"
files = [
    "ai_checkpoint_monitor.cpp",
    "ai_checkpoint_monitor.hpp",
    "ai_checkpoint_validator.cpp",
    "ai_checkpoint_validator.hpp",
    "ai_config.hpp",
    "ai_forced_remediation.cpp",
    "ai_forced_remediation.hpp",
    "ai_integrity_verifier.cpp",
    "ai_integrity_verifier.hpp",
    "ai_lwma_learning.cpp",
    "ai_lwma_learning.hpp",
    "ai_module.cpp",
    "ai_module.hpp",
    "ai_network_sandbox.cpp",
    "ai_network_sandbox.hpp",
    "ai_quarantine_system.cpp",
    "ai_quarantine_system.hpp",
    "ai_sandbox.cpp",
    "ai_sandbox.hpp",
]

combined = b""
for f in files:
    path = os.path.join(ai_dir, f)
    data = open(path, "rb").read()
    combined += data.replace(b"\r", b"")  # strip CR like CMake does

print(f"Hash (no CR): {hashlib.sha256(combined).hexdigest()}")

combined2 = b""
for f in files:
    path = os.path.join(ai_dir, f)
    combined2 += open(path, "rb").read()  # raw

print(f"Hash (raw):   {hashlib.sha256(combined2).hexdigest()}")
