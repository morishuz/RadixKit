# Vendored ska_sort

Source: https://github.com/skarupke/ska_sort
Commit: 2c14d4bf7b667032ed28a481065f721385e33849
File: ska_sort.hpp (two C++20 compatibility substitutions)
Author: Malte Skarupke
License: Boost Software License 1.0 (see LICENSE_1_0.txt).

Used only by the optional sorting benchmark, never installed with RadixKit.
The two removed std::result_of expressions in ska_sort_copy are replaced with
std::invoke_result_t. No sorting logic is changed. The benchmark retains both
ska_sort and ska_sort_copy and includes required scratch/copy-back costs.
