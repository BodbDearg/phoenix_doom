#include "ThreeDO.h"

// FIXME: TEMP - REMOVE ME!!
#include "Audio/AudioData.h"
#include "Base/FileUtils.h"
#include "ThreeDO/ChunkedStreamFileUtils.h"
#include "ThreeDO/MovieDecoder.h"

int main(int argc, char* argv[]) noexcept {
    // FIXME: TEMP - REMOVE ME!!
    {
        std::byte* pStreamFileData = nullptr;
        size_t streamFileSize = 0;

        if (!FileUtils::getContentsOfFile("AdiLogo.cine", pStreamFileData, streamFileSize))
            return -1;
        
        AudioData audioData;
        if (!MovieDecoder::decodeMovieAudio(pStreamFileData, streamFileSize, audioData))
            return -1;

        std::byte* pFilmData = nullptr;
        uint32_t filmSize = 0;
        if (!ChunkedStreamFileUtils::getSubStreamData(pStreamFileData, streamFileSize, CSFFourCID::make("FILM"), pFilmData, filmSize))
            return -1;
        
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.audio", audioData.pBuffer, audioData.bufferSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.film", pFilmData, filmSize);

        delete[] pFilmData;
        audioData.freeBuffer();
        delete[] pStreamFileData;
    }

    ThreeDOMain();
    return 0;
}
