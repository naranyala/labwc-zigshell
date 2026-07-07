#!/bin/bash
cd dotfiles/ocws/

find . -type f \( -name "*.config" -o -name "*.set" -o -name "*.widget" -o -name "*.source" \) | while read -r file; do
    grep -oP 'include\("\K[^"]+(?="\))' "$file" 2>/dev/null | while read -r inc; do
        if [[ "$inc" != */* ]]; then
            new_path=$(find . -name "$inc" | head -n 1 | sed 's|^./||')
            if [ -n "$new_path" ]; then
                sed -i "s|include(\"$inc\")|include(\"$new_path\")|g" "$file"
            fi
        fi
    done
done
echo "Paths updated."
