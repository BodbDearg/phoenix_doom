#include "MovieDecoder.h"

#include "Audio/AudioData.h"
#include "Base/Endian.h"
#include "Base/Finally.h"
#include "ChunkedStreamFileUtils.h"
#include <memory>

BEGIN_NAMESPACE(MovieDecoder)

//----------------------------------------------------------------------------------------------------------------------
// Header for audio data in a movie file. I don't know what all the fields for this are, I only know what I've 
// managed to figure out and guess from reverse engineering...
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

bool decodeMovieAudio(
    const std::byte* const pStreamFileData,
    const uint32_t streamFileSize,
    AudioData& audioDataOut
) noexcept {
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
        CSFFourCID::make("SNDS"),
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
    audioDataOut.numChannels = header.numChannels;
    audioDataOut.bitDepth = header.bitDepth;

    std::memcpy(audioDataOut.pBuffer, pAudioStreamData + sizeof(AudioStreamHeader), header.audioDataSize);
    return true;
}

END_NAMESPACE(MovieDecoder)
