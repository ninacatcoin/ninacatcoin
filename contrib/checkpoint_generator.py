#!/usr/bin/env python3
"""
NinaCat Automatic Checkpoint Generator with Auto-Discard
Generates checkpoints automatically without manual maintenance.
Runs as a daemon/cron job to keep checkpoints up-to-date.
Automatically discards old checkpoints to keep hosting storage minimal (~100 MB).
Includes SFTP upload to hosting server.

Security Model:
- Genesis block (height 0) is ALWAYS preserved
- Checkpoints older than 15 days are automatically discarded
- New nodes entering at any time are protected by contemporary checkpoints
- Network remains secure even with only recent checkpoints
"""

import requests
import json
import time
import sys
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import paramiko
from paramiko import SSHClient, AutoAddPolicy

class CheckpointGenerator:
    """Generate checkpoints automatically from blockchain."""
    
    def __init__(self, daemon_rpc_url: str = "http://localhost:19081", 
                 checkpoint_interval: int = 30,
                 network_type: str = "MAINNET",
                 sftp_host: str = None,
                 sftp_user: str = None,
                 sftp_password: str = None,
                 sftp_remote_path: str = "/checkpoints/",
                 max_checkpoint_age_seconds: int = 1296000):
        """
        Initialize checkpoint generator.
        
        Args:
            daemon_rpc_url: RPC endpoint of ninacatcoin daemon
            checkpoint_interval: Generate checkpoint every N blocks (default: 30 blocks = ~1 hour)
            network_type: Network type (MAINNET, TESTNET, STAGENET)
            sftp_host: SFTP server hostname
            sftp_user: SFTP username
            sftp_password: SFTP password
            sftp_remote_path: Remote directory path on SFTP server
            max_checkpoint_age_seconds: Maximum age of checkpoints in seconds (default: 1296000 = 15 days)
        """
        self.daemon_rpc = daemon_rpc_url
        self.checkpoint_interval = checkpoint_interval
        self.network_type = network_type
        self.sftp_host = sftp_host
        self.sftp_user = sftp_user
        self.sftp_password = sftp_password
        self.sftp_remote_path = sftp_remote_path
        self.max_checkpoint_age_seconds = max_checkpoint_age_seconds
        self.checkpoints: Dict[str, List[Tuple[int, str]]] = {
            'TESTNET': [],
            'STAGENET': [],
            'MAINNET': []
        }
        
        # Determine output filename based on network
        if network_type == "TESTNET":
            self.output_filename = "checkpoints_testnet_updated.json"
        elif network_type == "STAGENET":
            self.output_filename = "checkpoints_stagenet.json"
        else:
            self.output_filename = "checkpoints.json"
    
    def get_blockchain_height(self) -> Optional[int]:
        """Get current blockchain height from daemon."""
        try:
            response = requests.post(
                f"{self.daemon_rpc}/json_rpc",
                json={"jsonrpc": "2.0", "id": "0", "method": "get_info"},
                timeout=5
            )
            return response.json()['result']['height']
        except Exception as e:
            print(f"ERROR: Failed to get blockchain height: {e}")
            return None
    
    def get_block_hash(self, height: int) -> Optional[str]:
        """Get block hash at given height."""
        try:
            response = requests.post(
                f"{self.daemon_rpc}/json_rpc",
                json={"jsonrpc": "2.0", "id": "0", "method": "get_block_header_by_height", 
                      "params": {"height": height}},
                timeout=5
            )
            return response.json()['result']['block_header']['hash']
        except Exception as e:
            print(f"ERROR: Failed to get block hash at height {height}: {e}")
            return None
    
    def generate_checkpoints(self, min_confirmations: int = 100) -> bool:
        """
        Generate checkpoints for finalized blocks.
        Only checkpoint blocks with sufficient confirmations.
        
        Args:
            min_confirmations: Minimum blocks after checkpoint before marking it
        
        Returns:
            True if successful
        """
        height = self.get_blockchain_height()
        if height is None:
            return False
        
        # Only generate checkpoints for finalized blocks
        finalized_height = max(0, height - min_confirmations)
        
        print(f"[{datetime.now().isoformat()}] Network: {self.network_type}, Height: {height}, Finalized: {finalized_height}")
        
        # Generate checkpoints at intervals
        checkpoint_heights = []
        
        # Always include genesis
        checkpoint_heights.append(0)
        
        # Add interval checkpoints up to finalized height
        for h in range(self.checkpoint_interval, finalized_height + 1, self.checkpoint_interval):
            checkpoint_heights.append(h)
        
        for height in checkpoint_heights:
            block_hash = self.get_block_hash(height)
            if block_hash:
                print(f"  Checkpoint {height}: {block_hash}")
                self.checkpoints[self.network_type].append((height, block_hash))
            else:
                print(f"  WARNING: Could not get hash for block {height}")
        
        return len(self.checkpoints[self.network_type]) > 0
    def export_to_json(self, output_file: str = None, apply_discard: bool = False) -> bool:
        """Export checkpoints to JSON format for load_checkpoints_from_json().
        
        Args:
            output_file: Output filename
            apply_discard: If True, only export recent checkpoints (for SFTP upload)
                          If False, export all checkpoints (for local history)
        """
        try:
            if output_file is None:
                output_file = self.output_filename
            
            checkpoints_to_export = self.checkpoints[self.network_type]
            
            # If apply_discard, filter to only recent checkpoints
            if apply_discard:
                current_height = self.get_blockchain_height()
                if current_height is not None:
                    blocks_per_second = 1.0 / 120.0
                    max_age_blocks = int(self.max_checkpoint_age_seconds * blocks_per_second)
                    oldest_height_to_keep = max(0, current_height - max_age_blocks)
                    
                    checkpoints_to_export = [
                        cp for cp in self.checkpoints[self.network_type]
                        if cp[0] == 0 or cp[0] >= oldest_height_to_keep
                    ]
                
            # Calculate epoch_id from maximum checkpoint height
            epoch_id = 0
            if checkpoints_to_export:
                max_height = max(cp[0] for cp in checkpoints_to_export)
                epoch_id = max_height // self.checkpoint_interval
            
            # Get current timestamp
            import time
            generated_at_ts = int(time.time())
            
            checkpoint_data = {
                "network": self.network_type.lower(),
                "checkpoint_interval": self.checkpoint_interval,
                "epoch_id": epoch_id,
                "generated_at_ts": generated_at_ts,
                "hashlines": []
            }
            
            for height, hash_val in checkpoints_to_export:
                checkpoint_data["hashlines"].append({
                    "height": height,
                    "hash": hash_val
                })
            
            with open(output_file, 'w') as f:
                json.dump(checkpoint_data, f, indent=2)
            
            export_type = "recent" if apply_discard else "full history"
            print(f"Exported {len(checkpoint_data['hashlines'])} checkpoints ({export_type}) to {output_file}")
            print(f"  Network: {checkpoint_data['network']}")
            print(f"  Epoch ID: {epoch_id}")
            print(f"  Generated at: {generated_at_ts}")
            return True
        except Exception as e:
            print(f"ERROR: Failed to export checkpoints to JSON: {e}")
            return False
    
    def discard_old_checkpoints(self) -> int:
        """
        Remove checkpoints older than max_checkpoint_age_seconds.
        ALWAYS preserves Genesis block (height 0).
        
        Returns:
            Number of checkpoints removed
        """
        current_height = self.get_blockchain_height()
        if current_height is None:
            return 0
        
        # Calculate oldest height to keep based on block time
        # Assuming 120 seconds per block (ninacatcoin target)
        blocks_per_second = 1.0 / 120.0
        max_age_blocks = int(self.max_checkpoint_age_seconds * blocks_per_second)
        oldest_height_to_keep = max(0, current_height - max_age_blocks)
        
        print(f"\n[CHECKPOINT RETENTION] Current height: {current_height}")
        print(f"[CHECKPOINT RETENTION] Max age: {self.max_checkpoint_age_seconds}s ({self.max_checkpoint_age_seconds/86400:.1f} days)")
        print(f"[CHECKPOINT RETENTION] Oldest height to keep: {oldest_height_to_keep}")
        
        original_count = len(self.checkpoints[self.network_type])
        
        # Filter: keep Genesis (0) + checkpoints within retention window
        filtered_checkpoints = [
            cp for cp in self.checkpoints[self.network_type]
            if cp[0] == 0 or cp[0] >= oldest_height_to_keep
        ]
        
        removed_count = original_count - len(filtered_checkpoints)
        
        if removed_count > 0:
            print(f"[CHECKPOINT RETENTION] Removed {removed_count} old checkpoints (kept {len(filtered_checkpoints)})")
            self.checkpoints[self.network_type] = filtered_checkpoints
        else:
            print(f"[CHECKPOINT RETENTION] No old checkpoints to remove (all {original_count} are recent)")
        
        return removed_count
    
    def upload_via_sftp(self, local_file: str = None) -> bool:
        """Upload checkpoint JSON file to hosting via SFTP.
        
        SFTP credentials are read from environment variables:
        - SFTP_HOST: SFTP server hostname
        - SFTP_PORT: SFTP port (optional, default: 22)
        - SFTP_USER: SFTP username
        - SFTP_PASSWORD: SFTP password
        - SFTP_PATH: Remote directory path (optional, default: /checkpoints/)
        
        Example:
            export SFTP_HOST=access-5019408946.webspace-host.com
            export SFTP_PORT=22
            export SFTP_USER=myuser
            export SFTP_PASSWORD=mypassword
            export SFTP_PATH=/checkpoints/
        """
        import os
        
        # Read from environment variables (not script parameters)
        sftp_host = os.environ.get('SFTP_HOST', self.sftp_host)
        sftp_port = int(os.environ.get('SFTP_PORT', 22))
        sftp_user = os.environ.get('SFTP_USER', self.sftp_user)
        sftp_password = os.environ.get('SFTP_PASSWORD', self.sftp_password)
        sftp_path = os.environ.get('SFTP_PATH', self.sftp_remote_path)
        
        if not sftp_host or not sftp_user or not sftp_password:
            print("WARNING: SFTP credentials not configured (use env vars), skipping upload")
            return False
        
        try:
            if local_file is None:
                local_file = self.output_filename
            
            print(f"Connecting to SFTP server {sftp_host}:{sftp_port}...")
            ssh = SSHClient()
            ssh.set_missing_host_key_policy(AutoAddPolicy())
            ssh.connect(sftp_host, port=sftp_port, username=sftp_user, password=sftp_password, timeout=10)
            
            sftp = ssh.open_sftp()
            
            # Upload file
            remote_file = sftp_path + Path(local_file).name
            print(f"Uploading {local_file} to {sftp_host}:{remote_file}...")
            sftp.put(local_file, remote_file)
            
            sftp.close()
            ssh.close()
            
            print(f"Successfully uploaded {local_file} to {remote_file}")
            return True
        except Exception as e:
            print(f"ERROR: Failed to upload via SFTP: {e}")
            return False
    
    def export_to_dns_format(self, output_file: str = "checkpoints.txt") -> bool:
        """Export checkpoints in DNS TXT record format."""
        try:
            with open(output_file, 'w') as f:
                for height, hash_val in self.checkpoints[self.network_type]:
                    # DNS format: height:hash
                    f.write(f"{height}:{hash_val}\n")
            
            print(f"Exported {len(self.checkpoints[self.network_type])} checkpoints to {output_file}")
            return True
        except Exception as e:
            print(f"ERROR: Failed to export checkpoints to DNS format: {e}")
            return False
    
    def print_cpp_format(self) -> None:
        """Print checkpoints in C++ code format for reference."""
        print(f"\n=== C++ Format (for {self.network_type}) ===")
        for height, hash_val in self.checkpoints[self.network_type]:
            print(f'ADD_CHECKPOINT2({height:6d}, "{hash_val}", "0x0");')
    
    def run_daemon(self, interval_seconds: int = 3600):
        """
        Run as daemon, generating checkpoints periodically.
        HYBRID MODE:
        - Keeps FULL history locally (for seed node backup)
        - Uploads ONLY recent checkpoints to SFTP hosting
        
        Security: Genesis block is ALWAYS preserved, even with auto-discard on SFTP.
        
        Args:
            interval_seconds: Generate checkpoints every N seconds (default: 3600 = 1 hour)
        """
        print(f"\n{'='*70}")
        print(f"NinaCat Checkpoint Generator Daemon - HYBRID MODE")
        print(f"{'='*70}")
        print(f"Network: {self.network_type}")
        print(f"Daemon RPC: {self.daemon_rpc}")
        print(f"Generation interval: {interval_seconds}s ({interval_seconds/3600:.1f} hours)")
        print(f"Checkpoint interval: {self.checkpoint_interval} blocks")
        print(f"")
        print(f"LOCAL STORAGE (seed node backup):")
        print(f"  File: checkpoints_full_history_{self.network_type.lower()}.json")
        print(f"  Content: ALL checkpoints (complete history)")
        print(f"  Purpose: Seed node backup, analysis, debugging")
        print(f"")
        print(f"SFTP HOSTING (distributed to nodes):")
        print(f"  File: {self.output_filename}")
        print(f"  Content: ONLY recent checkpoints ({self.max_checkpoint_age_seconds/86400:.1f} days)")
        print(f"  Purpose: Lightweight download for new nodes")
        print(f"  Genesis block (height 0): ALWAYS PRESERVED ✓")
        print(f"{'='*70}\n")
        
        generation_count = 0
        
        while True:
            try:
                generation_count += 1
                print(f"\n[Generation #{generation_count}] {datetime.now().isoformat()}")
                self.checkpoints[self.network_type] = []
                
                if self.generate_checkpoints():
                    # EXPORT 1: Full history locally (NO discard)
                    full_history_file = f"checkpoints_full_history_{self.network_type.lower()}.json"
                    self.export_to_json(full_history_file, apply_discard=False)
                    
                    # EXPORT 2: Recent only for SFTP (WITH discard)
                    print(f"\n[AUTO-CHECKPOINT DISCARD FOR SFTP]")
                    sftp_file = self.output_filename
                    self.export_to_json(sftp_file, apply_discard=True)
                    
                    # Show discard statistics
                    current_height = self.get_blockchain_height()
                    if current_height is not None:
                        blocks_per_second = 1.0 / 120.0
                        max_age_blocks = int(self.max_checkpoint_age_seconds * blocks_per_second)
                        oldest_height_to_keep = max(0, current_height - max_age_blocks)
                        print(f"[AUTO-CHECKPOINT] Current height: {current_height}")
                        print(f"[AUTO-CHECKPOINT] Retention window: {self.max_checkpoint_age_seconds}s ({self.max_checkpoint_age_seconds/86400:.1f} days)")
                        print(f"[AUTO-CHECKPOINT] Oldest height to keep: {oldest_height_to_keep}")
                        print(f"[AUTO-CHECKPOINT] Genesis block (height 0): ALWAYS PRESERVED ✓")
                    
                    self.print_cpp_format()
                    
                    # Upload to SFTP (only recent version)
                    self.upload_via_sftp(sftp_file)
                else:
                    print("WARNING: Failed to generate checkpoints")
                
                print(f"\n→ Next update in {interval_seconds}s ({interval_seconds/3600:.1f} hours)")
                time.sleep(interval_seconds)
            
            except KeyboardInterrupt:
                print("\n\nShutdown signal received")
                print(f"Generated {generation_count} checkpoint updates before shutdown")
                print(f"Local full history preserved in: checkpoints_full_history_{self.network_type.lower()}.json")
                sys.exit(0)
            except Exception as e:
                print(f"ERROR in daemon loop: {e}")
                print(f"Retrying in {interval_seconds}s...")
                time.sleep(interval_seconds)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description="NinaCat Automatic Checkpoint Generator with SFTP upload"
    )
    parser.add_argument(
        "--daemon-rpc",
        default="http://localhost:19081",
        help="Daemon RPC endpoint (default: http://localhost:19081)"
    )
    parser.add_argument(
        "--interval",
        type=int,
        default=1000,
        help="Checkpoint interval in blocks (default: 1000)"
    )
    parser.add_argument(
        "--network",
        default="MAINNET",
        choices=["MAINNET", "TESTNET", "STAGENET"],
        help="Network type (default: MAINNET)"
    )
    parser.add_argument(
        "--output-json",
        help="Output JSON file (auto-determined by network if not specified)"
    )
    parser.add_argument(
        "--daemon-mode",
        action="store_true",
        help="Run as daemon (continuous)"
    )
    parser.add_argument(
        "--daemon-interval",
        type=int,
        default=3600,
        help="Daemon update interval in seconds (default: 3600 = 1 hour)"
    )
    parser.add_argument(
        "--confirmations",
        type=int,
        default=100,
        help="Minimum confirmations before marking checkpoint (default: 100)"
    )
    parser.add_argument(
        "--max-checkpoint-age",
        type=int,
        default=2592000,
        help="Maximum age of checkpoints in seconds (default: 2592000 = 30 days). Set to 0 to keep all checkpoints."
    )
    parser.add_argument(
        "--sftp-host",
        help="SFTP server hostname (can also use SFTP_HOST env var)"
    )
    parser.add_argument(
        "--sftp-user",
        help="SFTP username (can also use SFTP_USER env var)"
    )
    parser.add_argument(
        "--sftp-password",
        help="SFTP password (can also use SFTP_PASSWORD env var)"
    )
    parser.add_argument(
        "--sftp-path",
        default="/checkpoints/",
        help="Remote path on SFTP server (can also use SFTP_PATH env var, default: /checkpoints/)"
    )
    
    args = parser.parse_args()
    
    gen = CheckpointGenerator(
        daemon_rpc_url=args.daemon_rpc,
        checkpoint_interval=args.interval,
        network_type=args.network,
        sftp_host=args.sftp_host,
        sftp_user=args.sftp_user,
        sftp_password=args.sftp_password,
        sftp_remote_path=args.sftp_path,
        max_checkpoint_age_seconds=args.max_checkpoint_age
    )
    
    if args.daemon_mode:
        gen.run_daemon(interval_seconds=args.daemon_interval)
    else:
        # One-time generation
        if gen.generate_checkpoints(min_confirmations=args.confirmations):
            # Export full history locally
            full_history_file = f"checkpoints_full_history_{args.network.lower()}.json"
            gen.export_to_json(full_history_file, apply_discard=False)
            
            # Export recent only for SFTP
            output_file = args.output_json if args.output_json else gen.output_filename
            gen.export_to_json(output_file, apply_discard=True)
            
            gen.print_cpp_format()
            # Try to upload if SFTP configured
            gen.upload_via_sftp(output_file)
        else:
            sys.exit(1)


if __name__ == "__main__":
    main()
