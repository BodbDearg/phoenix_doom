#include "MovieDecoder.h"

#include "Audio/AudioData.h"
#include "Base/ByteInputStream.h"
#include "Base/Endian.h"
#include "Base/Finally.h"
#include "ChunkedStreamFileUtils.h"
#include <algorithm>

BEGIN_NAMESPACE(MovieDecoder)

// Header for video data in a movie file
struct VideoStreamHeader {
    FourCID     id;             // Should say 'CVID'
    uint32_t    height;         // Height of the movie in pixels
    uint32_t    width;          // Width of the movie in pixels
    uint32_t    fps;            // Framerate of the movie
    uint32_t    numFrames;      // Number of frames in the movie

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(height);
        Endian::convertBigToHost(width);
        Endian::convertBigToHost(fps);
        Endian::convertBigToHost(numFrames);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Header for a frame of video data in a movie file
//----------------------------------------------------------------------------------------------------------------------
struct VideoFrameHeader {
    // Size of the entire frame data, minus this 32-bit field
    uint32_t frameSize;
    
    // If bit 0 is set then it means the codebooks for each strip in the frame uses the codebook for the previous
    // strip in the frame. If there is no previous strip in the frame, which will always be the case for the videos
    // we are decoding ('numStrips' is always '1') then it means to use the codebook from the previous frame.
    //
    // Note: bits 1-7 are unused, and should be always cleared!
    uint8_t flags;

    // Size of the CVID data in the frame as a 24-bit big endian unsigned.
    // Storing as bytes because there is no standard 24-bit unsigned type in C++...
    uint8_t cvidDataLength[3];

    uint16_t width;         // Width of the video data
    uint16_t height;        // Height of the video data
    uint16_t numStrips;     // Number of coded strips in the CPAK video, I only support '1'
    uint16_t _unknown1;
    uint16_t _unknown2;
    uint16_t _unknown3;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(frameSize);
        Endian::convertBigToHost(width);
        Endian::convertBigToHost(height);
        Endian::convertBigToHost(numStrips);
    }
};

// Frame flags
static constexpr uint8_t FRAME_FLAG_KEEP_CODEBOOKS = 0x01;

// Strip types
static constexpr uint16_t STRIP_TYPE_KEY_FRAME      = 0x1000;     // This video strip is a keyframe and fully specified
static constexpr uint16_t STRIP_TYPE_DELTA_FRAME    = 0x1100;     // This video strip is a frame that only receives partial updates from the previous frame

//----------------------------------------------------------------------------------------------------------------------
// Header for a strip of video data in the CPAK codec.
// CPAK allows many of these per frame, but we only need to support 1 for decoding these movies.
//----------------------------------------------------------------------------------------------------------------------
struct VideoStripHeader {
    uint16_t    stripType;      // Either keyframe or delta frame
    uint16_t    stripSize;      // Size of the strip data
    uint16_t    topY;           // Rect defining what part of the video this strip encodes
    uint16_t    leftX;
    uint16_t    height;
    uint16_t    width;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(stripType);
        Endian::convertBigToHost(stripSize);
        Endian::convertBigToHost(topY);
        Endian::convertBigToHost(leftX);
        Endian::convertBigToHost(height);
        Endian::convertBigToHost(width);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Header for a CVID chunk in the CPAK codec.
//----------------------------------------------------------------------------------------------------------------------
struct CVIDChunkHeader {
    uint16_t    chunkType;
    uint16_t    chunkSize;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(chunkType);
        Endian::convertBigToHost(chunkSize);
    }
};

// CVID Chunk types: key frames
static constexpr uint16_t CVID_KF_V4_CODEBOOK_12_BIT    = 0x2000;
static constexpr uint16_t CVID_KF_V1_CODEBOOK_12_BIT    = 0x2200;
static constexpr uint16_t CVID_KF_V4_CODEBOOK_8_BIT     = 0x2400;
static constexpr uint16_t CVID_KF_V1_CODEBOOK_8_BIT     = 0x2600;
static constexpr uint16_t CVID_KF_VECTORS               = 0x3000;
static constexpr uint16_t CVID_KF_VECTORS_V1_ONLY       = 0x3200;

