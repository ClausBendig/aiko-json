# aiko-json
Tiny and fast library for parsing and generating JSON in C. The library based on jsmn and jWrite.
It's licensed under the MIT license. 

## How to implement

The library contains only the two files `aiko-json.h` and `aiko-json.c`.
Copy these two files into your project, add the file `aiko-json.c` to your build-process and use the header `aiko-json.h`.

If you don't want to use it in your build process, you can build it with cmake and link it to your project.

### Building
```
git clone https://github.com/ClausBendig/aiko-json.git
cd aiko-json/
cmake CMakeLists.txt
make
make install # As root
```
Use `-laiko-json` in gcc to link it with your project.

## Examples

### Generating JSON

### Parsing JSON

