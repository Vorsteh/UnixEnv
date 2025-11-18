OUTPUT_FILE="results.txt"
TARGET_DIR="/pkg/"
PROGRAM="./mdu"

echo "Threads | Time" > "$OUTPUT_FILE"

for t in $(seq 1 100); do
    echo "Running with $t threads..."
    
    # Use time command to get real time
    TIME=$(time -p $PROGRAM -j "$t" "$TARGET_DIR" >/dev/null 2>&1 2>&1 | grep real | awk '{print $2}')
    
    echo "$t | $TIME" >> "$OUTPUT_FILE"
done

echo "Done"
