#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

struct AudioData;

//----------------------------------------------------------------------------------------------------------------------
// Functionality for decoding the two movies that come with 3DO Doom.
// Contains separate functions to decode both the audio and video data streams.
// The video data is compressed using the old Cinepak (CVID) video codec.
//
// Notes:
//  (1) Constants/assumptions: for simplicity and speed I am hardcoding the video width and height to 280x200.
//      I am also assuming just 1 strip per frame at all times for both movies.
//      Much of this code is probably not of much use elsewhere anyway outside of this project, so that's okay...
//  (2) For simplicity the entireity of audio data is decompressed up front, and the entire of the movie data is 
//      de-chunked up front (but not decompressed). This simplifies the decoding and since the movies are only small
//      this is okay too. Would have to consider more streaming type approaches for larger files however...
//  (3) I don't know the exact details of all of the data structures stored in the movies, hence lots of 'unknown' fields.
//      Some stuff was figured and/or guessed out from reverse engineering and examining the raw data.
//      Enough is known however to decode the movie successfully.
//  (4) For more details on how the Cinepak codec works, see some of the docs accompanying this project.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(MovieDecoder)

//----------------------------------------------------------------------------------------------------------------------
// Video constants:
// Note that for simplicity and speed I am hardcoding the video width and height to 280x200 etc.
// Much of this code is probably not of much use elsewhere anyway outside of this project, so that's okay...
//----------------------------------------------------------------------------------------------------------------------
static constexpr uint32_t VIDEO_WIDTH = 280;
static constexpr uint32_t VIDEO_HEIGHT = 200;
static constexpr uint32_t VIDEO_FPS = 12;
static constexpr uint32_t NUM_BLOCKS_PER_FRAME = (VIDEO_WIDTH * VIDEO_HEIGHT) / 16;     // Each block is 4x4 pixels

//----------------------------------------------------------------------------------------------------------------------
// Represents one vector in the Cinepak 'V1' or 'V4' codebook.
//----------------------------------------------------------------------------------------------------------------------
struct VidVec {
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
struct VidCodebook {
    VidVec vectors[256];
};

//----------------------------------------------------------------------------------------------------------------------
// Holds the vector indices from either that are used for a 4x4 pixel block in the video.
// In the case of a 'V1' coded block, only the 'v1' vector index is used.
//----------------------------------------------------------------------------------------------------------------------
struct VidBlock {
    uint8_t codebookIdx;    // If '0' use the v1 codebook, if '1' use the 'v4' codebook
    uint8_t v0Idx;
    uint8_t v1Idx;
    uint8_t v2Idx;
    uint8_t v3Idx;
};

//----------------------------------------------------------------------------------------------------------------------
// Holds the current state/context for decoding video
//----------------------------------------------------------------------------------------------------------------------
struct VideoDecoderState {
    std::byte*      pMovieData;                     // The de-chunked movie data for the movie
    uint32_t        movieDataSize;                  // Size of the movie data
    uint32_t        curMovieDataOffset;             // Offset to the next movie data to use
    uint32_t        frameNum;                       // What frame we are on, '1' for the first frame and '0' before the first frame has been decoded.
    uint32_t        totalFrames;                    // Total number of frames in the movie.
    VidCodebook     codebooks[2];                   // V1 and V4 codebooks of vectors (in that order)
    VidBlock        blocks[NUM_BLOCKS_PER_FRAME];   // Data for each block in the image and which vectors the block uses
};

//----------------------------------------------------------------------------------------------------------------------
// Initialize the video decoder state before decoding; this must be done before decoding the first frame.
// Returns 'false' on failure to init the video decoder state successfully.
// N.B: 'shutdownMovieDecoder' should also be called after you are done with the movie.
//----------------------------------------------------------------------------------------------------------------------
bool initVideoDecoder(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    VideoDecoderState& decoderState
) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Release resources used by the video decoder state
//----------------------------------------------------------------------------------------------------------------------
void shutdownVideoDecoder(VideoDecoderState& decoderState) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Read the next frame of the video into the given decoder state.
// If this has been done successfully ('true' returned) then the video data is ready to be converted to XRGB.
//----------------------------------------------------------------------------------------------------------------------
bool readNextVideoFrame(VideoDecoderState& decoderState) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Decode the current frame read into the given row major pixel buffer, which is expected to be 280x200.
// The frame data is saved in XRGB8888 format (little endian).
//----------------------------------------------------------------------------------------------------------------------
bool decodeCurrentVideoFrame(VideoDecoderState& decoderState, uint32_t* const pDstPixels) noexcept;

//----------------------------------------------------------------------------------------------------------------------
// Decode the entire audio for a movie stored in the given 3DO stream file and save to the given audio data object.
// Returns 'false' on failure.
//----------------------------------------------------------------------------------------------------------------------
bool decodeMovieAudio(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    AudioData& audioDataOut
) noexcept;

END_NAMESPACE(MovieDecoder)
