import sys
import os

if not os.path.exists(sys.argv[1]):
    # If the .cmd file doesn't exist, just print an empty dependency
    print(f"{sys.argv[2]}: ")
    sys.exit(0)

with open(sys.argv[1], 'r', errors='ignore') as f:
    lines = f.readlines()

deps = []
capture = False
for line in lines:
    if line.startswith("deps_"):
        capture = True
        continue
    if capture:
        if line.strip() == "" or line.startswith("kernel/"): break
        deps.append(line.strip().replace(" \\", ""))

print(f"{sys.argv[2]}: {' '.join(deps)}")
