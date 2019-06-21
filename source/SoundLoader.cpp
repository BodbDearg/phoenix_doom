#include "SoundLoader.h"

#include "Endian.h"
#include "Finally.h"
#include "SoundData.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

//--------------------------------------------------------------------------------------------------
// Some ids expected in AIFF-C and AIFF files
//--------------------------------------------------------------------------------------------------
typedef uint32_t IffId;

// Note: this makes the id in such a way that we don't need to byte swap these fields
// from big to little endian!
static inline constexpr IffId makeIffId(const char chars[4]) noexcept {
    return (
        uint32_t(uint8_t(chars[0])) << 0 |
        uint32_t(uint8_t(chars[1])) << 8 |
        uint32_t(uint8_t(chars[2])) << 16 |
        uint32_t(uint8_t(chars[3])) << 24
    );
}

static constexpr IffId ID_FORM = makeIffId("FORM");     // Container chunk for the entire AIFF/AIFF-C file
static constexpr IffId ID_AIFF = makeIffId("AIFF");     // Form type: AIFF
static constexpr IffId ID_AIFC = makeIffId("AIFC");     // Form type: AIFF-C
static constexpr IffId ID_COMM = makeIffId("COMM");     // Common chunk for AIFF/AIFF-C
static constexpr IffId ID_SSND = makeIffId("SSND");     // Sound samples chunk
static constexpr IffId ID_NONE = makeIffId("NONE");     // Compression type: NONE
static constexpr IffId ID_SDX2 = makeIffId("SDX2");     // Compression type: SDX2

//--------------------------------------------------------------------------------------------------
// Exception thrown when there are not enough bytes to read in a memory stream
//--------------------------------------------------------------------------------------------------
class MemStreamException {
};

//--------------------------------------------------------------------------------------------------
// Allows reading from a stream in memory
//--------------------------------------------------------------------------------------------------
class MemStream {
public:
    inline MemStream(const std::byte* const pData, const uint32_t size) noexcept
        : mpData(pData)
        , mSize(size)
        , mCurByteIdx(0)
    {
    }

    inline MemStream(const MemStream& other) noexcept = default;

    inline uint32_t tell() const noexcept {
        return mCurByteIdx;
    }

    inline const std::byte* getCurData() const noexcept {
        return mpData + mCurByteIdx;
    }

    inline uint32_t getNumBytesLeft() const noexcept {
        return mSize - mCurByteIdx;
    }

    inline bool hasBytesLeft(const uint32_t numBytes = 1) const noexcept {
        return (getNumBytesLeft() >= numBytes);
    }

    inline void ensureBytesLeft(const uint32_t numBytes) THROWS {
        if (!hasBytesLeft(numBytes)) {
            throw MemStreamException();
        }        
    }

    inline void consume(const uint32_t numBytes) THROWS {
        ensureBytesLeft(numBytes);
        mCurByteIdx += numBytes;
    }

    template <class T>
    inline void read(T& output) THROWS {
        ensureBytesLeft(sizeof(T));
        std::memcpy(&output, mpData + mCurByteIdx, sizeof(T));
        mCurByteIdx += sizeof(T);
    }

    template <class T>
    inline T read() THROWS {
        ensureBytesLeft(sizeof(T));

        T output;
        std::memcpy(&output, mpData + mCurByteIdx, sizeof(T));
        mCurByteIdx += sizeof(T);

        return output;
    }

    // Aligns the memory stream to the given byte boundary (2, 4, 8 etc.)
    // Note: call is ignored if at the end of the stream!
    inline void align(const uint32_t numBytes) THROWS {
        if (mCurByteIdx >= mSize)
            return;
        
        if (numBytes >= 2) {
            const uint32_t modulus = mCurByteIdx % numBytes;

            if (modulus > 0) {
                const uint32_t numBytesToAlign = numBytes - modulus;
                ensureBytesLeft(numBytesToAlign);
                mCurByteIdx += numBytesToAlign;
            }
        }
    }

private:
    const std::byte* const  mpData;
    const uint32_t          mSize;
    uint32_t                mCurByteIdx;
};

//--------------------------------------------------------------------------------------------------
// Header for a chunk of data as per the 'EA IFF-85' standard and the wrapped data for a chunk.
//--------------------------------------------------------------------------------------------------
struct IffChunkHeader {
    IffId       id;
    uint32_t    dataSize;

    inline void correctEndian() noexcept {
        byteSwapU32(dataSize);
    }
};

struct IffChunk {
    IffId               id;
    uint32_t            dataSize;
    const std::byte*    pData;

    inline MemStream toStream() const {
        return MemStream(pData, dataSize);
    }
};

//--------------------------------------------------------------------------------------------------
// Chunk utilities
//--------------------------------------------------------------------------------------------------
static void readIffChunk(IffChunk& chunk, MemStream& stream) THROWS {
    // Read the header first
    IffChunkHeader header;
    stream.read(header);
    header.correctEndian();

    // Fill in the chunk details and consume the bytes
    chunk.id = header.id;
    chunk.dataSize = header.dataSize;
    chunk.pData = stream.getCurData();
    stream.consume(chunk.dataSize);

    // The data in an IFF chunk is always padded to 2 bytes
    stream.align(2);
}

