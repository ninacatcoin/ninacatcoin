#!/usr/bin/env python3
"""
COLLECT_DATA.py - Recolecta datos de minería Ninacatcoin en CSV/JSON
Guarda: timestamp, height, hash_rate, gpu_penalty, difficulty, etc.
Objetivo: 43,200 puntos en 30 días (1 punto cada 60 segundos)
"""

import json
import csv
import time
import requests
import os
import sys
from datetime import datetime
from pathlib import Path

class NinacatcoinDataCollector:
    def __init__(self, rpc_url="http://127.0.0.1:29084/json_rpc", 
                 output_dir="mining_data", collection_interval=60):
        """
        Initialize data collector
        
        Args:
            rpc_url: RPC endpoint URL
            output_dir: Directory to save data files
            collection_interval: Interval between data points (seconds)
        """
        self.rpc_url = rpc_url
        self.output_dir = Path(output_dir)
        self.collection_interval = collection_interval
        self.last_block_height = 0
        
        # Crear directorio si no existe
        self.output_dir.mkdir(exist_ok=True)
        
        # Archivos de salida
        self.csv_file = self.output_dir / f"mining_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        self.json_file = self.output_dir / f"mining_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        
        # CSV headers
        self.csv_headers = [
            "timestamp",
            "unix_timestamp",
            "block_height",
            "blocks_found",
            "gpu_penalty_blocks",
            "network_difficulty",
            "block_time_avg",
            "hash_rate_est",
            "blocks_per_hour"
        ]
        
        # Inicializar archivos
        self._init_csv()
        self.json_data = []
    
    def _init_csv(self):
        """Inicializar archivo CSV con headers"""
        with open(self.csv_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=self.csv_headers)
            writer.writeheader()
    
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
                return None
            
            return result.get("result", result)
        except:
            return None
    
    def is_gpu_penalty_block(self, height):
        """Detectar si un bloque tiene GPU penalty"""
        return (height % 5) == 0
    
    def collect_data_point(self):
        """Recolectar un punto de datos"""
        data_point = {
            "timestamp": datetime.now().isoformat(),
            "unix_timestamp": int(time.time()),
            "block_height": 0,
            "blocks_found": 0,
            "gpu_penalty_blocks": 0,
            "network_difficulty": 0,
            "block_time_avg": 0.0,
            "hash_rate_est": 0.0,
            "blocks_per_hour": 0.0
        }
        
        # Obtener info del daemon
        info = self.rpc_call("get_info")
        if not info:
            print(f"❌ [{data_point['timestamp']}] RPC conexión error")
            return None
        
        current_height = info.get("height", 0)
        data_point["block_height"] = current_height
        data_point["network_difficulty"] = info.get("difficulty", 0)
        
        # Calcular bloques encontrados desde último punto
        blocks_found = current_height - self.last_block_height
        data_point["blocks_found"] = blocks_found
        
        # Contar bloques con GPU penalty en este intervalo
        gpu_penalty_count = 0
        for h in range(self.last_block_height, current_height):
            if self.is_gpu_penalty_block(h):
                gpu_penalty_count += 1
        data_point["gpu_penalty_blocks"] = gpu_penalty_count
        
        # Calcular hash rate estimado
        if blocks_found > 0 and self.collection_interval > 0:
            # Fórmula simple: bloques encontrados * 1000 (escala arbitraria)
            data_point["hash_rate_est"] = blocks_found * 1000 / self.collection_interval
            data_point["blocks_per_hour"] = (blocks_found / self.collection_interval) * 3600
        
        # Obtener promedio de tiempo de bloque (últimos 10 bloques)
        block_times = []
        for i in range(max(0, current_height - 10), current_height):
            try:
                header = self.rpc_call("get_block_header", {"height": i})
                if header and i > 0:
                    prev_header = self.rpc_call("get_block_header", {"height": i - 1})
                    if prev_header:
                        time_diff = header.get("timestamp", 0) - prev_header.get("timestamp", 0)
                        if time_diff > 0:
                            block_times.append(time_diff)
            except:
                pass
        
        if block_times:
            data_point["block_time_avg"] = sum(block_times) / len(block_times)
        
        self.last_block_height = current_height
        return data_point
    
    def save_data_point(self, data_point):
        """Guardar punto de datos en CSV y JSON"""
        if not data_point:
            return False
        
        # Guardar en CSV
        try:
            with open(self.csv_file, 'a', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=self.csv_headers)
                writer.writerow(data_point)
        except Exception as e:
            print(f"❌ Error guardando CSV: {e}")
            return False
        
        # Guardar en JSON
        try:
            self.json_data.append(data_point)
            with open(self.json_file, 'w') as f:
                json.dump(self.json_data, f, indent=2)
        except Exception as e:
            print(f"❌ Error guardando JSON: {e}")
            return False
        
        return True
    
    def print_data_point(self, data_point):
        """Mostrar punto de datos"""
        print(f"[{data_point['timestamp']}] "
              f"Height: {data_point['block_height']}, "
              f"Blocks: {data_point['blocks_found']}, "
              f"GPU Penalty: {data_point['gpu_penalty_blocks']}, "
              f"H/s: {data_point['hash_rate_est']:.0f}, "
              f"Difficulty: {data_point['network_difficulty']:,}")
    
    def run(self):
        """Ejecutar colección de datos"""
        print(f"📊 Collector iniciado - {datetime.now().isoformat()}")
        print(f"📁 RPC: {self.rpc_url}")
        print(f"📁 Archivos: {self.csv_file} / {self.json_file}")
        print(f"⏱️  Intervalo de colección: {self.collection_interval}s")
        print(f"📈 Objetivo: 43,200 puntos en 30 días")
        print("Presiona Ctrl+C para detener\n")
        
        data_points_collected = 0
        
        try:
            while True:
                data_point = self.collect_data_point()
                
                if data_point and self.save_data_point(data_point):
                    self.print_data_point(data_point)
                    data_points_collected += 1
                    
                    # Mostrar resumen cada 100 puntos
                    if data_points_collected % 100 == 0:
                        days_equivalent = data_points_collected / (86400 / self.collection_interval / 30)
                        print(f"\n✅ {data_points_collected} puntos recolectados (≈{days_equivalent:.1f} días)")
                        print(f"📊 Archivos: {self.csv_file}\n")
                
                time.sleep(self.collection_interval)
                
        except KeyboardInterrupt:
            print(f"\n\n✅ Collector detenido")
            print(f"📊 Puntos recolectados: {data_points_collected}")
            print(f"📁 Archivos guardados:")
            print(f"   - CSV: {self.csv_file}")
            print(f"   - JSON: {self.json_file}")
            sys.exit(0)

if __name__ == "__main__":
    # Configuración
    RPC_URL = "http://127.0.0.1:29084/json_rpc"  # Node 2 RPC
    OUTPUT_DIR = "mining_data"
    COLLECTION_INTERVAL = 60  # 1 punto cada 60 segundos
    
    # Crear colector
    collector = NinacatcoinDataCollector(
        rpc_url=RPC_URL,
        output_dir=OUTPUT_DIR,
        collection_interval=COLLECTION_INTERVAL
    )
    
    # Iniciar colección
    collector.run()
