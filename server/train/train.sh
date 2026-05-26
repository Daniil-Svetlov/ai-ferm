#!/bin/bash
# Usage: ./train.sh
# Runs the autoencoder training with the correct Python version.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"

if command -v python3.11 &> /dev/null; then
    PY=python3.11
elif command -v python3 &> /dev/null; then
    PY=python3
else
    echo "Python 3 not found"
    exit 1
fi

echo "Using: $($PY --version)"
echo "Training autoencoder..."
$PY "$DIR/train_autoencoder.py"
echo "Done! model_autoencoder.h has been generated."
