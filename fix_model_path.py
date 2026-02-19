#!/usr/bin/env python3
"""Fix model path in server.py"""

import re

path = r'i:\ninacatcoin\src\nina_ml\server.py'

with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace the hardcoded path with a relative one
old_path = r'Path("src/nina_ml/models/block_validator_model.json")'
new_path = r"Path(__file__).parent / 'models' / 'block_validator_model.json'"

content = content.replace(old_path, new_path)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)

print('✓ Model path fixed successfully')
