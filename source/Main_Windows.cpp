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
        
        MovieDecoder::VideoDecoderState vidDecoder;
        if (!MovieDecoder::initVideoDecoder(pStreamFileData, streamFileSize, vidDecoder))
            return -1;

        while (vidDecoder.frameNum < vidDecoder.totalFrames) {
            const bool bSuccess = MovieDecoder::decodeNextVideoFrame(vidDecoder);
            ASSERT(bSuccess);
            std::string savePath = "E:/Darragh/Desktop/FRAMES/AdioLogo_" + std::to_string(vidDecoder.frameNum) + ".data";
            FileUtils::writeDataToFile(savePath.c_str(), (std::byte*) vidDecoder.pPixels, MovieDecoder::VIDEO_WIDTH * MovieDecoder::VIDEO_HEIGHT * sizeof(uint32_t));
        }
        
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.audio", audioData.pBuffer, audioData.bufferSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.film", vidDecoder.pMovieData, vidDecoder.movieDataSize);
        
        MovieDecoder::shutdownVideoDecoder(vidDecoder);
        audioData.freeBuffer();
        delete[] pStreamFileData;
    }
    {
        std::byte* pStreamFileData = nullptr;
        size_t streamFileSize = 0;

        if (!FileUtils::getContentsOfFile("logic.cine", pStreamFileData, streamFileSize))
            return -1;
        
        AudioData audioData;
        if (!MovieDecoder::decodeMovieAudio(pStreamFileData, streamFileSize, audioData))
            return -1;

        MovieDecoder::VideoDecoderState vidDecoder;
        if (!MovieDecoder::initVideoDecoder(pStreamFileData, streamFileSize, vidDecoder))
            return -1;

        while (vidDecoder.frameNum < vidDecoder.totalFrames) {
            const bool bSuccess = MovieDecoder::decodeNextVideoFrame(vidDecoder);
            ASSERT(bSuccess);
            std::string savePath = "E:/Darragh/Desktop/FRAMES/Logic_" + std::to_string(vidDecoder.frameNum) + ".data";
            FileUtils::writeDataToFile(savePath.c_str(), (std::byte*) vidDecoder.pPixels, MovieDecoder::VIDEO_WIDTH * MovieDecoder::VIDEO_HEIGHT * sizeof(uint32_t));
        }
        FileUtils::writeDataToFile("E:/Darragh/Desktop/logic.audio", audioData.pBuffer, audioData.bufferSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/logic.film", vidDecoder.pMovieData, vidDecoder.movieDataSize);
        MovieDecoder::shutdownVideoDecoder(vidDecoder);
        audioData.freeBuffer();
        delete[] pStreamFileData;
    }

    ThreeDOMain();
    return 0;
}
