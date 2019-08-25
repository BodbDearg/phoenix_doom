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
        
        MovieDecoder::VideoDecoderState* const pVideoDecoder = new MovieDecoder::VideoDecoderState();
        if (!MovieDecoder::initVideoDecoder(pStreamFileData, streamFileSize, *pVideoDecoder))
            return -1;
                
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.audio", audioData.pBuffer, audioData.bufferSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.film", pVideoDecoder->pMovieData, pVideoDecoder->movieDataSize);

        MovieDecoder::shutdownVideoDecoder(*pVideoDecoder);
        audioData.freeBuffer();
        delete[] pStreamFileData;
        delete pVideoDecoder;
    }

    {
        std::byte* pStreamFileData = nullptr;
        size_t streamFileSize = 0;

        if (!FileUtils::getContentsOfFile("logic.cine", pStreamFileData, streamFileSize))
            return -1;
        
        AudioData audioData;
        if (!MovieDecoder::decodeMovieAudio(pStreamFileData, streamFileSize, audioData))
            return -1;

        MovieDecoder::VideoDecoderState* const pVideoDecoder = new MovieDecoder::VideoDecoderState();
        if (!MovieDecoder::initVideoDecoder(pStreamFileData, streamFileSize, *pVideoDecoder))
            return -1;
        
        FileUtils::writeDataToFile("E:/Darragh/Desktop/logic.audio", audioData.pBuffer, audioData.bufferSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/logic.film", pVideoDecoder->pMovieData, pVideoDecoder->movieDataSize);

        MovieDecoder::shutdownVideoDecoder(*pVideoDecoder);
        audioData.freeBuffer();
        delete[] pStreamFileData;
        delete pVideoDecoder;
    }

    ThreeDOMain();
    return 0;
}
