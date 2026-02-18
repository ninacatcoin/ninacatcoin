#!/usr/bin/env python3
"""
MONITOR_MINING.py - Monitoreo en tiempo real de minería Ninacatcoin
Moestra estadísticas actuales: hash rate, bloques, GPU penalty, etc.
"""

import json
import time
import requests
import sys
from datetime import datetime
from collections import deque

class NinacatcoinMonitor:
    def __init__(self, rpc_url="http://127.0.0.1:29084/json_rpc", update_interval=10):
        """
        Initialize mining monitor
        
        Args:
            rpc_url: RPC endpoint URL
            update_interval: Update interval in seconds
        """
        self.rpc_url = rpc_url
        self.update_interval = update_interval
        self.last_block_height = 0
        self.last_block_hash = None
        self.last_timestamp = 0
        
        # Historial para cálculo de hash rate (últimos 60 segundos)
        self.block_times = deque(maxlen=100)
        self.gpu_penalty_blocks = 0
        self.total_blocks = 0
        
    def rpc_call(self, method, params=None):
        """Realizar llamada RPC"""
        try:
            payload = {
                "jsonrpc": "2.0",
                "id": "0",
                "method": method
            }
            if params:
                payload["params"] = params
            
            response = requests.post(self.rpc_url, json=payload, timeout=5)
            response.raise_for_status()
            result = response.json()
            
            if "error" in result and result["error"]:
                print(f"❌ RPC Error: {result['error']}")
                return None
            
            return result.get("result", result)
        except requests.exceptions.RequestException as e:
            print(f"❌ RPC Connection error: {e}")
            return None
    
    def get_block_info(self, height):
        """Obtener información de un bloque"""
        result = self.rpc_call("get_block", {"height": height})
        return result
    
    def get_block_header(self, height):
        """Obtener header de un bloque"""
        result = self.rpc_call("get_block_header", {"height": height})
        return result
    
    def is_gpu_penalty_block(self, height):
        """
        Detectar si un bloque tiene GPU penalty
        GPU penalty se aplica cada 5 bloques (bloques 5, 10, 15, ...)
        """
        return (height % 5) == 0
    
    def get_stats(self):
        """Obtener estadísticas de minería"""
        stats = {
            "timestamp": datetime.now().isoformat(),
            "height": 0,
            "hash_rate_est": 0.0,
            "blocks_per_hour": 0.0,
            "gpu_penalty_blocks": 0,
            "total_blocks": 0,
            "network_difficulty": 0,
            "block_time_avg": 0,
        }
        
        # Obtener altura actual
        info = self.rpc_call("get_info")
        if not info:
            return None
        
        current_height = info.get("height", 0)
        stats["height"] = current_height
        
        # Si es la primera llamada
        if self.last_block_height == 0:
            self.last_block_height = current_height
            return stats
        
        # Calcular bloques encontrados
        blocks_found = current_height - self.last_block_height
        
        if blocks_found > 0:
            # Obtener timestamp del bloque anterior
            if self.last_block_height > 0:
                prev_header = self.get_block_header(self.last_block_height - 1)
                if prev_header:
                    prev_timestamp = prev_header.get("timestamp", 0)
                else:
                    prev_timestamp = self.last_timestamp
            else:
                prev_timestamp = self.last_timestamp
            
            # Obtener timestamp del bloque actual
            curr_header = self.get_block_header(current_height - 1)
            if curr_header:
                curr_timestamp = curr_header.get("timestamp", 0)
            else:
                curr_timestamp = int(time.time())
            
            # Calcular tiempo diferencial
            time_diff = max(curr_timestamp - prev_timestamp, 1)
            self.last_timestamp = curr_timestamp
            
            # Guardar tiempo de bloque
            self.block_times.append(time_diff)
            
            # Calcular estadísticas
            if time_diff > 0:
                stats["blocks_per_hour"] = (3600 / time_diff) * blocks_found
            
            # Contar bloques con GPU penalty
            for h in range(self.last_block_height, current_height):
                if self.is_gpu_penalty_block(h):
                    self.gpu_penalty_blocks += 1
                self.total_blocks += 1
            
            stats["gpu_penalty_blocks"] = self.gpu_penalty_blocks
            stats["total_blocks"] = self.total_blocks
            
            # Promedio de tiempo de bloque
            if len(self.block_times) > 0:
                stats["block_time_avg"] = sum(self.block_times) / len(self.block_times)
            
            # Estimar hash rate (basado en tiempo de bloque)
            # Fórmula simplificada: no es exacta pero da una estimación
            if stats["block_time_avg"] > 0:
                stats["hash_rate_est"] = 1.0 / stats["block_time_avg"]  # Placeholder
        
        # Obtener dificultad
        stats["network_difficulty"] = info.get("difficulty", 0)
        
        self.last_block_height = current_height
        return stats
    
    def print_stats(self, stats):
        """Mostrar estadísticas en formato legible"""
        if not stats:
            return
        
        print("\n" + "="*70)
        print(f"  NINACATCOIN MINING MONITOR - {stats['timestamp']}")
        print("="*70)
        print(f"  Height:                    {stats['height']:,}")
        print(f"  Total blocks mined:        {stats['total_blocks']}")
        print(f"  GPU penalty blocks:        {stats['gpu_penalty_blocks']}")
        if stats['total_blocks'] > 0:
            penalty_rate = (stats['gpu_penalty_blocks'] / stats['total_blocks']) * 100
            print(f"  GPU penalty rate:          {penalty_rate:.1f}% (esperado: 20%)")
        print(f"  Network difficulty:        {stats['network_difficulty']:,}")
        print(f"  Avg block time:            {stats['block_time_avg']:.1f}s")
        print(f"  Blocks per hour:           {stats['blocks_per_hour']:.2f}")
        print("="*70)
    
    def run(self):
        """Ejecutar monitoreo en loop"""
        print(f"🔍 Conectando a Ninacatcoin RPC: {self.rpc_url}")
        print(f"📊 Actualización cada {self.update_interval} segundos")
        print("Presiona Ctrl+C para salir\n")
        
        try:
            while True:
                stats = self.get_stats()
                self.print_stats(stats)
                time.sleep(self.update_interval)
        except KeyboardInterrupt:
            print("\n\n✅ Monitor detenido")
            sys.exit(0)

if __name__ == "__main__":
    # Configuración
    RPC_URL = "http://127.0.0.1:29084/json_rpc"  # Node 2 RPC
    UPDATE_INTERVAL = 10  # segundos
    
    # Iniciar monitor
    monitor = NinacatcoinMonitor(rpc_url=RPC_URL, update_interval=UPDATE_INTERVAL)
    monitor.run()
