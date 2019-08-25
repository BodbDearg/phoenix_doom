#pragma once

#include "Base/Macros.h"
#include <cstddef>
#include <cstdint>

struct AudioData;

//----------------------------------------------------------------------------------------------------------------------
// Functionality for decoding the two movies that come with 3DO Doom.
// Separate functions to decode both the audio and video data streams.
// The video data is compressed using the old Cinepak (CVID) video codec.
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(MovieDecoder)

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
