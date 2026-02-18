#!/usr/bin/env python3
"""
NinaCatCoin - Generador automático de checkpoints.dat
=====================================================

Genera el archivo binario checkpoints.dat consultando el daemon RPC.
También actualiza el hash SHA256 en blockchain.cpp automáticamente.

Formato del archivo:
  [4 bytes]  nblocks (uint32 LE) - número de grupos de 512 bloques
  Por cada grupo:
    [32 bytes] cn_fast_hash(hashes de 512 bloques concatenados)
    [32 bytes] cn_fast_hash(weights de 512 bloques como uint64 LE concatenados)

Uso:
  1. Tener el daemon corriendo con RPC en localhost:19081
  2. python3 generate_checkpoints_dat.py
  3. Recompilar el proyecto

Opciones:
  --daemon-url URL     RPC del daemon (default: http://127.0.0.1:19081)
  --output PATH        Archivo de salida (default: src/blocks/checkpoints.dat)
  --update-source      Actualizar hash SHA256 en blockchain.cpp
  --network NETWORK    mainnet, testnet, stagenet (default: mainnet)
  --dry-run            Solo mostrar info, no escribir archivos
"""

import argparse
import hashlib
import json
import os
import re
import struct
import sys
import requests
from pathlib import Path
from typing import List, Tuple, Optional

# ===========================================================================
# Keccak-256 puro en Python (cn_fast_hash en Cryptonote)
# Cryptonote usa Keccak-256 (NO SHA3-256, que tiene padding diferente)
# Implementación sin dependencias externas
# ===========================================================================

# Intentar usar librerías nativas primero (más rápido)
_keccak_impl = None
try:
    from Crypto.Hash import keccak as _keccak_mod
    _keccak_impl = "pycryptodome"
except ImportError:
    try:
        import sha3 as _sha3_mod
        _keccak_impl = "pysha3"
    except ImportError:
        _keccak_impl = "pure"

if _keccak_impl == "pycryptodome":
    def cn_fast_hash(data: bytes) -> bytes:
        k = _keccak_mod.new(digest_bits=256)
        k.update(data)
        return k.digest()
elif _keccak_impl == "pysha3":
    def cn_fast_hash(data: bytes) -> bytes:
        return _sha3_mod.keccak_256(data).digest()