// CVID Chunk types: delta frames (partially updated frames)
static constexpr uint16_t CVID_DF_V4_CODEBOOK_12_BIT    = 0x2100;
static constexpr uint16_t CVID_DF_V1_CODEBOOK_12_BIT    = 0x2300;
static constexpr uint16_t CVID_DF_V4_CODEBOOK_8_BIT     = 0x2500;
static constexpr uint16_t CVID_DF_V1_CODEBOOK_8_BIT     = 0x2700;
static constexpr uint16_t CVID_DF_VECTORS               = 0x3100;

//----------------------------------------------------------------------------------------------------------------------
// Exception thrown when video decoding fails
//----------------------------------------------------------------------------------------------------------------------
struct VideoDecodeException {};

//----------------------------------------------------------------------------------------------------------------------
// Holds a color in YUV format
//----------------------------------------------------------------------------------------------------------------------
struct YUVColor {
    uint8_t y;
    int8_t  u;      // N.B: Chrominance values are signed!
    int8_t  v;
};

//----------------------------------------------------------------------------------------------------------------------
// Header for audio data in a movie file
//----------------------------------------------------------------------------------------------------------------------
struct AudioStreamHeader {
    uint32_t    _unknown1;
    uint32_t    _unknown2;
    uint32_t    _unknown3;
    uint32_t    _unknown4;
    uint32_t    bitDepth;
    uint32_t    sampleRate;
    uint32_t    numChannels;
    uint32_t    _unknown5;
    uint32_t    _unknown6;
    uint32_t    audioDataSize;

