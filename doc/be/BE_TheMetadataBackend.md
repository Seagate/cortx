# BE - The metadata backend

### Why do we need it ?

We need a place to store the metadata.  
Currently we store 2 kinds of metadata in BE :
- The metadata about the data stored on storage devices. It consists of :
	- balloc : what space is free on the data storage device and what is not
	- extmap in ad stob domain : if we have an ad stob it has the information where the ad stob data is stored on the storage device
	- cob : the gob (file) attributes, including file size
- The metadata exported to user. It's DIX which is exported through Clovis.

### What was wrong with db4/db5 ?

- The locking used in the db5 doesn't fit m0 programming model. Table locks are held until the transaction is committed, as it doesn't have asynchronous interface.
- A fom can't sleep (waiting for disk I/O) during tx commit.
- M0 threading model doesn’t match with db5 threading and locking because it’s assumed that db5 transactions would run inside generic thread working in preemptive manner but cooperative, so it’s impossible to get maximum performance from it.

### Requirements for BE

- Scalability (~100k-1M tx/s)
- Asynchronous interface
- It should be possible to put fom to sleep at any point of tx life cycle
- Atomicity, Consistency, Durability (ACID without I)
- It's allowed to store data structures as is, without serialisation
- RVM-alike operation
- In-memory segments, which is persistently stored on backing store
- Transactional log
- Recovery

### How BE works ?

- Data from segments is mmap()ed to memory
- Changes to segments are captured to transactions
- The captured changes are written to log
- At this point the tx becomes persistent and then written in-place into the segments
- In case of failure the changes from the log are applied to the segments

### BE components relationship

Top-level components
- BE domain : handles BE startup/shutdown
- BE engine : the transaction engine. Manages transactions and transaction groups
- BE segment : data is stored there. Consists of backing store and in-memory  "mapping"
- BE tx : the transaction. The changes in segments are captured to the transactions
- BE log : all the segment changes that need to become persistent go there. The changes that didn't go to the segments are replayed during BE recovery.

Data flow for the user's persistent data  
BE tx usage, 1/2  
BE tx usage, 2/2

### BE data structures

- BE allocator
	- Used to allocate a part of segment that can be used by the BE user.
	- Current implementation : list-based first fit allocator + optimisations.
	- Interface : m0_be_alloc(), m0_be_free(), m0_be_alloc_stats()

- BE list
	- Persistent version of m0_tl
	- Interface : tail(), head(), prev(), next(), add(), add_after(), add_before(), add_tail(), del()

- BE btree
	- Persistent key-value storage
	- Interface : insert(), delete(), update(), lookup(), maxkey(), minkey(), _inplace() version and cursors API

- BE extmap
	- Built on top of btree. Extent map is a persistent transactional collection of extents in an abstract numerical name-space with a numerical value associated to each extent.
	- Interface : obj_insert(), obj_delete(), lookup()
	- Cursor API : next(), prev(), merge(), split(), paste(), count(), extent_update()
	- Caret API : move(), step()

### Current problems and future directions

- Entire BE segment is currently mmap()ed to memory
	- Solution : map individual pages on request
	- Implementation : BE paged, exists in a branch

- A locality may sleep on segment's memory page fault
	- Solution : paged

- There is no good way to sleep for I/O (page request)
	- Solution : asynchronous framework
	- Implementation : PoC exists in a branch

- There is only one open tx group at any moment of time 
	- Solution : (possible) LSN-based serialized txs
	- Implementation : doesn't exists
