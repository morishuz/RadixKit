# Packaging a release

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DRADIXKIT_BUILD_STRESS_TESTS=ON
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
python3 scripts/audit_release.py
python3 scripts/check_package.py
python3 scripts/package_release.py
```

The deterministic archive is `build-dist/radixkit-source.tar.gz`, with a
`radixkit/` root directory. It includes public headers, CMake integration, examples,
tests, documentation and the optional benchmark with its pinned ska_sort license.
Git metadata, raw results, private research history and local build output are
excluded. No network access is required to build the library or benchmarks.

Install with CMake and use `find_package(radixkit CONFIG REQUIRED)` and
`radixkit::radixkit`, or include the project with `add_subdirectory`. Package checks
validate both integration routes and a relocated installation.

Before distributing a new version, run the [correctness checks](CORRECTNESS.md),
review [API and memory contracts](API.md), check notices and documentation links,
and reproduce any advertised benchmark claim. The [release audit](PUBLIC_RELEASE_AUDIT.md)
records the scope of the current local validation.

RadixKit was renamed from fast32. The namespace, header tree, CMake package/target
and build-option prefix changed; use a fresh build directory after migration.
Version 0.1.0 is the initial standalone release and does not imply validation on
all C++20 platforms.
