#include "ChunkedStreamFileUtils.h"

#include "Base/ByteInputStream.h"
#include "Base/Endian.h"
#include <cstring>
#include <memory>

BEGIN_NAMESPACE(ChunkedStreamFileUtils)

//----------------------------------------------------------------------------------------------------------------------
// Describes a data stream 'subscriber'.
// This was obtained from 3DO SDK documentation.
//----------------------------------------------------------------------------------------------------------------------
struct DataStreamSubscriber {
    FourCID     dataType;
    uint32_t    subscriberPort;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(subscriberPort);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Header for a stream file.
// This was obtained from 3DO SDK documentation.
//----------------------------------------------------------------------------------------------------------------------
struct StreamHeader {
    FourCID                 chunkType;              // Should be 'SHDR'
    uint32_t                chunkSize;              // Should be 'sizeof(StreamHeader)'
    uint32_t                time;
    uint32_t                channel;
    uint32_t                subChunkType;
    uint32_t                headerVersion;          // Should be '2'
    uint32_t                streamBlockSize;
    uint32_t                streamBuffers;
    uint32_t                streamerDeltaPri;
    uint32_t                dataAcqDeltaPri;
    uint32_t                numSubsMsgs;
    uint32_t                audioClockChan;
    uint32_t                enableAudioChan;
    uint32_t                preloadInstList[16];    // Totally guessing the sizes of these, but I'm not using them anyway so it doesn't matter!
    DataStreamSubscriber    subscriberList[16];

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(chunkSize);
        Endian::convertBigToHost(time);
        Endian::convertBigToHost(channel);
        Endian::convertBigToHost(subChunkType);
        Endian::convertBigToHost(headerVersion);
        Endian::convertBigToHost(streamBlockSize);
        Endian::convertBigToHost(streamBuffers);
        Endian::convertBigToHost(streamerDeltaPri);
        Endian::convertBigToHost(dataAcqDeltaPri);
        Endian::convertBigToHost(numSubsMsgs);
        Endian::convertBigToHost(audioClockChan);
        Endian::convertBigToHost(enableAudioChan);
        
        for (uint32_t i = 0; i < C_ARRAY_SIZE(preloadInstList); ++i) {
            Endian::convertBigToHost(preloadInstList[i]);
        }

        for (uint32_t i = 0; i < C_ARRAY_SIZE(subscriberList); ++i) {
            subscriberList->convertBigToHostEndian();
        }
    }
};

//----------------------------------------------------------------------------------------------------------------------
// A header for chunk (other than the header) in a stream file.
// The data follows the chunk header in the stream.
// I have incomplete info on what all these fields mean; what I have here was pieced together from clues in
// the 3DO SDK as well as a bit of reverse engineering and hex editing...
//----------------------------------------------------------------------------------------------------------------------
struct ChunkHeader {
    FourCID     chunkType;
    uint32_t    chunkSize;
    uint32_t    _unknown1;
    uint32_t    _unknown2;
    FourCID     subChunkType;   // This can just be ignored, 3DO streaming libraries probably made use of this...
    uint32_t    _unknown3;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(chunkSize);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Iterates over all the chunks in a stream file to determine the size of the specified sub stream
//----------------------------------------------------------------------------------------------------------------------
uint32_t determineSubStreamSize(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    const FourCID subStreamId
) noexcept {
    ASSERT(streamFileSize > sizeof(StreamHeader));

    try {
        // First skip over the stream header
        uint32_t subStreamSize = 0;
        ByteInputStream bytes(pStreamFileData, streamFileSize);
        bytes.consume(sizeof(StreamHeader));

        // Keep reading more chunk headers until we are done
        while (bytes.hasBytesLeft()) {
            // Get the sub chunk header and see if it is the type we are interested in.
            // If it is the correct type then account for it's size.
            ChunkHeader header;
            bytes.read(header);
            header.convertBigToHostEndian();
            const uint32_t chunkDataSize = header.chunkSize - sizeof(ChunkHeader);

            if (header.chunkType == subStreamId) {
                subStreamSize += chunkDataSize;
            }

            // Skip past this chunk
            bytes.consume(chunkDataSize);
        }

        // Return the result
        return subStreamSize;
    }
    catch (...) {
        return 0;
    }
}

bool getSubStreamData(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    const FourCID subStreamId,
    std::byte*& pSubStreamDataOut,
    uint32_t& subStreamSizeOut
) noexcept {
    ASSERT(pStreamFileData || streamFileSize == 0);

    // Give these initial invalid values
    pSubStreamDataOut = nullptr;
    subStreamSizeOut = 0;

    // The stream data MUST be at least big enough for the stream header
    if (streamFileSize <= sizeof(StreamHeader))
        return false;
    
    // Read the stream header and endian correct
    StreamHeader header;
    std::memcpy(&header, pStreamFileData, sizeof(StreamHeader));
    header.convertBigToHostEndian();

    // Ensure the header is what we expect
    if (header.chunkType != FourCID("SHDR"))
        return false;
    
    if (header.headerVersion != 2)
        return false;

    if (header.chunkSize != sizeof(StreamHeader))
        return false;
    
    // Determine the size of the sub stream, if there is no data then we cannot read it
    const uint32_t subStreamSize = determineSubStreamSize(pStreamFileData, streamFileSize, subStreamId);

    if (subStreamSize <= 0)
        return false;

    // Read all of the data for this stream from each chunk in the stream file
    std::unique_ptr<std::byte[]> streamData(new std::byte[subStreamSize]);

    try {
        // First skip over the stream header
        std::byte* pCurData = streamData.get();
        ByteInputStream bytes(pStreamFileData, streamFileSize);
        bytes.consume(sizeof(StreamHeader));

        // Keep reading more chunks in the stream until we are done
        while (bytes.hasBytesLeft()) {
            // Get this chunk's header and see if it is the type we are interested in.
            // If it is the correct type then read the chunk, otherwise skip:
            ChunkHeader header;
            bytes.read(header);
            header.convertBigToHostEndian();
            const uint32_t chunkDataSize = header.chunkSize - sizeof(ChunkHeader);

            if (header.chunkType == subStreamId) {
                bytes.readBytes(pCurData, chunkDataSize);
                pCurData += chunkDataSize;
            } else {
                bytes.consume(chunkDataSize);
            }
        }

        // If we read successfully then make the unique ptr relinquish memory ownership and hand over to caller
        pSubStreamDataOut = streamData.release();
        subStreamSizeOut = subStreamSize;

        return true;    // We succeeded to read the stream data!
    }
    catch (...) {
        return false;   // Failed to read the stream data!
    }
}

END_NAMESPACE(ChunkedStreamFileUtils)
