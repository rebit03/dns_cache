DNS-Cache
========

DNS-Cache C++ implementation using radix-tree for storing `A records`
(host->IP address) and doubly linked list for LRU trashing records.

The implementation is thread safe where the write operations have exclusive
locking - update operations for the whole cache and resolve operations for
the linked list, read operations have shared locking.

### Example

 List of records:
* seznam.cz: 77.75.79.53
* seznam.cn: 67.229.34.179
* seznamka.cz: 212.109.183.150
* seznamky.cz: 185.53.178.7

> Tree structure

    seznam
            [k]
                    [a]
                    .cz
                    212.109.183.150
                    [y]
                    .cz
                    185.53.178.7
            [.]
            c
                    [n]
                    67.229.34.179
                    [z]
                    77.75.79.53

### Complexity

##### Update operations

`O(k) + constant amortized time` at worst, where k is length of the host name
and constant amortized time to update the linked list and in case of reach of threshold
to remove the tail of the linked list and merge proxy values

##### Resolve operations

`O(k) + constant amortized time` at worst, where k is length of the host name
and constant amortized time to update the linked list

Usage
------------

Provided source code is in form of static library.

> Clone the repository

    git clone git@github.com:rebit03/dns_cache.git

> Linux

    cd libs/dns_cache
    make all

Link the library using `-ldns_cache`. Required `gcc6` or newer.

> Windows

Add dns_cache project to your solution. Update your project dependencies
`(Project-> Project Dependencies)` and references `(Project-> Add Reference)`
to link the library to your project. Required `MSVC 14.2` or newer.

> Source code

 To use the library, include `dns_cache.h` and define following extern in your
code to set threshold for DNS-Cache:

```<language>
namespace Cache {
	namespace DNS {
		const size_t DNS_CACHE_SIZE = <cache_size>;
	}
}
```

TODO
-----------------------
 * variable data storage and different alphabets using templates
 * better exception handling
 * unit tests
 * finish TODOs in code :)
 * compare to other implementations (LinkedHashMap - boost multi_index,
tbb concurrent hash map, junction ...)
 * enhance makefile for linux build
 * enhance logging
 