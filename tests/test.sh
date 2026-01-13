#!/bin/bash

CLIENT="./da"

TARGET_ROOT="/usr"

if [ ! -f "$CLIENT" ]; then
    echo "Eroare: Executabilul '$CLIENT' nu exista. Compileaza proiectul cu 'make'."
    exit 1
fi

if [ ! -d "$TARGET_ROOT" ]; then
    echo "Eroare: Directorul $TARGET_ROOT nu exista."
    exit 1
fi

echo "Pornire analiza in masa pentru subdirectoarele din $TARGET_ROOT..."

count=0

for dir in "$TARGET_ROOT"/*; do
    if [ -d "$dir" ]; then
        echo " -> Adaugare job pentru: $dir"
        $CLIENT -a "$dir" -p $(((count%3) + 1))
        
        ((count++))
        
    fi
done

echo "-----------------------------------------------------"
echo " Am trimis $count cereri de analiza."
echo " Listare status curent (./da -l):"
echo "-----------------------------------------------------"

$CLIENT -l