#include "ThreeDO.h"

// FIXME: TEMP - REMOVE ME!!
#include "Audio/AudioData.h"
#include "Base/FileUtils.h"
#include "ThreeDO/ChunkedStreamFileUtils.h"
#include "ThreeDO/MovieDecoder.h"
#include <string>

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
        
        uint32_t* const pDecodedFrame = new uint32_t[MovieDecoder::VIDEO_WIDTH * MovieDecoder::VIDEO_HEIGHT];

        while (pVideoDecoder->frameNum < pVideoDecoder->totalFrames) {
            const bool bSuccess = MovieDecoder::readNextVideoFrame(*pVideoDecoder);
            ASSERT(bSuccess);
            MovieDecoder::decodeCurrentVideoFrame(*pVideoDecoder, pDecodedFrame);
            std::string savePath = "E:/Darragh/Desktop/FRAMES/AdioLogo_" + std::to_string(pVideoDecoder->frameNum) + ".data";
            FileUtils::writeDataToFile(savePath.c_str(), (std::byte*) pDecodedFrame, MovieDecoder::VIDEO_WIDTH * MovieDecoder::VIDEO_HEIGHT * sizeof(uint32_t));
        }

        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.audio", audioData.pBuffer, audioData.bufferSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.film", pVideoDecoder->pMovieData, pVideoDecoder->movieDataSize);

        delete[] pDecodedFrame;
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

        while (pVideoDecoder->frameNum < pVideoDecoder->totalFrames) {
            const bool bSuccess = MovieDecoder::readNextVideoFrame(*pVideoDecoder);
            ASSERT(bSuccess);
        }
        
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
