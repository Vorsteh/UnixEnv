OUTPUT_FILE="results.txt"
TARGET_DIR="/pkg/"
PROGRAM="./mdu"

echo "Threads | Time" > "$OUTPUT_FILE"

for t in $(seq 1 100); do
    echo "Running with $t threads..."
    
    START=$(date +%s.%N)
    $PROGRAM -j "$t" "$TARGET_DIR" >/dev/null 2>/dev/null
    END=$(date +%s.%N)

    # Beräkna skillnaden
    TIME=$(echo "$END - $START" | bc)

    echo "$t | $TIME" >> "$OUTPUT_FILE"
done

echo "Done"

