import json
import hashlib

print("--- DEPARTMENT OF INVESTIGATIONS: FORENSIC AUDIT INITIATED ---\n")

# Load the blackbox evidence file
try:
    with open('blackbox.json', 'r') as f:
        ledger = json.load(f)
except FileNotFoundError:
    print("[ERROR] blackbox.json not found.")
    exit()

is_valid = True
# The Genesis Hash (must match the exact 64 zeros from main.py)
previous_hash = "0000000000000000000000000000000000000000000000000000000000000000"

for i, block in enumerate(ledger):
    data = block['telemetry']
    saved_prev_hash = block['previous_hash']
    saved_block_hash = block['block_hash']

    # 1. Check if the chain is broken (Did someone delete a block?)
    if saved_prev_hash != previous_hash:
        print(f"[CRITICAL FAILURE] BROKEN CHAIN DETECTED AT BLOCK {i}!")
        is_valid = False
        break
    
    # 2. Recalculate the math using the exact same formula as the helmet
    block_content = data + saved_prev_hash
    recalculated_hash = hashlib.sha256(block_content.encode()).hexdigest()

    # 3. Compare our math to the math saved in the file
    if recalculated_hash != saved_block_hash:
        print(f"[CRITICAL FAILURE] TAMPERING DETECTED IN BLOCK {i}!")
        print(f"-> Evidence reads: {data}")
        print(f"-> Saved Hash: {saved_block_hash[:20]}...")
        print(f"-> True Math:  {recalculated_hash[:20]}...")
        is_valid = False
        break
    
    print(f"Block {i} [VERIFIED] -> {data}")
    # Set the previous hash for the next loop
    previous_hash = saved_block_hash

if is_valid:
    print("\n[SUCCESS] Ledger is 100% Cryptographically Secure. No tampering detected.")
else:
    print("\n[ALERT] LEDGER HAS BEEN COMPROMISED. EVIDENCE IS INVALID.")