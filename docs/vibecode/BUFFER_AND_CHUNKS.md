# Buffer & Chunk Management

## Chunk

Each chunk stores:
- `shared_ptr<const vector<uint8_t>>` — immutable encrypted data
- `m_uses` (atomic) — number of receivers who confirmed this chunk
- `m_consumerExpected` — reference to buffer's AtomicSet of expected consumers

`howMuchIsLeft()` = `expected - uses`. When 0, chunk is eligible for deletion.

## Buffer

Ordered map of chunks (key = index, ascending). Manages:

- **Expected consumers:** `AtomicSet<publicId>` — all receivers who should download each chunk
- **Freeze flag:** `m_initialChunksFreezing` — prevents deletion during initial window
- **EOF flag:** `m_EOF` — sender finished uploading
- **someChunkWasRemoved flag** — set permanently once any chunk is deleted
- **Byte counters:** `m_bytesIn` (uploaded), `m_bytesOut` (downloaded)

## Sanitization

```cpp
void Buffer::sanitize(removed_list):
    if (m_initialChunksFreezing) return;  // Freeze active — no deletion

    for each chunk in m_chunks:
        if chunk.howMuchIsLeft() == 0:    // All expected receivers confirmed
            remove chunk
            mark someChunkWasRemoved = true
            add index to removed_list
```

Called from:
- `setChunkAsReceived()` — after receiver confirms
- `removeOneFromExpectedConsumers()` — after receiver leaves (reduces expected count)
- `setInitialChunksFreezingDropped()` — when freeze drops

## Flow Control

Server limits buffer to `max_chunk_queue` chunks (default 10).

```
Sender uploads chunk → buffer.addChunk()
  If buffer full (count >= max): return 0 (failure)
  Sender gets HTTP 421 or no new_chunk event

After sanitize removes chunks:
  If was full, now has space → publish newChunkIsAllowed(true) to sender
  Sender resumes uploading
```

## Expected Consumers

When receiver joins: `addNewToExpectedConsumers(publicId)` — fails if freeze dropped AND chunks already removed.

When receiver leaves: `removeOneFromExpectedConsumers(publicId)` → sanitize. Reduces expected count for all chunks, potentially triggering deletion.

## Completion Detection

```
setChunkAsReceived():
    chunk.incrementUses()
    sanitize(removedChunks)
    newCount = chunkCount()

    if newCount == 0 AND eof():
        → Session complete! Remove session.
```

## Key Invariants

1. Chunks are never modified after creation (immutable data)
2. `someChunkWasRemoved` is permanent — once true, new receivers cannot join
3. Freeze prevents ALL deletions, not selective
4. `howMuchIsLeft()` uses current `expectedConsumers.size()`, so removing a receiver immediately affects all chunks
5. Chunk indices are sparse (not reindexed after deletion)
