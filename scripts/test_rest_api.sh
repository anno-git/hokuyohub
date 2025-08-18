#!/bin/bash

# HokuyoHub REST API Smoke Tests
# Usage: ./test_rest_api.sh [base_url] [token]

BASE_URL=${1:-"http://localhost:8080"}
TOKEN=${2:-""}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local auth_required=${4:-false}
    
    log_info "Testing $method $endpoint"
    
    local curl_cmd="curl -s -w '%{http_code}' -X $method"
    
    if [ "$auth_required" = true ] && [ -n "$TOKEN" ]; then
        curl_cmd="$curl_cmd -H 'Authorization: Bearer $TOKEN'"
    fi
    
    if [ -n "$data" ]; then
        curl_cmd="$curl_cmd -H 'Content-Type: application/json' -d '$data'"
    fi
    
    curl_cmd="$curl_cmd $BASE_URL$endpoint"
    
    local response=$(eval $curl_cmd)
    local http_code="${response: -3}"
    local body="${response%???}"
    
    if [[ "$http_code" =~ ^2[0-9][0-9]$ ]]; then
        log_success "$method $endpoint - HTTP $http_code"
        if [ -n "$body" ]; then
            echo "Response: $body" | head -c 200
            echo
        fi
    else
        log_error "$method $endpoint - HTTP $http_code"
        if [ -n "$body" ]; then
            echo "Error: $body"
        fi
    fi
    
    echo
}

echo "=== HokuyoHub REST API Smoke Tests ==="
echo "Base URL: $BASE_URL"
if [ -n "$TOKEN" ]; then
    echo "Using authentication token"
else
    echo "No authentication token provided"
fi
echo

# Test sensors endpoints
log_info "=== Testing Sensors Endpoints ==="
test_endpoint "GET" "/api/v1/sensors"
test_endpoint "GET" "/api/v1/sensors/0"
test_endpoint "PATCH" "/api/v1/sensors/0" '{"enabled": true}' true

# Test filters endpoints
log_info "=== Testing Filters Endpoints ==="
test_endpoint "GET" "/api/v1/filters"
test_endpoint "GET" "/api/v1/filters/prefilter"
test_endpoint "GET" "/api/v1/filters/postfilter"

# Test prefilter update
prefilter_config='{
  "enabled": true,
  "neighborhood": {
    "enabled": true,
    "k": 5,
    "r_base": 0.05,
    "r_scale": 1.0
  },
  "spike_removal": {
    "enabled": true,
    "dr_threshold": 0.3,
    "window_size": 3
  }
}'
test_endpoint "PUT" "/api/v1/filters/prefilter" "$prefilter_config" true

# Test postfilter update
postfilter_config='{
  "enabled": true,
  "isolation_removal": {
    "enabled": true,
    "min_points_size": 3,
    "isolation_radius": 0.1,
    "required_neighbors": 2
  }
}'
test_endpoint "PUT" "/api/v1/filters/postfilter" "$postfilter_config" true

# Test snapshot endpoint
log_info "=== Testing Snapshot Endpoint ==="
test_endpoint "GET" "/api/v1/snapshot"

# Test config endpoints
log_info "=== Testing Config Endpoints ==="

# Test config list
test_endpoint "GET" "/api/v1/configs/list" "" true

# Test config save by name
test_endpoint "POST" "/api/v1/configs/save" '{"name": "smoke_test"}' true

# Test config load by name
test_endpoint "POST" "/api/v1/configs/load" '{"name": "smoke_test"}' true

# Test invalid name validation
test_endpoint "POST" "/api/v1/configs/save" '{"name": "invalid/name"}' true
test_endpoint "POST" "/api/v1/configs/load" '{"name": "invalid/name"}' true

# Test missing name field
test_endpoint "POST" "/api/v1/configs/save" '{"invalid": "field"}' true
test_endpoint "POST" "/api/v1/configs/load" '{"invalid": "field"}' true

# Test export (unchanged)
test_endpoint "GET" "/api/v1/configs/export"

# Test config import (using exported config)
log_info "Exporting config for import test..."
export_response=$(curl -s "$BASE_URL/api/v1/configs/export")
if [ $? -eq 0 ] && [ -n "$export_response" ]; then
    test_endpoint "POST" "/api/v1/configs/import" "$export_response" true
else
    log_error "Failed to export config for import test"
fi

echo "=== Smoke Tests Complete ==="