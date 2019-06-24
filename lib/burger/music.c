#include "burger.h"
#include "Audio/Audio.h"

Word MusicVolume = 15;  // Maximum volume
Word SfxVolume = 15;

void PlaySound(Word SoundNum, Word LeftVolume, Word RightVolume) {
    const float leftVol = (float) LeftVolume / 255.0f;
    const float rightVol = (float) RightVolume / 255.0f;
    audioPlaySound(SoundNum, leftVol, rightVol);

    // DC: FIXME: reimplement/replace
    #if 0
        Word Number;
        Word i;
        Word OldPrior;

        if (SoundNum&0x8000) {      /* Stop previous sound */
            SoundNum&=(~0x8000);    /* Clear the flag */
            StopSound(SoundNum);    /* Stop the sound */
        }
        if (!SoundNum ||            /* No sound at all? */
            !AllSamples[SoundNum-1] ||  /* Valid sound loaded? */
            !(SystemState&SfxActive)) { /* Sound enabled? */
            goto WrapUp;
        }

        Number = 0;
        do {
            if (!SampleSound[Number]) {     /* Find an empty sound channel */
                goto Begin;             /* Found it! */
            }
        } while (++Number<VOICECOUNT);

        Number = 0;         /* Get the lowest priority sound */
        do {
            if (!SamplePriority[Number]) {  /* Get the priority */
                break;              /* Priority zero? */
            }
        } while (++Number<(VOICECOUNT-1));  /* Drop out on the last channel */

        StopInstrument(SamplerIns[Number],0);       /* Stop the sound */
        if (SampleAtt[Number]) {                    /* Delete the attachment */
            DetachSample(SampleAtt[Number]);
        }

    Begin:
        SampleSound[Number] = SoundNum;     /* Attach the sound number to this channel */
        SampleAtt[Number] = AttachSample(SamplerIns[Number],AllSamples[SoundNum-1],0);
        TweakKnob(SamplerRateKnob[Number],AllRates[SoundNum-1]);        /* Set to 11 Khz fixed */
        TweakKnob(LeftKnobs[Number],LeftVolume<<6);     /* Set the volume setting */
        TweakKnob(RightKnobs[Number],RightVolume<<6);
        StartInstrument(SamplerIns[Number],0);      /* Begin playing... */
        i = 0;
        OldPrior = SamplePriority[Number];      /* Get the old priority */
        do {
            if (SamplePriority[i]>=OldPrior) {
                --SamplePriority[i];            /* Reduce the priority */
            }
        } while (++i<VOICECOUNT);
        SamplePriority[Number] = (VOICECOUNT-1);        /* Set the higher priority */
    WrapUp:
        LeftVolume = 255;       /* Reset the volume */
        RightVolume = 255;
    #endif
}

void StopSound(Word SoundNum) {
    // DC: FIXME: reimplement/replace
    #if 0
        Word i;
        if (SoundNum) {
            i = 0;
            do {
                if (SampleSound[i] == SoundNum) {       /* Match? */
                    StopInstrument(SamplerIns[i],0);    /* Stop the sound */
                    if (SampleAtt[i]) {
                        DetachSample(SampleAtt[i]);     /* Remove the attachment */
                        SampleAtt[i] = 0;
                    }
                    SampleSound[i] = 0;     /* No sound active */
                }
            } while (++i<VOICECOUNT);       /* All channels checked? */
        }
    #endif
}

void SetSfxVolume(Word NewVolume) {
    // DC: FIXME: reimplement/replace
    #if 0
        Word i;
        Item MyKnob;
        if (NewVolume>=16) {
            NewVolume=15;
        }
        SfxVolume=NewVolume;
        NewVolume = NewVolume*0x888;    /* Convert 0-15 -> 0-0x7FF8 */
        i = 0;
        do {
            MyKnob = GrabKnob(SamplerIns[i],"Amplitude");
            TweakKnob(MyKnob,NewVolume);        /* Set to 11 Khz fixed */
            ReleaseKnob(MyKnob);
        } while (++i<VOICECOUNT);
    #endif
}

void SetMusicVolume(Word NewVolume) {
    // DC: FIXME: reimplement/replace
    #if 0
        Item MyKnob;
        if (NewVolume>=16) {
            NewVolume=15;
        }
        MusicVolume=NewVolume;
        NewVolume = NewVolume*0x888;    /* Convert 0-15 -> 0-0x7FF8 */
        MyKnob = GrabKnob(MusicIns,"Amplitude");
        TweakKnob(MyKnob,NewVolume);        /* Set to 11 Khz fixed */
        ReleaseKnob(MyKnob);
    #endif
}