else:
    # Implementación pura de Keccak-256 en Python
    # Basada en la especificación NIST FIPS 202 / Keccak reference
    def _keccak_f1600(state):
        """Keccak-f[1600] permutation."""
        RC = [
            0x0000000000000001, 0x0000000000008082, 0x800000000000808A,
            0x8000000080008000, 0x000000000000808B, 0x0000000080000001,
            0x8000000080008081, 0x8000000000008009, 0x000000000000008A,
            0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
            0x000000008000808B, 0x800000000000008B, 0x8000000000008089,
            0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
            0x000000000000800A, 0x800000008000000A, 0x8000000080008081,
            0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
        ]
        ROT = [
            [0, 36, 3, 41, 18], [1, 44, 10, 45, 2],
            [62, 6, 43, 15, 61], [28, 55, 25, 21, 56],
            [27, 20, 39, 8, 14],
        ]
        MASK64 = (1 << 64) - 1

        lanes = list(state)
        for rc in RC:
            # θ
            C = [lanes[x] ^ lanes[x + 5] ^ lanes[x + 10] ^ lanes[x + 15] ^ lanes[x + 20] for x in range(5)]
            D = [C[(x - 1) % 5] ^ (((C[(x + 1) % 5] << 1) | (C[(x + 1) % 5] >> 63)) & MASK64) for x in range(5)]
            lanes = [(lanes[i] ^ D[i % 5]) & MASK64 for i in range(25)]
            # ρ and π
            B = [0] * 25
            for x in range(5):
                for y in range(5):
                    rot = ROT[x][y]
                    val = lanes[x + 5 * y]
                    B[y + 5 * ((2 * x + 3 * y) % 5)] = ((val << rot) | (val >> (64 - rot))) & MASK64
            # χ
            lanes = [((B[i] ^ ((~B[(i // 5) * 5 + (i % 5 + 1) % 5]) & B[(i // 5) * 5 + (i % 5 + 2) % 5])) & MASK64) for i in range(25)]
            # ι
            lanes[0] = (lanes[0] ^ rc) & MASK64
        return lanes

    def cn_fast_hash(data: bytes) -> bytes:
        """Keccak-256 (rate=1088, capacity=512, output=256 bits, Keccak padding)."""
        rate = 136  # 1088 bits = 136 bytes
        # Keccak padding (NOT SHA3): append 0x01 and then 0x80 at end of block
        padded = bytearray(data)
        padded.append(0x01)
        while len(padded) % rate != 0:
            padded.append(0x00)
        padded[-1] |= 0x80

        # Initialize state (25 x uint64 = 1600 bits)
        state = [0] * 25

        # Absorb
        for offset in range(0, len(padded), rate):
            block = padded[offset:offset + rate]
            for i in range(rate // 8):
                state[i] ^= int.from_bytes(block[i * 8:(i + 1) * 8], 'little')
            state = _keccak_f1600(state)

        # Squeeze (only need 32 bytes = 256 bits, which is < rate)
        output = b""
        for i in range(4):  # 4 * 8 = 32 bytes
            output += state[i].to_bytes(8, 'little')
        return output

    print("NOTA: Usando implementación pura de Keccak-256 (más lenta).")
    print("      Para más velocidad: pip install pycryptodome")


HASH_OF_HASHES_STEP = 512  # Debe coincidir con cryptonote_config.h
HASH_SIZE = 32  # bytes en un crypto::hash
CHECKPOINTS_DAT_URL = "https://ninacatcoin.es/checkpoints/checkpoints.dat"
CHECKPOINTS_SHA256_URL = "https://ninacatcoin.es/checkpoints/checkpoints.dat.sha256"


class CheckpointsDatGenerator:
    """Genera checkpoints.dat desde el daemon RPC."""

    def __init__(self, daemon_url: str = "http://127.0.0.1:19081"):
        self.daemon_url = daemon_url
        self.session = requests.Session()

    def json_rpc(self, method: str, params: dict = None) -> dict:
        """Llamada JSON-RPC al daemon."""
        payload = {
            "jsonrpc": "2.0",
            "id": "0",
            "method": method,
        }
        if params:
            payload["params"] = params
        resp = self.session.post(f"{self.daemon_url}/json_rpc", json=payload, timeout=30)
        resp.raise_for_status()
        result = resp.json()
        if "error" in result:
            raise RuntimeError(f"RPC error: {result['error']}")
        return result.get("result", {})

    def get_height(self) -> int:
        """Obtener altura actual de la blockchain."""
        info = self.json_rpc("get_info")
        return info["height"]

    def get_block_hash(self, height: int) -> str:
        """Obtener hash de un bloque por altura."""
        resp = self.session.post(
            f"{self.daemon_url}/json_rpc",
            json={
                "jsonrpc": "2.0",
                "id": "0",
                "method": "on_getblockhash",
                "params": [height],
            },
            timeout=30,
        )
        resp.raise_for_status()
        result = resp.json()
        if "error" in result:
            raise RuntimeError(f"RPC error getting hash for height {height}: {result['error']}")
        return result["result"]

    def get_block_header(self, height: int) -> dict:
        """Obtener header de un bloque por altura."""
        result = self.json_rpc("getblockheaderbyheight", {"height": height})
        return result["block_header"]

    def get_block_hashes_batch(self, start_height: int, count: int) -> List[str]:
        """Obtener hashes de bloques en lote."""
        hashes = []
        for h in range(start_height, start_height + count):
            hash_hex = self.get_block_hash(h)
            hashes.append(hash_hex)
        return hashes

    def get_block_weights_batch(self, start_height: int, count: int) -> List[int]:
        """Obtener weights de bloques en lote."""
        weights = []
        for h in range(start_height, start_height + count):
            header = self.get_block_header(h)
            weights.append(header["block_weight"])
        return weights

    def hex_to_bytes(self, hex_str: str) -> bytes:
        """Convertir hash hex a bytes (little-endian como en Cryptonote)."""
        return bytes.fromhex(hex_str)

    def compute_group_hashes(self, block_hashes: List[str], block_weights: List[int]) -> Tuple[bytes, bytes]:
        """
        Calcular hash_of_hashes y hash_of_weights para un grupo de 512 bloques.
        
        Reproduce exactamente lo que hace blockchain.cpp:
          cn_fast_hash(data_hashes.data(), HASH_OF_HASHES_STEP * sizeof(crypto::hash), hash)
          cn_fast_hash(data_weights.data(), HASH_OF_HASHES_STEP * sizeof(uint64_t), hash)
        """
        assert len(block_hashes) == HASH_OF_HASHES_STEP, \
            f"Se necesitan exactamente {HASH_OF_HASHES_STEP} hashes, se recibieron {len(block_hashes)}"
        assert len(block_weights) == HASH_OF_HASHES_STEP, \
            f"Se necesitan exactamente {HASH_OF_HASHES_STEP} weights, se recibieron {len(block_weights)}"

        # Concatenar hashes (cada uno es 32 bytes)
        hashes_data = b""
        for h in block_hashes:
            hashes_data += self.hex_to_bytes(h)

        # Concatenar weights (cada uno es uint64 little-endian, 8 bytes)
        weights_data = b""
        for w in block_weights:
            weights_data += struct.pack("<Q", w)

        # cn_fast_hash = Keccak-256
        hash_of_hashes = cn_fast_hash(hashes_data)
        hash_of_weights = cn_fast_hash(weights_data)

        return hash_of_hashes, hash_of_weights

    def generate(self, dry_run: bool = False) -> Tuple[bytes, int, int]:
        """
        Generar el contenido binario de checkpoints.dat.
        
        Returns:
            (datos_binarios, num_grupos, altura_blockchain)
        """
        height = self.get_height()
        num_groups = height // HASH_OF_HASHES_STEP

        print(f"Altura de la blockchain: {height}")
        print(f"Grupos completos de {HASH_OF_HASHES_STEP} bloques: {num_groups}")
        print(f"Bloques cubiertos: {num_groups * HASH_OF_HASHES_STEP}")
        print(f"Bloques restantes (no cubiertos): {height - num_groups * HASH_OF_HASHES_STEP}")

        if num_groups == 0:
            print(f"\nSe necesitan al menos {HASH_OF_HASHES_STEP} bloques para generar un grupo.")
            print("Generando archivo vacío (0 grupos).")
            return struct.pack("<I", 0), 0, height

        if dry_run:
            print("\n[DRY RUN] No se consultarán los bloques.")
            return b"", num_groups, height

        # Generar cada grupo
        dat = struct.pack("<I", num_groups)  # nblocks header

        for g in range(num_groups):
            start = g * HASH_OF_HASHES_STEP
            end = start + HASH_OF_HASHES_STEP
            progress = f"[{g + 1}/{num_groups}]"
            print(f"\r{progress} Procesando bloques {start}-{end - 1}...", end="", flush=True)

            # Obtener hashes y weights del grupo
            block_hashes = self.get_block_hashes_batch(start, HASH_OF_HASHES_STEP)
            block_weights = self.get_block_weights_batch(start, HASH_OF_HASHES_STEP)

            # Calcular hashes del grupo
            hash_of_hashes, hash_of_weights = self.compute_group_hashes(block_hashes, block_weights)

            # Añadir al binario
            dat += hash_of_hashes + hash_of_weights

        print(f"\n\nGenerados {num_groups} grupos ({len(dat)} bytes)")
        return dat, num_groups, height


def compute_sha256(data: bytes) -> str:
    """Calcular SHA256 de datos (como hace blockchain.cpp para validar)."""
    return hashlib.sha256(data).hexdigest()


def update_blockchain_cpp(project_root: Path, new_hash: str) -> bool:
    """Actualizar expected_block_hashes_hash en blockchain.cpp."""
    cpp_path = project_root / "src" / "cryptonote_core" / "blockchain.cpp"
    if not cpp_path.exists():
        print(f"ERROR: No se encontró {cpp_path}")
        return False

    content = cpp_path.read_text(encoding="utf-8")

    # Buscar la línea con el hash
    pattern = r'(static const char expected_block_hashes_hash\[\] = ")[0-9a-fA-F]+(";)'
    match = re.search(pattern, content)
    if not match:
        print("ERROR: No se encontró expected_block_hashes_hash en blockchain.cpp")
        return False

    old_hash = content[match.start(1) + len(match.group(1)):match.end(0) - len(match.group(2))]
    if old_hash == new_hash:
        print(f"Hash SHA256 ya está actualizado: {new_hash}")
        return True

    new_content = content[:match.start(1)] + match.group(1) + new_hash + match.group(2) + content[match.end(0):]
    cpp_path.write_text(new_content, encoding="utf-8")
    print(f"blockchain.cpp actualizado:")
    print(f"  Anterior: {old_hash}")
    print(f"  Nuevo:    {new_hash}")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Generador de checkpoints.dat para NinaCatCoin",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos:
  %(prog)s                                    # Generar con defaults
  %(prog)s --daemon-url http://1.2.3.4:19081  # Daemon remoto
  %(prog)s --update-source                    # Actualizar blockchain.cpp
  %(prog)s --network testnet                  # Para testnet
  %(prog)s --dry-run                          # Solo mostrar info
  %(prog)s --upload                           # Generar y subir al hosting
  %(prog)s --upload --update-source           # Generar, subir y actualizar source
  %(prog)s --auto --upload --interval 300     # Modo auto cada 5 min con upload
  %(prog)s --download                         # Descargar desde ninacatcoin.es
  %(prog)s --download --update-source         # Descargar y actualizar blockchain.cpp

Variables de entorno para SFTP:
  SFTP_HOST       Hostname del servidor (ej: access-xxx.webspace-host.com)
  SFTP_PORT       Puerto (default: 22)
  SFTP_USER       Usuario
  SFTP_PASSWORD   Contraseña
  SFTP_PATH       Ruta remota (default: /checkpoints/)
        """,
    )
    parser.add_argument(
        "--daemon-url",
        default="http://127.0.0.1:19081",
        help="URL del daemon RPC (default: http://127.0.0.1:19081)",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Archivo de salida (default: auto según --network)",
    )
    parser.add_argument(
        "--update-source",
        action="store_true",
        help="Actualizar hash SHA256 en blockchain.cpp (solo mainnet)",
    )
    parser.add_argument(
        "--network",
        choices=["mainnet", "testnet", "stagenet"],
        default="mainnet",
        help="Red objetivo (default: mainnet)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Solo mostrar información, no escribir archivos",
    )
    parser.add_argument(
        "--project-root",
        default=None,
        help="Raíz del proyecto (default: auto-detectar)",
    )
    parser.add_argument(
        "--upload",
        action="store_true",
        help="Subir checkpoints.dat al hosting via SFTP (requiere env vars)",
    )
    parser.add_argument(
        "--auto",
        action="store_true",
        help="Modo automático: monitorea y regenera cuando hay nuevos grupos de 512",
    )
    parser.add_argument(
        "--interval",
        type=int,
        default=300,
        help="Intervalo en segundos para modo --auto (default: 300 = 5 min)",
    )
    parser.add_argument(
        "--download",
        action="store_true",
        help="Download latest checkpoints.dat from " + CHECKPOINTS_DAT_URL,
    )

    args = parser.parse_args()

    # Detectar raíz del proyecto
    if args.project_root:
        project_root = Path(args.project_root)
    else:
        # Subir desde el script hasta encontrar CMakeLists.txt
        script_dir = Path(__file__).resolve().parent
        project_root = script_dir.parent
        if not (project_root / "CMakeLists.txt").exists():
            project_root = Path.cwd()

    # Determinar archivo de salida
    if args.output:
        output_path = Path(args.output)
    else:
        blocks_dir = project_root / "src" / "blocks"
        if args.network == "mainnet":
            output_path = blocks_dir / "checkpoints.dat"
        elif args.network == "testnet":
            output_path = blocks_dir / "testnet_blocks.dat"
        else:
            output_path = blocks_dir / "stagenet_blocks.dat"

    if args.download:
        return download_checkpoints_dat(output_path, args.update_source, project_root)
    elif args.auto:
        run_auto_mode(args, project_root, output_path)
    else:
        run_once(args, project_root, output_path)


def upload_to_hosting(local_path: Path, sha256_hash: str = None) -> bool:
    """Subir checkpoints.dat y su SHA256 al hosting via SFTP."""
    import getpass

    # Defaults para NinaCatCoin hosting (override con env vars)
    host = os.environ.get("SFTP_HOST", "access-5019408946.webspace-host.com")
    port = int(os.environ.get("SFTP_PORT", 22))
    user = os.environ.get("SFTP_USER", "su68747")
    password = os.environ.get("SFTP_PASSWORD", "")
    remote_path = os.environ.get("SFTP_PATH", "/checkpoints/")

    if not password:
        password = getpass.getpass(f"SFTP password for {user}@{host}: ")

    try:
        import paramiko
    except ImportError:
        print("ERROR: paramiko no instalado. Ejecuta: pip install paramiko")
        return False

    try:
        remote_file = remote_path.rstrip("/") + "/" + local_path.name
        print(f"Conectando a {host}:{port}...")
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(host, port=port, username=user, password=password, timeout=15)

        sftp = ssh.open_sftp()

        # Crear directorio remoto si no existe
        try:
            sftp.stat(remote_path)
        except FileNotFoundError:
            print(f"Creando directorio remoto: {remote_path}")
            sftp.mkdir(remote_path)

        print(f"Subiendo {local_path.name} ({local_path.stat().st_size} bytes) -> {remote_file}")
        sftp.put(str(local_path), remote_file)

        # Verificar subida
        remote_size = sftp.stat(remote_file).st_size
        local_size = local_path.stat().st_size
        if remote_size == local_size:
            print(f"Subida OK ({remote_size} bytes)")
        else:
            print(f"ADVERTENCIA: tamaño local ({local_size}) != remoto ({remote_size})")

        # Subir también el SHA256 para verificación
        if sha256_hash:
            sha256_remote = remote_file + ".sha256"
            sha256_content = f"{sha256_hash}  checkpoints.dat\n"
            import io
            sha256_buf = io.BytesIO(sha256_content.encode("utf-8"))
            sftp.putfo(sha256_buf, sha256_remote)
            print(f"SHA256 file uploaded: {sha256_remote}")

            # Subir version JSON para que los nodos puedan verificar actualizaciones
            from datetime import datetime, timezone
            num_groups = struct.unpack("<I", local_path.read_bytes()[:4])[0]
            version_json = {
                "groups": num_groups,
                "blocks_covered": num_groups * HASH_OF_HASHES_STEP,
                "sha256": sha256_hash,
                "url": CHECKPOINTS_DAT_URL,
                "updated": datetime.now(timezone.utc).isoformat(),
            }
            version_remote = remote_path.rstrip("/") + "/checkpoints_version.json"
            version_buf = io.BytesIO(json.dumps(version_json, indent=2).encode("utf-8"))
            sftp.putfo(version_buf, version_remote)
            print(f"Version JSON uploaded: {version_remote}")

        sftp.close()
        ssh.close()
        return True
    except Exception as e:
        print(f"ERROR SFTP: {e}")
        return False


def print_node_update_notice(sha256_hash: str, num_groups: int, blocks_covered: int):
    """Print English notice for node operators to update their checkpoints.dat."""
    notice = f"""
{'=' * 70}
  NOTICE TO ALL NODE OPERATORS - New checkpoints.dat available!
{'=' * 70}

  A new checkpoints.dat has been published with {num_groups} hash group(s)
  covering the first {blocks_covered} blocks of the NinaCatCoin blockchain.

  Download URL:
    {CHECKPOINTS_DAT_URL}

  SHA256 verification:
    {sha256_hash}

  To update your node:
    1. Stop your daemon (ninacatcoind)
    2. Download the new checkpoints.dat:
       curl -o src/blocks/checkpoints.dat {CHECKPOINTS_DAT_URL}
    3. Verify the SHA256 hash matches the one above
    4. Recompile:
       cd build-linux && make -j$(nproc)
    5. Restart your daemon

  This ensures faster initial sync and protection against chain
  reorganization attacks for the covered block range.
{'=' * 70}
"""
    print(notice)


def download_checkpoints_dat(output_path: Path, update_source: bool = False,
                             project_root: Path = None) -> int:
    """Download checkpoints.dat from the official URL and optionally update blockchain.cpp."""
    print(f"Downloading checkpoints.dat from {CHECKPOINTS_DAT_URL}...")

    try:
        resp = requests.get(CHECKPOINTS_DAT_URL, timeout=30)
        resp.raise_for_status()
    except requests.ConnectionError:
        print(f"ERROR: Could not connect to {CHECKPOINTS_DAT_URL}")
        return 1
    except requests.HTTPError as e:
        print(f"ERROR: HTTP {e.response.status_code} - {e}")
        return 1

    dat = resp.content
    if len(dat) < 4:
        print("ERROR: Downloaded file is too small")
        return 1

    num_groups = struct.unpack("<I", dat[:4])[0]
    expected_size = 4 + num_groups * 64
    if len(dat) != expected_size:
        print(f"ERROR: File size mismatch. Expected {expected_size} bytes, got {len(dat)}")
        return 1

    sha256 = compute_sha256(dat)

    # Verify SHA256 against published hash
    try:
        sha_resp = requests.get(CHECKPOINTS_SHA256_URL, timeout=10)
        if sha_resp.status_code == 200:
            published_hash = sha_resp.text.strip().split()[0]
            if published_hash == sha256:
                print(f"SHA256 verified OK: {sha256}")
            else:
                print(f"WARNING: SHA256 mismatch!")
                print(f"  Downloaded: {sha256}")
                print(f"  Published:  {published_hash}")
                print(f"  Proceeding anyway...")
        else:
            print(f"SHA256 file not available (HTTP {sha_resp.status_code}), skipping verification")
    except Exception:
        print("SHA256 file not available, skipping verification")

    # Write file
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(dat)
    print(f"\nSaved: {output_path} ({len(dat)} bytes)")
    print(f"Groups: {num_groups} ({num_groups * HASH_OF_HASHES_STEP} blocks covered)")
    print(f"SHA256: {sha256}")

    # Update blockchain.cpp if requested
    if update_source and project_root:
        print()
        update_blockchain_cpp(project_root, sha256)

    print(f"\n{'=' * 60}")
    print(f"Next steps:")
    print(f"  1. Recompile: cd build-linux && make -j$(nproc)")
    print(f"  2. Restart your daemon")
    print(f"{'=' * 60}")
    return 0


def run_once(args, project_root: Path, output_path: Path) -> int:
    """Ejecutar una sola vez: generar, guardar y opcionalmente subir."""
    print(f"Proyecto: {project_root}")
    print(f"Red:      {args.network}")
    print(f"Daemon:   {args.daemon_url}")
    print(f"Salida:   {output_path}")
    print()

    gen = CheckpointsDatGenerator(daemon_url=args.daemon_url)

    try:
        dat, num_groups, height = gen.generate(dry_run=args.dry_run)
    except requests.ConnectionError:
        print(f"\nERROR: No se pudo conectar al daemon en {args.daemon_url}")
        print("Asegúrate de que el daemon esté corriendo con RPC habilitado.")
        return 1
    except Exception as e:
        print(f"\nERROR: {e}")
        return 1

    if args.dry_run:
        print("\n[DRY RUN] No se escribió ningún archivo.")
        return 0

    # Escribir archivo
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(dat)
    print(f"\nArchivo escrito: {output_path} ({len(dat)} bytes)")

    # Calcular SHA256
    sha256 = compute_sha256(dat)
    print(f"SHA256: {sha256}")

    # Actualizar blockchain.cpp si se pidió (solo mainnet)
    if args.update_source:
        if args.network != "mainnet":
            print("\nNOTA: --update-source solo aplica a mainnet (el hash SHA256")
            print("      solo se valida para mainnet en blockchain.cpp).")
        else:
            print()
            update_blockchain_cpp(project_root, sha256)

    # Subir al hosting si se pidió
    uploaded = False
    if args.upload:
        print()
        uploaded = upload_to_hosting(output_path, sha256_hash=sha256)

    print(f"\n{'=' * 60}")
    print(f"Resumen:")
    print(f"  Altura blockchain:  {height}")
    print(f"  Grupos generados:   {num_groups}")
    print(f"  Bloques cubiertos:  {num_groups * HASH_OF_HASHES_STEP}")
    print(f"  Tamaño archivo:     {len(dat)} bytes")
    print(f"  SHA256:             {sha256}")
    if uploaded:
        print(f"  URL:                {CHECKPOINTS_DAT_URL}")
    if num_groups > 0:
        print(f"\nPróximos pasos:")
        print(f"  1. Recompilar: cd build-linux && make -j$(nproc)")
        if not args.update_source and args.network == "mainnet":
            print(f"  2. O ejecutar de nuevo con --update-source para actualizar blockchain.cpp")
    print(f"{'=' * 60}")

    # Show English notice for node operators after successful upload
    if uploaded and num_groups > 0:
        print_node_update_notice(sha256, num_groups, num_groups * HASH_OF_HASHES_STEP)

    return 0


def run_auto_mode(args, project_root: Path, output_path: Path):
    """Modo automático: monitorea la blockchain y regenera/sube cuando hay nuevos grupos."""
    import time
    from datetime import datetime

    print(f"{'=' * 60}")
    print(f"  MODO AUTOMÁTICO - checkpoints.dat")
    print(f"{'=' * 60}")
    print(f"Proyecto:   {project_root}")
    print(f"Red:        {args.network}")
    print(f"Daemon:     {args.daemon_url}")
    print(f"Salida:     {output_path}")
    print(f"Intervalo:  {args.interval}s ({args.interval // 60} min)")
    print(f"Upload:     {'Sí' if args.upload else 'No'}")
    print(f"{'=' * 60}")
    print()

    last_num_groups = -1  # Forzar primera generación

    while True:
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        try:
            gen = CheckpointsDatGenerator(daemon_url=args.daemon_url)
            height = gen.get_height()
            current_groups = height // HASH_OF_HASHES_STEP

            if current_groups == last_num_groups:
                next_group_at = (last_num_groups + 1) * HASH_OF_HASHES_STEP
                remaining = next_group_at - height
                print(f"[{now}] Altura: {height} | Grupos: {current_groups} | "
                      f"Próximo grupo en {remaining} bloques | Sin cambios")
            else:
                if current_groups == 0:
                    print(f"[{now}] Altura: {height} | Aún no hay 512 bloques | "
                          f"Faltan {HASH_OF_HASHES_STEP - height} bloques para primer grupo")
                else:
                    print(f"\n[{now}] ¡Nuevo grupo detectado! {last_num_groups} -> {current_groups}")
                    print(f"Generando checkpoints.dat...")

                    dat, num_groups, h = gen.generate()

                    # Escribir
                    output_path.parent.mkdir(parents=True, exist_ok=True)
                    output_path.write_bytes(dat)
                    sha256 = compute_sha256(dat)
                    print(f"Archivo: {output_path} ({len(dat)} bytes)")
                    print(f"SHA256:  {sha256}")
                    print(f"Grupos:  {num_groups} ({num_groups * HASH_OF_HASHES_STEP} bloques)")

                    # Subir si está habilitado
                    if args.upload:
                        uploaded = upload_to_hosting(output_path, sha256_hash=sha256)
                        if uploaded:
                            print_node_update_notice(sha256, num_groups,
                                                    num_groups * HASH_OF_HASHES_STEP)

                    # Actualizar source si se pidió
                    if args.update_source and args.network == "mainnet":
                        update_blockchain_cpp(project_root, sha256)

                    print()

                last_num_groups = current_groups

        except requests.ConnectionError:
            print(f"[{now}] Daemon no disponible, reintentando en {args.interval}s...")
        except Exception as e:
            print(f"[{now}] Error: {e}")

        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main() or 0)
