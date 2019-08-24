#include "ThreeDO.h"

#include "Base/FileUtils.h"
#include "ThreeDO/ChunkedStreamFileUtils.h"

int main(int argc, char* argv[]) noexcept {
    // FIXME: TEMP - REMOVE ME!!
    {
        std::byte* pStreamFileData = nullptr;
        size_t streamFileSize = 0;

        if (!FileUtils::getContentsOfFile("AdiLogo.cine", pStreamFileData, streamFileSize))
            return -1;
        
        std::byte* pSoundData = nullptr;
        uint32_t soundSize = 0;
        if (!ChunkedStreamFileUtils::getSubStreamData(pStreamFileData, streamFileSize, CSFFourCID::make("SNDS"), pSoundData, soundSize))
            return -1;

        std::byte* pFilmData = nullptr;
        uint32_t filmSize = 0;
        if (!ChunkedStreamFileUtils::getSubStreamData(pStreamFileData, streamFileSize, CSFFourCID::make("FILM"), pFilmData, filmSize))
            return -1;
        
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.audio", pSoundData, soundSize);
        FileUtils::writeDataToFile("E:/Darragh/Desktop/AdioLogo.film", pFilmData, filmSize);

        delete[] pFilmData;
        delete[] pSoundData;
        delete[] pStreamFileData;
    }

    ThreeDOMain();
    return 0;
}
