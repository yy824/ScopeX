# scopeX

* M2-01 single producer single consumer model
  
* M2-02 matching engine -> producer(commiting orders) consumer(matching orders) threads

# Building and installing

See the [BUILDING](BUILDING.md) document.

- install
  
  ```sudo apt update```
  
  ```sudo apt install -y build-essential ninja-build cmake gdb```
  
  optional:

  installation:
  
  ```sudo apt install doxygen doxygen-doc doxygen-gui graphviz texlive sphinx```

  ```pipx inject sphinx breathe myst-parser sphinx-rtd-theme```

  run from root folder:

  ```doxygen```

  ```sphinx-build -E -b html docs/sphinx/source docs/doc_html/```

- developement
  
  run ```rm -rf build``` at first, in case of changed cmake.
  
  create cmake environment ```cmake --preset dev```.

  - build
    * ```cmake --build build --target scopeX```
    * ```cmake --build build --target scopeX_cli``` cli use case
    * ```cmake --build build --target scopeX_bench``` stress test use case

- google test

  1. ```rm -rf build-tests```
  2. ```cmake --preset dev-tests```
  3. ```cmake --build build-tests --target scopeX_tests```
  4. ```ctest --test-dir build-tests --output-on-failure```

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Licensing

<!--
Please go to https://choosealicense.com/licenses/ and choose a license that
fits your needs. The recommended license for a project of this type is the
GNU AGPLv3.
-->
