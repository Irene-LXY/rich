$ErrorActionPreference = "Stop"

$gcc = "gcc"

& $gcc -std=c11 -Wall -Wextra `
  -I third_party/cjson `
  -I src `
  src/test_runner.c `
  src/game_engine_stub.c `
  third_party/cjson/cJSON.c `
  -o rich_test.exe

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. Check the error messages above."
    exit $LASTEXITCODE
}

Write-Host "Build succeeded: rich_test.exe"
