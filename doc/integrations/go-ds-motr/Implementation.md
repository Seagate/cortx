# Implementation
I used the [Go bindings](https://github.com/Seagate/cortx-motr/tree/main/bindings/go) to the Motr C API to create 2 modules:

* The [CLI](https://github.com/allisterb/go-ds-motr/blob/master/main.go) module provides interactive functions for testing connectivity to a Motr key-value store, creating indexes and other utility functions.
* The [motrds](https://github.com/allisterb/go-ds-motr/blob/master/motrds/motrds.go) module implements the IPFS data store plugin interface.

The IPFS data store interface consists of a [set of functions](https://github.com/ipfs/go-datastore/blob/master/datastore.go) like **Get**, **Put**, **Has** etc. that each data store must implement. Each function passes a unique key as input. This is relatively simple to translate to the Motr key-value API. I used the FNV-1 hash function to generate a 128-bit identifier for each IPFS CID key and Go functions from the [Motr mkv package](https://github.com/Seagate/cortx-motr/blob/main/bindings/go/mkv/mkv.go) to implement the corresponding data store functions. 

The major challenge to building an IPFS data store plugin for Motr is that keys in an IPFS data store are hierachical and can be queried e.g an IPFS block key might look like `/blocks/CIQFTFEEHEDF6KLBT32BFAGLXEZL4UWFNWM4LFTLMXQBCERZ6CMLX3Y....`  and the IPFS data store must implement a **Query** function which can say 'find all keys that begin with `/blocks/CIQ..` or `/pins/..`'...
Motr however doesn't currently support any kind of native query or search facility. As best as I can tell query functions in the current CORTX RGW S3 implementation [using Motr](https://github.com/Seagate/cortx-rgw/blob/main/src/rgw/rgw_sal_motr.cc) are implemented by storing S3 *metadata only* in an distributed object cache and querying the cache, synchronizing changes when needed.

 So I used a similar approach: a LevelDB database is used alongside Motr to store IPFS *keys only* which correspond to values stored in the Motr store and provide a query and search facility for IPFS keys. So Motr is used to store the data that corresponds to each IPFS CID while a LevelDB database is used as an independent metadata cache to facilitate querying on IPFS CIDs. `Put` and `Delete` operations write both to the Motr store and to LevelDB to indicate that an IPFS block exists in Motr corresponding to this CID. Since IPFS CIDs are immutable there is no need for any more synchronization of metadata once it is written once.

This approach will not affect performance as no IPFS data is actually stored in or retrieved from LevelDB, however it does reduce the reliabiity considerably as the LevelDB CID index is file-based. Future implementations will use a more robust approach following what the RGW S3 implementation does.
