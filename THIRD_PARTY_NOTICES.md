# Third-party material

RadixKit project code is licensed under the [MIT License](LICENSE). Installed
headers depend only on the C++ standard library. The original fast32 contributor
copyright notices are retained after the project rename.

The optional sorting benchmark vendors `ska_sort` by Malte Skarupke under the
Boost Software License 1.0. Its source revision and two C++20 compatibility edits
are documented in [its README](third_party/ska_sort/README.md); the complete
[license](third_party/ska_sort/LICENSE_1_0.txt) and upstream copyright notice are
retained. No ska_sort code is installed with RadixKit. The source archive includes
it only to keep the optional benchmark reproducible.
