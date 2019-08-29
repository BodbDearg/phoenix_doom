#pragma once

#include "AudioData.h"

#include <map>
#include <string>
#include <vector>

//------------------------------------------------------------------------------------------------------------------
// Holds audio pieces loaded from files and allocates each audio piece handles.
// Simple resource manager for all kinds of audio.
//------------------------------------------------------------------------------------------------------------------
class AudioDataMgr {
public:
    // Type for an audio file handle and a handle value returned to signify an invalid handle
    typedef uint32_t Handle;
    static constexpr Handle INVALID_HANDLE = UINT32_MAX;

    AudioDataMgr() noexcept;
    ~AudioDataMgr() noexcept;

    //------------------------------------------------------------------------------------------------------------------
    // Lookup a specified handle for a file.
    // Returns 'INVALID_HANDLE' if the file is not loaded.
    //------------------------------------------------------------------------------------------------------------------
    Handle getFileHandle(const char* const file) const noexcept;

    //------------------------------------------------------------------------------------------------------------------
    // Get the audio data for a particular handle.
    // If the handle is not loaded then a null pointer is returned.
    //------------------------------------------------------------------------------------------------------------------
    const AudioData* getHandleData(const Handle handle) const noexcept;
    inline bool isHandleLoaded(const Handle handle) const noexcept { return (getHandleData(handle) != nullptr); }

    //------------------------------------------------------------------------------------------------------------------
    // Load the specified audio file and return its handle.
    // If it fails then 'INVALID_HANDLE' will be returned.
    // If the audio file is already loaded then the existing handle will be returned.
    //------------------------------------------------------------------------------------------------------------------
    Handle loadFile(const char* const file) noexcept;

    //------------------------------------------------------------------------------------------------------------------
    // Adds audio data that has been generated or loaded externally to the manager.
    // The manager assumes ownership of the data and will free it when done with it.
    //
    // Notes:
    //  (1) If the audio data passed is somehow invalid, then 'INVALID_HANDLE' will be returned.
    //  (2) Even in failure cases the manager maintains ownership over any data pointer.
    //      It may free the data as a result of the failure.
    //  (3) It is undefined behavior to add audio data that is already owned by the manager.
    //------------------------------------------------------------------------------------------------------------------
    Handle addAudioDataWithOwnership(AudioData& data) noexcept;

    // Unload the specified handle or everything
    void unloadHandle(const Handle handle) noexcept;
    void unloadAll() noexcept;

private:
    // Note: using std::less<> to allow for hetrogenous comparison.
    // That way we don't have to construct std::string when given a C-String!
    typedef std::map<std::string, Handle, std::less<>> HandleLut;

    // Represents a single managed piece of audio
    struct AudioEntry {
        AudioData               data;
        HandleLut::iterator     pathToHandleIter;   // For removing the 'path to handle' entry

        inline bool isLoaded() const noexcept {
            return (data.pBuffer != nullptr);
        }
    };

    // Alloc a new audio data handle or re-use a previous one
    Handle allocHandle() noexcept;

    std::vector<AudioEntry>     mAudioEntries;
    std::vector<Handle>         mFreeHandles;
    HandleLut                   mPathToHandle;
};
