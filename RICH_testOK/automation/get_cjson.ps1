$ErrorActionPreference = "Stop"

$base = "third_party/cjson"
New-Item -ItemType Directory -Force -Path $base | Out-Null

Invoke-WebRequest -Uri "https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.c" -OutFile "$base/cJSON.c"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.h" -OutFile "$base/cJSON.h"

Write-Host "cJSON downloaded to $base"
