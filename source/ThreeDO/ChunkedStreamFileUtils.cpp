#include "ChunkedStreamFileUtils.h"

#include "Base/ByteStream.h"
#include "Base/Endian.h"
#include <cstring>
#include <memory>

BEGIN_NAMESPACE(ChunkedStreamFileUtils)

//----------------------------------------------------------------------------------------------------------------------
// Describes a data stream 'subscriber'.
// This was obtained from 3DO SDK documentation.
//----------------------------------------------------------------------------------------------------------------------
struct DataStreamSubscriber {
    CSFFourCID   dataType;
    uint32_t     subscriberPort;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(subscriberPort);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Header chunk for a stream file.
// This was obtained from 3DO SDK documentation.
//----------------------------------------------------------------------------------------------------------------------
struct HeaderChunk {
    CSFFourCID              chunkType;              // Should be 'SHDR'
    uint32_t                chunkSize;
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
// A header for a sub chunk in a stream file.
// The data follows the chunk in the stream.
// This was obtained from 3DO SDK documentation.
//----------------------------------------------------------------------------------------------------------------------
struct SubChunkHeader {
   CSFFourCID   chunkType;
   uint32_t     chunkSize;

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
    const CSFFourCID subStreamId
) noexcept {
    ASSERT(streamFileSize > sizeof(HeaderChunk));

    try {
        // First skip over the file header
        uint32_t subStreamSize = 0;
        ByteStream bytes(pStreamFileData, streamFileSize);
        bytes.consume(sizeof(HeaderChunk));

        // Keep reading more chunk headers until we are done
        while (bytes.hasBytesLeft()) {
            // Get the sub chunk header and see if it is the type we are interested in.
            // If it is the correct type then account for it's size.
            SubChunkHeader header;
            bytes.read(header);
            header.convertBigToHostEndian();
            const uint32_t chunkDataSize = header.chunkSize - sizeof(SubChunkHeader);

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
    const CSFFourCID subStreamId,
    std::byte*& pSubStreamDataOut,
    uint32_t& subStreamSizeOut
) noexcept {
    ASSERT(pStreamFileData || streamFileSize == 0);

    // Give these initial invalid values
    pSubStreamDataOut = nullptr;
    subStreamSizeOut = 0;

    // The stream data MUST be at least big enough for the stream header
    if (streamFileSize <= sizeof(HeaderChunk))
        return false;
    
    // Read the header and endian correct
    HeaderChunk header;
    std::memcpy(&header, pStreamFileData, sizeof(HeaderChunk));
    header.convertBigToHostEndian();

    // Ensure the header is what we expect
    if (header.chunkType != CSFFourCID::make("SHDR"))
        return false;
    
    if (header.headerVersion != 2)
        return false;

    // Determine the size of the sub stream, if there is no data then we cannot read it
    const uint32_t subStreamSize = determineSubStreamSize(pStreamFileData, streamFileSize, subStreamId);

    if (subStreamSize <= 0)
        return false;

    // Read all of the data for this stream from each chunk in the stream file
    std::unique_ptr<std::byte[]> streamData(new std::byte[subStreamSize]);

    try {
        // First skip over the file header
        std::byte* pCurData = streamData.get();
        ByteStream bytes(pStreamFileData, streamFileSize);
        bytes.consume(sizeof(HeaderChunk));

        // Keep reading more stream until we are done
        while (bytes.hasBytesLeft()) {
            // Get the sub chunk header and see if it is the type we are interested in.
            // If it is the correct type then read the chunk, otherwise skip:
            SubChunkHeader header;
            bytes.read(header);
            header.convertBigToHostEndian();
            const uint32_t chunkDataSize = header.chunkSize - sizeof(SubChunkHeader);

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
