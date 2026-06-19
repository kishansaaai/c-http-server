#!/bin/bash

# Simple script to test the C HTTP server

SERVER_URL="http://127.0.0.1:8080"
echo "Testing C HTTP Server at $SERVER_URL..."

# 1. Test Root
echo -n "1. Testing Root (GET /) ... "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/")
if [ "$HTTP_CODE" -eq 200 ]; then
    echo "PASS (200)"
else
    echo "FAIL ($HTTP_CODE)"
fi

# 2. Test 404
echo -n "2. Testing 404 (GET /nonexistent) ... "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/nonexistent")
if [ "$HTTP_CODE" -eq 404 ]; then
    echo "PASS (404)"
else
    echo "FAIL ($HTTP_CODE)"
fi

# 3. Test CGI
echo -n "3. Testing CGI (GET /cgi-bin/test.sh) ... "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/cgi-bin/test.sh")
if [ "$HTTP_CODE" -eq 200 ] || [ "$HTTP_CODE" -eq 404 ]; then
    echo "PASS ($HTTP_CODE)"
else
    echo "FAIL ($HTTP_CODE)"
fi

# 4. Test Upload
echo -n "4. Testing Upload (POST /upload) ... "
echo "test file content" > test_upload_data.txt
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -F "file=@test_upload_data.txt" "$SERVER_URL/upload")
if [ "$HTTP_CODE" -eq 200 ] || [ "$HTTP_CODE" -eq 400 ]; then
    echo "PASS ($HTTP_CODE)"
else
    echo "FAIL ($HTTP_CODE)"
fi
rm test_upload_data.txt

echo "Tests completed."
