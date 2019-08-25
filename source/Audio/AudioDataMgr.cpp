#include "AudioDataMgr.h"

#include "AudioLoader.h"

AudioDataMgr::AudioDataMgr() noexcept
    : mAudioEntries()
    , mFreeHandles()
    , mPathToHandle()
{
}

AudioDataMgr::~AudioDataMgr() noexcept {
    unloadAll();
}

AudioDataMgr::Handle AudioDataMgr::getFileHandle(const char* const file) const noexcept {
    ASSERT(file);
    const auto iter = mPathToHandle.find(file);
    return (iter != mPathToHandle.end()) ? iter->second : INVALID_HANDLE;
}

const AudioData* AudioDataMgr::getHandleData(const Handle handle) const noexcept {
    if (handle < mAudioEntries.size()) {
        const AudioEntry& entry = mAudioEntries[handle];
        return (entry.isLoaded()) ? &entry.data : nullptr;
    }

    return nullptr;
}

AudioDataMgr::Handle AudioDataMgr::loadFile(const char* const file) noexcept {
    // If the file is already loaded we can just early out
    ASSERT(file);
    const Handle fileHandle = getFileHandle(file);

    if (fileHandle != INVALID_HANDLE)
        return fileHandle;

    // Try to load the audio data
    AudioData audioData;

    if (!AudioLoader::loadFromFile(file, audioData))
        return INVALID_HANDLE;

    // Otherwise try to allocate a handle for the file
    uint32_t handle;

    if (!mFreeHandles.empty()) {
        handle = mFreeHandles.back();
        mFreeHandles.pop_back();
    } else {
        handle = (uint32_t) mAudioEntries.size();
        mAudioEntries.emplace_back();
    }

    // Add path to handle lut entry
    HandleLut::iterator pathToHandleIter = mPathToHandle.insert({ file, handle }).first;

    // Save the entry details and return the loaded handle
    AudioEntry& audioEntry = mAudioEntries[handle];
    audioEntry.data = audioData;
    audioEntry.pathToHandleIter = pathToHandleIter;

    return handle;
}

void AudioDataMgr::unloadHandle(const Handle handle) noexcept {
    if (handle < mAudioEntries.size()) {
        AudioEntry& entry = mAudioEntries[handle];

        if (entry.isLoaded()) {
            entry.data.clear();
            mPathToHandle.erase(entry.pathToHandleIter);
            entry.pathToHandleIter = {};
            mFreeHandles.emplace_back(handle);
        }
    }
}

void AudioDataMgr::unloadAll() noexcept {
    for (AudioEntry& entry : mAudioEntries) {
        if (entry.isLoaded()) {
            entry.data.clear();
        }
    }

    mAudioEntries.clear();
    mFreeHandles.clear();
    mPathToHandle.clear();
}
