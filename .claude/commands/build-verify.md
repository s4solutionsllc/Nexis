# Build & Verify

Run the standard build-test-verify cycle for the Nexis project. Use after making code changes.

## Arguments

$ARGUMENTS — Optional: "clean" for a clean rebuild, "quick" to skip tests, or blank for default incremental build + test.

## Steps

### 1. Determine build type

- If `$ARGUMENTS` contains "clean" or if `CMakeLists.txt` was modified:
  ```bash
  rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) && cmake --build build -j$(sysctl -n hw.ncpu)
  ```
- Otherwise, incremental build:
  ```bash
  cmake --build build -j$(sysctl -n hw.ncpu)
  ```

### 2. Check build result

- If build fails, analyze the error output and report:
  - Which file(s) failed
  - The specific error messages
  - Suggested fixes
- Do NOT proceed to testing if build fails

### 3. Run tests (unless "quick" mode)

```bash
ctest --test-dir build --output-on-failure
```

- Report test results: N passed, N failed
- If any tests fail, show the failure output and suggest fixes

### 4. Summary

Report a one-line status:
```
✅ Build OK | Tests: 7/7 passed | Time: Xs
```
or
```
❌ Build FAILED | Error in <file>: <message>
```
