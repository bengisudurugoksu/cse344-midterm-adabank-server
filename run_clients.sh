#!/bin/bash
echo "[Script] Starting batch client execution..."

for file in Client*.file
do
    echo "[Script] Running client file: $file"
    ./BankClient "$file"
done

echo "[Script] All client operations done."

# 🌟 BankServer'a SIGINT gönder
echo "[Script] Sending SIGINT to BankServer..."
pkill -SIGINT BankServer
