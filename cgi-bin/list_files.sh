#!/bin/sh
printf "Content-Type: application/json\r\n\r\n"
printf "["
first=1
for f in uploads/*; do
  if [ -f "$f" ]; then
    if [ $first -eq 0 ]; then
      printf ","
    fi
    first=0
    name=$(basename "$f")
    size=$(stat -c%s "$f")
    printf "{\"name\": \"%s\", \"size\": %s}" "$name" "$size"
  fi
done
printf "]"
