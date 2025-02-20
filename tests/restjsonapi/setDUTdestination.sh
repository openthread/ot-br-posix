#!/bin/bash

# Command to get the extaddr value and trim any trailing whitespace/newline
EXTADDR=$(sudo ot-ctl extaddr | head -n 1 | tr -d '[:space:]')

# Check if the command executed successfully
if [[ $? -ne 0 || -z $EXTADDR ]]; then
    echo "Error: Failed to retrieve extaddr value."
    exit 1
fi

# Directory containing the .bru files
DIRECTORY="./actions"

# Find and replace "destination": lines in text files
for file in "$DIRECTORY"/*.bru; do
    if [[ -f $file ]]; then
        # Create a temporary file for detecting changes
        tmpfile=$(mktemp)
        cp "$file" "$tmpfile"

        sed -i -E "s/\"destination\":.*$/\"destination\": \"$EXTADDR\",/" "$file"

        # Compare the original file with the modified file
        if ! cmp -s "$tmpfile" "$file"; then
            echo "Updated $file"
        fi

        # Clean up the temporary file
        rm "$tmpfile"
    fi
done

echo "Set DUT destination complete."
