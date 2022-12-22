Prior to running the fuzzers, build and prepare for instance as follows:
```sh
CXX=clang++ cmake -DBUILD_FUZZING=1 ..
make
export LSAN_OPTIONS=detect_leaks=0
```
