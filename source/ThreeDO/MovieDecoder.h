#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

struct AudioData;

//----------------------------------------------------------------------------------------------------------------------
// Functionality for decoding the two movies that come with 3DO Doom.
// Separate functions to decode both the audio and video data streams.
// The video data is compressed using the old Cinepak (CVID) video codec.
//
// For more details on how the Cinepak format works, see some of the docs accompanying this project.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(MovieDecoder)

//----------------------------------------------------------------------------------------------------------------------
// Video constants:
// Note that for simplicity and speed I am hardcoding the video width and height to 280x200.
// Much of this code is probably not of much use elsewhere anyway outside of this project, so that's okay...
//----------------------------------------------------------------------------------------------------------------------
static constexpr uint32_t VIDEO_WIDTH = 280;
static constexpr uint32_t VIDEO_HEIGHT = 200;
static constexpr uint32_t NUM_BLOCKS_PER_FRAME = (VIDEO_WIDTH * VIDEO_HEIGHT) / 16;     // Each block is 4x4 pixels

//----------------------------------------------------------------------------------------------------------------------
// Represents one vector in the Cinepak 'V1' or 'V4' codebook.
//----------------------------------------------------------------------------------------------------------------------
struct CodebookVec {
    uint8_t y0;     // Luminance values
    uint8_t y1;
    uint8_t y2;
    uint8_t y3;
    uint8_t u;      // Chrominance (color) values
    uint8_t v;
};

//----------------------------------------------------------------------------------------------------------------------
// A series of vectors that is looked up to decode a frame.
// The list of vectors is indexed using a single byte value, so there are at most 256 values.
//----------------------------------------------------------------------------------------------------------------------
struct Codebook {
    CodebookVec vectors[256];
};

//----------------------------------------------------------------------------------------------------------------------
// Holds the vector indices from either that are used for a 4x4 pixel block.
// In the case of a 'V1' coded block, only the 'v1' vector index is used.
//----------------------------------------------------------------------------------------------------------------------
struct Block {
    uint8_t codebookIdx;    // If '0' use the v1 codebook, if '1' use the 'v4' codebook
    uint8_t v1;
    uint8_t v2;
    uint8_t v3;
    uint8_t v4;
};

//----------------------------------------------------------------------------------------------------------------------
// Holds the current state/context for decoding video
//----------------------------------------------------------------------------------------------------------------------
struct DecoderState {
    std::byte*      pCurData;                           // Pointer to the next frame data to use
    size_t          bytesRemaining;                     // Number of bytes remaining in the frame data
    uint32_t        frameNum;                           // What frame we are on, '1' for the first frame and '0' before the first frame has been decoded.
    uint32_t        totalFrames;                        // Total number of frames in the movie.
    Codebook        codebooks[2];                       // V1 and V4 codebooks of vectors (in that order)
    Block           blocks[NUM_BLOCKS_PER_FRAME];       // Data for each block in the image and which vectors the block uses
};

//----------------------------------------------------------------------------------------------------------------------
// Decode the audio for a movie stored in the given 3DO stream file and save to the given audio data object.
// Returns 'false' on failure.
//----------------------------------------------------------------------------------------------------------------------
bool decodeMovieAudio(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    AudioData& audioDataOut
) noexcept;

END_NAMESPACE(MovieDecoder)