    void convertBigToHostEndian() noexcept {
        Endian::convertBigToHost(bitDepth);
        Endian::convertBigToHost(sampleRate);
        Endian::convertBigToHost(numChannels);
        Endian::convertBigToHost(audioDataSize);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Convert a color in YUV format to XRGB8888 as it is done in the Cinepak codec.
// According to what I read this is not a standard way of converting, but was chosen because of it's simplicity.
//----------------------------------------------------------------------------------------------------------------------
static uint32_t yuvToXRGB8888(const YUVColor color) noexcept {
    const int32_t y = (int32_t) color.y;
    const int32_t u = (int32_t) color.u;
    const int32_t v = (int32_t) color.v;

    const uint32_t r = std::min(std::max(y + (v * 2),       0), 255);
    const uint32_t g = std::min(std::max(y - (u / 2) - v,   0), 255);
    const uint32_t b = std::min(std::max(y + (u * 2),       0), 255);

    return (0xFF000000u | (r << 16) | (g << 8) | (b << 0));
}

//----------------------------------------------------------------------------------------------------------------------
// Decode a block of 4x4 pixels using the specified codebook (which will be either the 'V1' or 'V4' codebook) and the
// given set of indexes that reference particular vectors in the codebook.
//----------------------------------------------------------------------------------------------------------------------
static void decodePixelBlock(
    VideoDecoderState& decoderState,
    const uint32_t blockIdx,
    const VidCodebook& codebook,
    const uint8_t v0Idx,
    const uint8_t v1Idx,
    const uint8_t v2Idx,
    const uint8_t v3Idx
) noexcept {
    // Figure out the row and column in terms of rows
    constexpr uint32_t NUM_BLOCKS_PER_ROW = VIDEO_WIDTH / 4;

    const uint32_t blockRow = blockIdx / NUM_BLOCKS_PER_ROW;
    const uint32_t blockCol = blockIdx - (blockRow * NUM_BLOCKS_PER_ROW);

    // Figure out the top left x and y in terms of pixels
    const uint32_t lx = blockCol * 4;
    const uint32_t ty = blockRow * 4;    

    // Grab the 4 vectors for this block
    const VidVec v0 = codebook.vectors[v0Idx];
    const VidVec v1 = codebook.vectors[v1Idx];
    const VidVec v2 = codebook.vectors[v2Idx];
    const VidVec v3 = codebook.vectors[v3Idx];

    // Get the YUV colors for each pixel
    YUVColor pixelsYUV[16];
    pixelsYUV[0 ] = { v0.y0, v0.u, v0.v };
    pixelsYUV[1 ] = { v0.y1, v0.u, v0.v };
    pixelsYUV[2 ] = { v1.y0, v1.u, v1.v };
    pixelsYUV[3 ] = { v1.y1, v1.u, v1.v };
    pixelsYUV[4 ] = { v0.y2, v0.u, v0.v };
    pixelsYUV[5 ] = { v0.y3, v0.u, v0.v };
    pixelsYUV[6 ] = { v1.y2, v1.u, v1.v };
    pixelsYUV[7 ] = { v1.y3, v1.u, v1.v };
    pixelsYUV[8 ] = { v2.y0, v2.u, v2.v };
    pixelsYUV[9 ] = { v2.y1, v2.u, v2.v };
    pixelsYUV[10] = { v3.y0, v3.u, v3.v };
    pixelsYUV[11] = { v3.y1, v3.u, v3.v };
    pixelsYUV[12] = { v2.y2, v2.u, v2.v };
    pixelsYUV[13] = { v2.y3, v2.u, v2.v };
    pixelsYUV[14] = { v3.y2, v3.u, v3.v };
    pixelsYUV[15] = { v3.y3, v3.u, v3.v };

    // Now convert each pixel to XRGB and save
    uint32_t* const pRow1 = &decoderState.pPixels[ty * VIDEO_WIDTH + lx];
    uint32_t* const pRow2 = pRow1 + VIDEO_WIDTH;
    uint32_t* const pRow3 = pRow1 + VIDEO_WIDTH * 2;
    uint32_t* const pRow4 = pRow1 + VIDEO_WIDTH * 3;

    pRow1[0] = yuvToXRGB8888(pixelsYUV[0]);
    pRow1[1] = yuvToXRGB8888(pixelsYUV[1]);
    pRow1[2] = yuvToXRGB8888(pixelsYUV[2]);
    pRow1[3] = yuvToXRGB8888(pixelsYUV[3]);
    pRow2[0] = yuvToXRGB8888(pixelsYUV[4]);
    pRow2[1] = yuvToXRGB8888(pixelsYUV[5]);
    pRow2[2] = yuvToXRGB8888(pixelsYUV[6]);
    pRow2[3] = yuvToXRGB8888(pixelsYUV[7]);
    pRow3[0] = yuvToXRGB8888(pixelsYUV[8]);
    pRow3[1] = yuvToXRGB8888(pixelsYUV[9]);
    pRow3[2] = yuvToXRGB8888(pixelsYUV[10]);
    pRow3[3] = yuvToXRGB8888(pixelsYUV[11]);
    pRow4[0] = yuvToXRGB8888(pixelsYUV[12]);
    pRow4[1] = yuvToXRGB8888(pixelsYUV[13]);
    pRow4[2] = yuvToXRGB8888(pixelsYUV[14]);
    pRow4[3] = yuvToXRGB8888(pixelsYUV[15]);
}

//----------------------------------------------------------------------------------------------------------------------
// Read CVID chunk: a codebook for a key frame (12 bits per pixel mode)
//----------------------------------------------------------------------------------------------------------------------
static void readCVIDChunk_KF_Codebook_12_Bit(
    ByteInputStream& stream,
    const uint16_t chunkSize,
    VidCodebook& codebook
) THROWS {
    const uint16_t numEntries = chunkSize / (uint16_t) sizeof(VidVec);

    if (numEntries > 256) {
        throw VideoDecodeException();
    }

    VidVec* pCurVec = codebook.vectors;

    for (uint32_t i = 0; i < numEntries; ++i) {
        stream.read(*pCurVec);
        ++pCurVec;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Read CVID chunk: a selectively updated codebook for a delta frame (12 bits per pixel mode)
//----------------------------------------------------------------------------------------------------------------------
static void readCVIDChunk_DF_Codebook_12_Bit(ByteInputStream& stream, VidCodebook& codebook) THROWS {
    if (!stream.hasBytesLeft())
        return;
    
    VidVec* const pVectors = codebook.vectors;

    // Note: need to read a flags vector for every 32 vectors in the code book, hence batches of 32:
    for (uint32_t startVecIdx = 0; startVecIdx < 256; startVecIdx += 32) {
        // First read the flags vector telling whether the next 32 vectors are updated or not
        uint32_t updateFlags = stream.read<uint32_t>();        
        Endian::convertBigToHost(updateFlags);

        // Update whatever vectors are to be updated
        const uint32_t endVecIdx = startVecIdx + 32;

        for (uint32_t vecIdx = startVecIdx; vecIdx < endVecIdx; ++vecIdx) {
            // Is this vector to be updated?
            if ((updateFlags & 0x80000000) != 0) {
                pVectors[vecIdx] = stream.read<VidVec>();
            }

            // Move the next bit up into the top slot for the next loop iteration
            updateFlags <<= 1;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Read CVID chunk: key frame vectors (referencing either V1 or V4 codebooks)
//----------------------------------------------------------------------------------------------------------------------
static void readCVIDChunk_KF_Vectors(VideoDecoderState& decoderState, ByteInputStream& stream) THROWS {
    for (uint32_t batchStartBlockIdx = 0; batchStartBlockIdx < NUM_BLOCKS_PER_FRAME; batchStartBlockIdx += 32) {
        // First read the flags vector telling whether the next 32 blocks are using the V1 or V4 codebook
        uint32_t updateFlags = stream.read<uint32_t>();
        Endian::convertBigToHost(updateFlags);

        // Read the vector indexes for the next 32 blocks:
        const uint32_t endBlockIdx = std::min(batchStartBlockIdx + 32, NUM_BLOCKS_PER_FRAME);

        for (uint32_t blockIdx = batchStartBlockIdx; blockIdx < endBlockIdx; ++blockIdx) {
            const bool bBlockIsV4Coded = ((updateFlags & 0x80000000u) != 0);
            updateFlags <<= 1;

            if (bBlockIsV4Coded) {
                // This block uses the v4 codebook: 4 bytes for 4 V4 codebook vector references
                const uint8_t v0Idx = stream.read<uint8_t>();
                const uint8_t v1Idx = stream.read<uint8_t>();
                const uint8_t v2Idx = stream.read<uint8_t>();
                const uint8_t v3Idx = stream.read<uint8_t>();
                decodePixelBlock(decoderState, blockIdx, decoderState.codebooks[1], v0Idx, v1Idx, v2Idx, v3Idx);
            } else {
                // This block uses the v1 codebook: 1 byte for 1 V1 codebook vector reference
                const uint8_t vIdx = stream.read<uint8_t>();
                decodePixelBlock(decoderState, blockIdx, decoderState.codebooks[0], vIdx, vIdx, vIdx, vIdx);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Read CVID chunk: key frame vectors (in V1 codebook only)
//----------------------------------------------------------------------------------------------------------------------
static void readCVIDChunk_KF_Vectors_V1_Only(VideoDecoderState& decoderState, ByteInputStream& stream) THROWS {
    for (uint32_t blockIdx = 0; blockIdx < NUM_BLOCKS_PER_FRAME; ++blockIdx) {
        // This block uses the v1 codebook: 1 byte for 1 V1 codebook vector reference
        const uint8_t vIdx = stream.read<uint8_t>();
        decodePixelBlock(decoderState, blockIdx, decoderState.codebooks[0], vIdx, vIdx, vIdx, vIdx);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Read CVID chunk: delta frame vectors (selectively updated & referencing either V1 or V4 codebooks)
//----------------------------------------------------------------------------------------------------------------------
static void readCVIDChunk_DF_Vectors(VideoDecoderState& decoderState, ByteInputStream& stream) THROWS {
    // Maintain a bit vector here and read as needed
    uint32_t updateFlags = 0;
    uint32_t updateFlagBitsLeft = 0;

    for (uint32_t blockIdx = 0; blockIdx < NUM_BLOCKS_PER_FRAME; ++blockIdx) {        
        if (updateFlagBitsLeft <= 0) {
            updateFlags = stream.read<uint32_t>();
            Endian::convertBigToHost(updateFlags);
            updateFlagBitsLeft = 32;
        }

        // See if this block is to be updated
        const bool bBlockUpdated = ((updateFlags & 0x80000000u) != 0);
        updateFlags <<= 1;
        updateFlagBitsLeft--;

        if (bBlockUpdated) {
            if (updateFlagBitsLeft <= 0) {
                updateFlags = stream.read<uint32_t>();
                Endian::convertBigToHost(updateFlags);
                updateFlagBitsLeft = 32;
            }

            const bool bBlockIsV4Coded = ((updateFlags & 0x80000000u) != 0);
            updateFlags <<= 1;
            updateFlagBitsLeft--;

            if (bBlockIsV4Coded) {
                // This block uses the v4 codebook: 4 bytes for 4 V4 codebook vector references
                const uint8_t v0Idx = stream.read<uint8_t>();
                const uint8_t v1Idx = stream.read<uint8_t>();
                const uint8_t v2Idx = stream.read<uint8_t>();
                const uint8_t v3Idx = stream.read<uint8_t>();
                decodePixelBlock(decoderState, blockIdx, decoderState.codebooks[1], v0Idx, v1Idx, v2Idx, v3Idx);
            } else {
                // This block uses the v1 codebook: 1 byte for 1 V1 codebook vector reference
                const uint8_t vIdx = stream.read<uint8_t>();
                decodePixelBlock(decoderState, blockIdx, decoderState.codebooks[0], vIdx, vIdx, vIdx, vIdx);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Attempt to read a CVID chunk in the data for the video frame strip
//----------------------------------------------------------------------------------------------------------------------
static void readCVIDChunk(VideoDecoderState& decoderState, ByteInputStream& stream) THROWS {
    // Read the chunk header details and compute the offset in the stream we expect it to end at
    CVIDChunkHeader chunkHeader;
    stream.read(chunkHeader);
    chunkHeader.convertBigToHostEndian();
    
    if (chunkHeader.chunkSize < sizeof(CVIDChunkHeader))
        throw VideoDecodeException();
    
    // Package that up into it's own sub stream and consume the input stream bytes
    const uint16_t chunkSize = chunkHeader.chunkSize - (uint16_t) sizeof(CVIDChunkHeader);

    if (chunkSize > stream.getNumBytesLeft())
        throw VideoDecodeException();
    
    ByteInputStream chunkDataStream(stream.getCurData(), chunkSize);
    stream.consume(chunkSize);
    
    // See what type of chunk we are dealing with
    if (chunkHeader.chunkType == CVID_KF_V4_CODEBOOK_12_BIT) {
        readCVIDChunk_KF_Codebook_12_Bit(chunkDataStream, chunkSize, decoderState.codebooks[1]);
    } 
    else if (chunkHeader.chunkType == CVID_DF_V4_CODEBOOK_12_BIT) {
        readCVIDChunk_DF_Codebook_12_Bit(chunkDataStream, decoderState.codebooks[1]);
    }
    else if (chunkHeader.chunkType == CVID_KF_V1_CODEBOOK_12_BIT) {
        readCVIDChunk_KF_Codebook_12_Bit(chunkDataStream, chunkSize, decoderState.codebooks[0]);
    }
    else if (chunkHeader.chunkType == CVID_DF_V1_CODEBOOK_12_BIT) {
        readCVIDChunk_DF_Codebook_12_Bit(chunkDataStream, decoderState.codebooks[0]);
    }
    else if (chunkHeader.chunkType == CVID_KF_VECTORS) {
        readCVIDChunk_KF_Vectors(decoderState, chunkDataStream);
    }
    else if (chunkHeader.chunkType == CVID_KF_VECTORS_V1_ONLY) {
        readCVIDChunk_KF_Vectors_V1_Only(decoderState, chunkDataStream);
    }
    else if (chunkHeader.chunkType == CVID_DF_VECTORS) {
        readCVIDChunk_DF_Vectors(decoderState, chunkDataStream);
    }
    else {
        // Unknown or unhandled type of chunk, we will just skip over it outside of debug builds...
        // Anything that I'm not handling here basically isn't used in 3DO Doom so no point in writing that code!
        ASSERT(false);
    }
}

bool initVideoDecoder(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    VideoDecoderState& decoderState
) noexcept {
    ASSERT(pStreamFileData || streamFileSize <= 0);

    // Default initialize the decoder state initially
    std::memset(&decoderState, 0, sizeof(VideoDecoderState));

    // Grab the video stream data and abort if that fails
    bool bInitializedSuccessfully = false;
    const bool bGotVideoStreamData = ChunkedStreamFileUtils::getSubStreamData(
        pStreamFileData,
        streamFileSize,
        FourCID("FILM"),
        decoderState.pMovieData,
        decoderState.movieDataSize
    );

    auto cleanupOnFailure = finally([&]() noexcept {
        if (!bInitializedSuccessfully) {
            shutdownVideoDecoder(decoderState);
        }
    });

    if (!bGotVideoStreamData)
        return false;
    
    // There should be at least the header and more data following it.
    // Grab the header and verify that it looks as we expect:
    if (decoderState.movieDataSize <= sizeof(VideoStreamHeader))
        return false;

    VideoStreamHeader header;
    std::memcpy(&header, decoderState.pMovieData, sizeof(VideoStreamHeader));
    header.convertBigToHostEndian();

    if (header.id != FourCID("cvid") && header.id != FourCID("CVID"))
        return false;
    
    if (header.width != VIDEO_WIDTH || header.height != VIDEO_HEIGHT)
        return false;

    // Note: disabled this check because 'logic.cine' appears to have garbage in this field or at least something I don't know
    // how to interpret. Both videos used in 3DO Doom should be 12 FPS anyway so I will just make that assumption...
    #if 0
    if (header.fps != VIDEO_FPS)
        return false;
    #endif
    
    decoderState.curMovieDataOffset += sizeof(VideoStreamHeader);

    // All good if we get to here - alloc a pixel buffer and fill in some other details!
    decoderState.pPixels = new uint32_t[VIDEO_WIDTH * VIDEO_HEIGHT];
    decoderState.totalFrames = header.numFrames;
    bInitializedSuccessfully = true;
    return true;
}

void shutdownVideoDecoder(VideoDecoderState& decoderState) noexcept {
    delete[] decoderState.pPixels;
    decoderState.pPixels = nullptr;
    delete[] decoderState.pMovieData;
    decoderState.pMovieData = nullptr;
}

bool decodeNextVideoFrame(VideoDecoderState& decoderState) noexcept {
    // Sanity checks: decoder must have been initialized
    ASSERT(decoderState.pMovieData);
    ASSERT(decoderState.movieDataSize > 0);

    // Can't read if we are at the end of the data
    const bool bAtMovieEnd = (
        (decoderState.curMovieDataOffset >= decoderState.movieDataSize) ||
        (decoderState.frameNum >= decoderState.totalFrames)
    );

    if (bAtMovieEnd)
        return false;
    
    // Start reading
    ByteInputStream movieData(
        decoderState.pMovieData + decoderState.curMovieDataOffset,
        decoderState.movieDataSize - decoderState.curMovieDataOffset
    );

    try {
        // Read the frame header and verify it is correct
        VideoFrameHeader frameHeader;
        movieData.read(frameHeader);
        frameHeader.convertBigToHostEndian();

        const uint32_t frameSizeRemaining = (
            frameHeader.frameSize
            + sizeof(uint32_t)              // Have to add this on because the frame size read does not include the 'size' field!
            - sizeof(VideoFrameHeader)
        );

        const bool bInvalidFrameHeader = (
            (frameSizeRemaining > movieData.getNumBytesLeft()) ||                           // Bad data length?
            (frameHeader.width != VIDEO_WIDTH || frameHeader.height != VIDEO_HEIGHT) ||     // Unexpected size?
            (frameHeader.numStrips != 1)                                                    // Allow just 1 strip per frame
        );

        if (bInvalidFrameHeader)
            return false;

        // If the frame flags specify to NOT keep the codebooks from previous frames then ensure they are reset here
        if ((frameHeader.flags & FRAME_FLAG_KEEP_CODEBOOKS) == 0) {
            std::memset(decoderState.codebooks, 0, sizeof(decoderState.codebooks));
        }
        
        // Read the header for the single strip that we expect to be in the frame and validate        
        VideoStripHeader stripHeader;
        movieData.read(stripHeader);
        stripHeader.convertBigToHostEndian();

        const bool bInvalidStripHeader = (
            (stripHeader.stripSize < 12) ||
            (stripHeader.stripType != STRIP_TYPE_KEY_FRAME && stripHeader.stripType != STRIP_TYPE_DELTA_FRAME) ||
            (stripHeader.topY != 0) ||
            (stripHeader.leftX != 0) ||
            (stripHeader.height != VIDEO_HEIGHT) ||
            (stripHeader.width != VIDEO_WIDTH)
        );

        if (bInvalidStripHeader)
            return false;
        
        // Read all of the CVID chunks that follow
        uint32_t stripSize = stripHeader.stripSize - sizeof(VideoStripHeader);
        stripSize = std::min(stripSize, movieData.getNumBytesLeft());
        ByteInputStream stripData(movieData.getCurData(), stripSize);

        while (stripData.getNumBytesLeft() > sizeof(CVIDChunkHeader)) {
            readCVIDChunk(decoderState, stripData);
        }

        // Update decoder state to point to the next frame
        ++decoderState.frameNum;
        decoderState.curMovieDataOffset += frameHeader.frameSize + 4;

        // Frame decode succeeded
        return true;
    }
    catch (...) {
        // Failed to decode!
        return false;
    }
}

bool decodeMovieAudio(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    AudioData& audioDataOut
) noexcept {
    ASSERT(pStreamFileData || streamFileSize <= 0);

    // Default initialize the output until we are successful
    audioDataOut = {};

    // Firstly try to grab the audio stream data from the header
    std::byte* pAudioStreamData = nullptr;
    uint32_t audioStreamDataSize = 0;

    auto cleanupAudioStreamData = finally([&]() noexcept {
        delete[] pAudioStreamData;
    });

    const bool bGotAudioStreamData = ChunkedStreamFileUtils::getSubStreamData(
        pStreamFileData,
        streamFileSize,
        FourCID("SNDS"),
        pAudioStreamData,
        audioStreamDataSize
    );

    if (!bGotAudioStreamData)
        return false;

    // Read the header
    if (audioStreamDataSize <= sizeof(AudioStreamHeader))
        return false;

    AudioStreamHeader header;
    std::memcpy(&header, pAudioStreamData, sizeof(AudioStreamHeader));
    header.convertBigToHostEndian();

    // Sanity check the header and make sure it has a supported format
    const bool bInvalidHeader = (
        (header.audioDataSize + sizeof(AudioStreamHeader) > audioStreamDataSize) ||
        (header.numChannels != 1 && header.numChannels != 2) ||
        (header.bitDepth != 8 && header.bitDepth != 16) ||
        (header.sampleRate <= 0)
    );

    if (bInvalidHeader)
        return false;
    
    // The rest is easy, just fill in the audio data struct, copy the audio data and return 'true' for success
    const uint32_t bytesPerSample = header.bitDepth / 8;

    audioDataOut.allocBuffer(header.audioDataSize);
    audioDataOut.bufferSize = header.audioDataSize;
    audioDataOut.numSamples = (header.audioDataSize / bytesPerSample) / header.numChannels;
    audioDataOut.sampleRate = header.sampleRate;
    audioDataOut.numChannels = (uint16_t) header.numChannels;
    audioDataOut.bitDepth = (uint16_t) header.bitDepth;

    std::memcpy(audioDataOut.pBuffer, pAudioStreamData + sizeof(AudioStreamHeader), header.audioDataSize);
    return true;
}

END_NAMESPACE(MovieDecoder)
