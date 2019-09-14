#include "IntroMovies.h"

#include "Audio/Audio.h"
#include "Audio/AudioDataMgr.h"
#include "Audio/AudioSystem.h"
#include "Base/Finally.h"
#include "Base/Input.h"
#include "Base/Tables.h"
#include "Game/Controls.h"
#include "Game/Data.h"
#include "Game/DoomMain.h"
#include "Game/GameDataFS.h"
#include "GFX/Blit.h"
#include "GFX/Video.h"
#include "ThreeDO/MovieDecoder.h"

BEGIN_NAMESPACE(IntroMovies)

static AudioDataMgr::Handle                 gMovieAudioDataHandle   = AudioDataMgr::INVALID_HANDLE;
static uint32_t                             gMovieAudioSampleRate   = 0;
static AudioSystem::VoiceIdx                gMovieAudioVoice        = AudioSystem::INVALID_VOICE_IDX;
static MovieDecoder::VideoDecoderState*     gpVidDecoderState       = nullptr;

static void shutdownMovie() noexcept {
    if (gMovieAudioVoice != AudioSystem::INVALID_VOICE_IDX) {
        Audio::getSoundAudioSystem().stopVoice(gMovieAudioVoice);
        gMovieAudioVoice = AudioSystem::INVALID_VOICE_IDX;
    }

    if (gMovieAudioDataHandle != AudioDataMgr::INVALID_HANDLE) {
        Audio::getAudioDataMgr().unloadHandle(gMovieAudioDataHandle);
        gMovieAudioDataHandle = AudioDataMgr::INVALID_HANDLE;
    }

    gMovieAudioSampleRate = 0;
    delete gpVidDecoderState;
    gpVidDecoderState = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
// Loads the given movie and it's audio.
// Starts playing the audio.
//----------------------------------------------------------------------------------------------------------------------
static void startupMovie(const char* path) noexcept {
    // Ensure the screen is clear before we display the movie
    Video::clearScreen(0, 0, 0);

    // Don't do any screen wipes for movies
    gDoWipe = false;

    // Try to load the movie file into memory
    std::byte* pMovieFileData = nullptr;
    size_t movieFileSize = 0;

    auto cleanupMovieFileData = finally([&]() {
        delete[] pMovieFileData;
    });

    if (!GameDataFS::getContentsOfFile(path, pMovieFileData, movieFileSize))
        return;

    // Initialize the decoder: destroy if failed
    gpVidDecoderState = new MovieDecoder::VideoDecoderState();

    if (!MovieDecoder::initVideoDecoder(pMovieFileData, (uint32_t) movieFileSize, *gpVidDecoderState)) {
        shutdownMovie();
        return;
    }

    // Decode the first frame of the video
    if (!MovieDecoder::decodeNextVideoFrame(*gpVidDecoderState)) {
        shutdownMovie();
        return;
    }

    // Load the audio for the movie and start playing it
    AudioData audioData;

    if (!MovieDecoder::decodeMovieAudio(pMovieFileData, (uint32_t) movieFileSize, audioData)) {
        shutdownMovie();
        return;
    }

    gMovieAudioDataHandle = Audio::getAudioDataMgr().addAudioDataWithOwnership(audioData);

    if (gMovieAudioDataHandle == AudioDataMgr::INVALID_HANDLE) {
        shutdownMovie();
        return;
    }

    gMovieAudioSampleRate = audioData.sampleRate;
    gMovieAudioVoice = Audio::getSoundAudioSystem().play(gMovieAudioDataHandle);

    if (gMovieAudioVoice == AudioSystem::INVALID_VOICE_IDX) {
        shutdownMovie();
        return;
    }
}

static void onAdiMovieStarting() noexcept {
    startupMovie("AdiLogo.cine");
}

static void onLogicwareMovieStarting() noexcept {
    startupMovie("logic.cine");
}

static void onMovieStopping() noexcept {
    shutdownMovie();
}

static gameaction_e updateMovie() noexcept {
    // If there is no movie playing then we are done!
    if (!gpVidDecoderState)
        return ga_completed;
    
    // If the sound is done playing then we are done
    const AudioVoice voice = Audio::getSoundAudioSystem().getVoiceState(gMovieAudioVoice);
    
    if (voice.state == AudioVoice::State::STOPPED)
        return ga_completed;
    
    // Skip pressed?
    if (MENU_ACTION_ENDED(OK) || MENU_ACTION_ENDED(BACK))
        return ga_exitdemo;
    
    // Figure out what frame in the video the audio data is at
    const double audioTimeInSeconds = (double) voice.curSample / (double) gMovieAudioSampleRate;
    const double tgtVidFrameNumFrac = audioTimeInSeconds * (double) MovieDecoder::VIDEO_FPS;
    const uint32_t tgtVidFrameNum = (uint32_t) tgtVidFrameNumFrac;

    // If the video is behind then decode to catch up
    while (gpVidDecoderState->frameNum < tgtVidFrameNum) {
        if (!MovieDecoder::decodeNextVideoFrame(*gpVidDecoderState))
            break;
    }

    return ga_nothing;
}

static void drawMovie(const bool bPresent, const bool bSaveFrameBuffer) noexcept {
    if (!gpVidDecoderState)
        return;
    
    // Figure out the unscaled x and y position of the movie
    constexpr float MOVIE_X = (float)(Video::REFERENCE_SCREEN_WIDTH - MovieDecoder::VIDEO_WIDTH) * 0.5f;
    constexpr float MOVIE_Y = (float)(Video::REFERENCE_SCREEN_HEIGHT - MovieDecoder::VIDEO_HEIGHT) * 0.5f;

    // Now figure out the scaled position and size of the movie
    const float xScaled = (float) MOVIE_X * gScaleFactor;
    const float yScaled = (float) MOVIE_Y * gScaleFactor;
    const float wScaled = (float) MovieDecoder::VIDEO_WIDTH * gScaleFactor;
    const float hScaled = (float) MovieDecoder::VIDEO_HEIGHT * gScaleFactor;

    // Now blit to the screen
    Blit::blitSprite<
        Blit::BCF_H_CLIP |
        Blit::BCF_V_CLIP
    >(
        gpVidDecoderState->pPixels,
        MovieDecoder::VIDEO_WIDTH,
        MovieDecoder::VIDEO_HEIGHT,
        0.0f,
        0.0f,
        (float) MovieDecoder::VIDEO_WIDTH,
        (float) MovieDecoder::VIDEO_HEIGHT,
        Video::gpFrameBuffer,
        Video::gScreenWidth,
        Video::gScreenHeight,
        Video::gScreenWidth,
        xScaled,
        yScaled,
        wScaled,
        hScaled
    );

    Video::endFrame(bPresent, bSaveFrameBuffer);
}

void run() noexcept {
    if (Input::isQuitRequested())
        return;

    if (RunGameLoop(onLogicwareMovieStarting, onMovieStopping, updateMovie, drawMovie) == ga_quit)
        return;
    
    RunGameLoop(onAdiMovieStarting, onMovieStopping, updateMovie, drawMovie);
}

END_NAMESPACE(IntroMovies)
