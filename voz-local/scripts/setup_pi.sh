#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
sudo apt update
sudo apt install -y git cmake build-essential curl ffmpeg
mkdir -p "$project_dir/vendor"
if [[ ! -d "$project_dir/vendor/whisper.cpp/.git" ]]; then
  git clone https://github.com/ggml-org/whisper.cpp.git "$project_dir/vendor/whisper.cpp"
fi
cd "$project_dir/vendor/whisper.cpp"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
if [[ ! -s models/ggml-tiny.bin ]]; then
  bash ./models/download-ggml-model.sh tiny
fi
echo 'Instalacion terminada. Inicia con: bash scripts/run_pi.sh'