static const IffChunk* findIffChunkWithId(const IffId id, const std::vector<IffChunk>& chunks) noexcept {
    for (const IffChunk& chunk : chunks) {
        if (chunk.id == id)
            return &chunk;
    }

    return nullptr;
}

static const IffChunk* findAiffFormChunk(const std::vector<IffChunk>& chunks) noexcept {
    for (const IffChunk& chunk : chunks) {        
        if (chunk.id == ID_FORM && chunk.dataSize >= sizeof(uint32_t)) {            
            const IffId formType = ((const uint32_t*) chunk.pData)[0];

            if (formType == ID_AIFF || formType == ID_AIFC)
                return &chunk;
        }
    }

    return nullptr;
}

//----------

# define UnsignedToFloat(u)         (((double)((long)(u - 2147483647L - 1))) + 2147483648.0)
double ConvertFromIeeeExtended(unsigned char* bytes /* LCN */)
{
    double    f;
    int    expon;
    unsigned long hiMant, loMant;
    
    expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
    hiMant    =    ((unsigned long)(bytes[2] & 0xFF) << 24)
            |    ((unsigned long)(bytes[3] & 0xFF) << 16)
            |    ((unsigned long)(bytes[4] & 0xFF) << 8)
            |    ((unsigned long)(bytes[5] & 0xFF));
    loMant    =    ((unsigned long)(bytes[6] & 0xFF) << 24)
            |    ((unsigned long)(bytes[7] & 0xFF) << 16)
            |    ((unsigned long)(bytes[8] & 0xFF) << 8)
            |    ((unsigned long)(bytes[9] & 0xFF));

    if (expon == 0 && hiMant == 0 && loMant == 0) {
        f = 0;
    }
    else {
        if (expon == 0x7FFF) {    /* Infinity or NaN */
            f = HUGE_VAL;
        }
        else {
            expon -= 16383;
            f  = ldexp(UnsignedToFloat(hiMant), expon-=31);
            f += ldexp(UnsignedToFloat(loMant), expon-=32);
        }
    }

    if (bytes[0] & 0x80)
        return -f;
    else
        return f;
}

//--------------------------------------------------------------------------------------------------
// Reads an 80-bit float in big endian format.
// Need to do things this way since MSVC no longer treats 'long double' as 80-bit extended.
//--------------------------------------------------------------------------------------------------
static double readBigEndianExtendedFloat(MemStream& stream) THROWS {
    // Read in reverse order to correct endianness
    uint8_t bytes[10];
    const double double2 = ConvertFromIeeeExtended((unsigned char*)stream.getCurData());
    bytes[9] = stream.read<uint8_t>();
    bytes[8] = stream.read<uint8_t>();
    bytes[7] = stream.read<uint8_t>();
    bytes[6] = stream.read<uint8_t>();
    bytes[5] = stream.read<uint8_t>();
    bytes[4] = stream.read<uint8_t>();
    bytes[3] = stream.read<uint8_t>();
    bytes[2] = stream.read<uint8_t>();
    bytes[1] = stream.read<uint8_t>();
    bytes[0] = stream.read<uint8_t>();

    // 80-bit IEEE is unusual in that it specifies an integer part.
    // Normally '1.fraction' is assumed with floating point but 80-bit uses 'x.fraction' where 'x'
    // is this special integer part bit. Need to read this in order to calculate the exponent correctly:
    const bool integerPartSet = ((bytes[7] & 0x80) != 0);

    // Get whether there is a negative sign and read the exponent
    bool sign = ((bytes[9] & 0x80) != 0);
    const uint16_t unbiasedExponent = (
        ((uint16_t(bytes[9]) & uint16_t(0x7F)) << 8) | uint16_t(bytes[8])) + 
        (integerPartSet ? 0 : -1);
    
    const int16_t exponent = int16_t(unbiasedExponent) - int16_t(0x3FFFU);

    // Read the fractional bits (63-bits)
    const uint64_t fraction = (
        (uint64_t(bytes[7] & 0x7F) << 57) |
        (uint64_t(bytes[6]) << 49) |
        (uint64_t(bytes[5]) << 41) |
        (uint64_t(bytes[4]) << 33) |
        (uint64_t(bytes[3]) << 25) |
        (uint64_t(bytes[2]) << 17) |
        (uint64_t(bytes[1]) << 9) |
        (uint64_t(bytes[0]) << 1)
    );

    // Exponent range check! - if it's outside of the precision of a double then return infinity or 0
    if (exponent > 1023) {
        if (sign) {
            return std::numeric_limits<double>::infinity();
        }
        else {
            return -std::numeric_limits<double>::infinity();
        }
    }
    else if (exponent < -1022) {
        return 0.0;
    }

    // Makeup the double in binary format
    const uint64_t doubleBits = (
        ((sign) ? uint64_t(0x8000000000000000) : uint64_t(0)) |
        ((uint64_t(exponent + 1023) & uint64_t(0x7FF)) << 52) |
        (fraction >> 12)
    );

    const double doubleVal = reinterpret_cast<const double&>(doubleBits);    
    return doubleVal;
}

