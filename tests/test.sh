#!/bin/bash

# Calea catre executabilul client
CLIENT="./da"
# Directorul tinta

# TARGET_ROOT="/usr/include"
TARGET_ROOT="/usr"

# 1. Verificari preliminare
if [ ! -f "$CLIENT" ]; then
    echo "❌ Eroare: Executabilul '$CLIENT' nu exista. Compileaza proiectul cu 'make'."
    exit 1
fi

if [ ! -d "$TARGET_ROOT" ]; then
    echo "❌ Eroare: Directorul $TARGET_ROOT nu exista."
    exit 1
fi

echo "🚀 Pornire analiza in masa pentru subdirectoarele din $TARGET_ROOT..."

# 2. Iterare prin foldere
# Numaram cate am adaugat
count=0

for dir in "$TARGET_ROOT"/*; do
    if [ -d "$dir" ]; then
        # Trimitem comanda de add (-a) cu prioritate normala (-p 2)
        # Daemonul va raspunde cu "Job added with ID: X"
        echo " -> Adaugare job pentru: $dir"
        $CLIENT -a "$dir" -p $(((count%3) + 1))
        
        ((count++))
        
        # Optional: O mica pauza de 0.1s pentru a nu inunda bufferul socket-ului
        # daca daemonul nu e perfect optimizat, dar ar trebui sa mearga si fara.
        # sleep 0.1
    fi
done

echo "-----------------------------------------------------"
echo "✅ Am trimis $count cereri de analiza."
echo "📋 Listare status curent (./da -l):"
echo "-----------------------------------------------------"

# 3. Afisare lista finala
$CLIENT -l