//--------------------------------------------------------------------------------------------------
// Reads the contents of the FORM chunk
//--------------------------------------------------------------------------------------------------
static bool readFormChunk(const IffChunk& formChunk, SoundData& soundData) THROWS {
    // Validate and read form type firstly
    MemStream formStream = formChunk.toStream();
    const IffId formType = formStream.read<IffId>();

    if (formType != ID_AIFF && formType != ID_AIFC)
        return false;
    
    const bool bIsAifc = (formType == ID_AIFC);

    // Read sub-chunks
    std::vector<IffChunk> chunks;

    while (formStream.hasBytesLeft()) {
        IffChunk& chunk = chunks.emplace_back();
        readIffChunk(chunk, formStream);
    }

    // Find the common chunk and the sound data chunk
    const IffChunk* const pCommonChunk = findIffChunkWithId(ID_COMM, chunks);
    const IffChunk* const pSoundChunk = findIffChunkWithId(ID_SSND, chunks);

    if (!pCommonChunk || !pSoundChunk)
        return false;
    
    // Read the file format info in the common chunk
    uint16_t numChannels;
    uint32_t numSamples;
    uint16_t bitDepth;
    uint32_t sampleRate;
    IffId compressionType;

    {
        MemStream commonStream = pCommonChunk->toStream();

        numChannels = byteSwappedU16(commonStream.read<uint16_t>());
        numSamples = byteSwappedU32(commonStream.read<uint32_t>());
        bitDepth = byteSwappedU16(commonStream.read<uint16_t>());
        sampleRate = (uint32_t) readBigEndianExtendedFloat(commonStream);

        // Note: if the format is AIFF-C then the common chunk is extended to include compression info.
        // If the format is AIFF then there is no compression.
        if (bIsAifc) {
            compressionType = commonStream.read<uint32_t>();
        }
        else {
            compressionType = ID_NONE;
        }
    }

    // Sanity check some of the data - only supporting certain formats
    if (numChannels != 1 && numChannels != 2)
        return false;

    if (bitDepth != 8 && bitDepth != 16)
        return false;
    
    if (sampleRate <= 0)
        return false;

    // FIXME: Support the 3DO's SDX2 compression (required for music)
    if (compressionType != ID_NONE)
        return false;

    // Read the sound data
    {
        // Ensure all the data is there
        MemStream soundStream = pSoundChunk->toStream();
        const uint32_t bytesPerSample = (bitDepth == 8) ? 1 : 2;
        const uint32_t soundDataSize = bytesPerSample * numSamples * numChannels;
        soundStream.ensureBytesLeft(soundDataSize);

        // Alloc the sound buffer and save it in there
        soundData.allocBuffer(soundDataSize);
        std::memcpy(soundData.pBuffer, soundStream.getCurData(), soundDataSize);

        // Save other sound properties
        soundData.numSamples = numSamples;
        soundData.sampleRate = sampleRate;
        soundData.numChannels = numChannels;
        soundData.bitDepth = bitDepth;
    }

    // All good if we reach here ok
    return true;
}

bool SoundLoader::loadFromFile(const char* const filePath, SoundData& soundData) noexcept {
    // Open the file firstly
    ASSERT(filePath);
    FILE* pFile = std::fopen(filePath, "rb");

    if (!pFile)
        return false;
    
    auto closeFile = finally([&]() noexcept {
        std::fclose(pFile);
    });

    // Figure out what size it is
    if (std::fseek(pFile, 0, SEEK_END) != 0)
        return false;

    const long fileSize = std::ftell(pFile);

    if (fileSize <= 0 || fileSize >= INT32_MAX)
        return false;
    
    // Read the entire file
    std::unique_ptr<std::byte[]> fileBytes(new std::byte[fileSize]);

    if (std::fseek(pFile, 0, SEEK_SET) != 0)
        return false;

    if (std::fread(fileBytes.get(), (uint32_t) fileSize, 1, pFile) != 1)
        return false;

    // Now load the sound from the file's buffer
    return loadFromBuffer(fileBytes.get(), (uint32_t) fileSize, soundData);
}

bool SoundLoader::loadFromBuffer(const std::byte* const pBuffer, const uint32_t bufferSize, SoundData& soundData) noexcept {
    MemStream stream(pBuffer, bufferSize);

    try {
        // Read all the root chunks in the file firstly
        std::vector<IffChunk> rootChunks;

        while (stream.hasBytesLeft()) {
            IffChunk& chunk = rootChunks.emplace_back();
            readIffChunk(chunk, stream);
        }

        // Find the 'FORM' chunk that contains sound data
        const IffChunk* const formChunk = findAiffFormChunk(rootChunks);

        if (!formChunk) {
            return false;
        }

        return readFormChunk(*formChunk, soundData);
    }
    catch (MemStreamException) {
        soundData.clear();
        return false;
    }
}
