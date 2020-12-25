#include "sonic_field.h"
#include <fstream>

// TODO: Split this into two classes not one for read and write.
// Remove/rationalise getLE and putLE.
// Correct use of uint64_t and int64_t to 32 corrected 32 bit versions.
namespace sonic_field
{

constexpr int32_t BUFFER_SIZE    = 4096;
constexpr int32_t FMT_CHUNK_ID   = 0x20746D66;
constexpr int32_t DATA_CHUNK_ID  = 0x61746164;
constexpr int32_t RIFF_CHUNK_ID  = 0x46464952;
constexpr int32_t RIFF_TYPE_ID   = 0x45564157;

class wav_file
{
protected:

    std::ifstream m_istream;
    std::string   m_file;
    uint16_t      m_bytes_per_sample;
    uint64_t      m_num_frames;
    bool          m_word_align_adjust;
    uint32_t      m_sample_rate;
    uint16_t      m_block_align;
    uint16_t      m_valid_bits;
    char          m_buffer[BUFFER_SIZE];
    uint32_t      m_buffer_pointer;
    uint32_t      m_bytes_read;
    uint64_t      m_frame_counter;

    // TODO: Optimize where direct copy would work for these two.
    void putLE(const uint32_t valIn, char* buffer, int32_t posIn, const int32_t numBytes)
    {
        long val = valIn;
        int32_t pos = posIn;
        for (int32_t b = 0; b < numBytes; ++b)
        {
            buffer[pos] = val & 0xFF;
            val >>= 8;
            ++pos;
        }
    }

    static uint32_t getLE(const char* buffer, const int32_t posIn, const int32_t numBytesIn)
    {
        int32_t numBytes = numBytesIn - 1;
        int32_t pos = posIn + numBytes;

        uint32_t val = buffer[pos] & 0xFF;
        for (int32_t b = 0; b < numBytes; b++)
            val = (val << 8) + (buffer[--pos] & 0xFF);

        return val;
    }

    wav_file(const std::string& file, uint64_t numFrames, uint32_t sampleRate) :
        m_file{ file },
        m_bytes_per_sample{ 4 },
        m_num_frames{ numFrames },
        m_sample_rate{ sampleRate },
        m_block_align{ m_bytes_per_sample },
        m_valid_bits{ 32 }
    {}

    wav_file() {}

public:

    decltype(m_num_frames) getNumFrames()
    {
        return m_num_frames;
    }

    decltype(m_num_frames) getFramesRemaining()
    {
        return m_num_frames - m_frame_counter;
    }

    decltype(m_sample_rate) getSampleRate()
    {
        return m_sample_rate;
    }

    decltype(m_valid_bits) getValidBits()
    {
        return m_valid_bits;
    }
};

class wavsignal_writer : public wav_file
{
    std::ofstream m_ostream;

public:
    wavsignal_writer(const std::string& file, uint32_t numFrames, uint32_t sampleRate) :
        wav_file{ file, numFrames, sampleRate },
        m_ostream{ file, std::ios_base::binary }
    {
        // Calculate the chunk sizes
        uint32_t  dataChunkSize = m_block_align * numFrames;
        uint32_t mainChunkSize = 4 + // Riff Type
            8 + // Format ID and size
            16 + // Format data
            8 + // Data ID and size
            dataChunkSize;

        // Chunks must be word aligned, so if odd number of audio data bytes
        // adjust the main chunk size
        if (dataChunkSize % 2 == 1)
        {
            mainChunkSize += 1;
            m_word_align_adjust = true;
        }
        else
        {
            m_word_align_adjust = false;
        }

        // Set the main chunk size
        putLE(RIFF_CHUNK_ID, m_buffer, 0, 4);
        putLE(mainChunkSize, m_buffer, 4, 4);
        putLE(RIFF_TYPE_ID, m_buffer, 8, 4);

        // Write out the header
        m_ostream.write(m_buffer, 12);

        // Put format data in buffer
        long averageBytesPerSecond = sampleRate * m_block_align;

        putLE(FMT_CHUNK_ID, m_buffer, 0, 4); // Chunk ID
        putLE(16, m_buffer, 4, 4); // Chunk Data Size
        putLE(1, m_buffer, 8, 2); // Compression Code (Uncompressed)
        putLE(1, m_buffer, 10, 2); // Number of channels
        putLE(sampleRate, m_buffer, 12, 4); // Sample Rate
        putLE(averageBytesPerSecond, m_buffer, 16, 4); // Average Bytes
                                                                // Per Second
        putLE(m_block_align, m_buffer, 20, 2); // Block Align
        putLE(32, m_buffer, 22, 2); // Valid Bits

        // Write Format Chunk
        m_ostream.write(m_buffer, 24);

        // Start Data Chunk
        putLE(DATA_CHUNK_ID, m_buffer, 0, 4); // Chunk ID
        putLE(dataChunkSize, m_buffer, 4, 4); // Chunk Data Size

        // Write Format Chunk
        m_ostream.write(m_buffer, 8);

        // Finally, set the IO State
        m_buffer_pointer = 0;
        m_bytes_read = 0;
        m_frame_counter = 0;
    }

    void writeSample(uint64_t valIn)
    {
        auto cval = valIn;
        for (uint64_t b = 0; b < m_bytes_per_sample; b++)
        {
            if (m_buffer_pointer == BUFFER_SIZE)
            {
                m_ostream.write(m_buffer, BUFFER_SIZE);
                m_buffer_pointer = 0;
            }

            m_buffer[m_buffer_pointer] = char(cval & 0xFF);
            cval >>= 8;
            m_buffer_pointer++;
        }
    }

    void flush()
    {
        m_ostream.write(m_buffer, m_buffer_pointer);
        m_ostream.flush();
        m_buffer_pointer = 0;
    }
};

class wav_reader : public wav_file
{
    std::ifstream m_istream;

    void check_istream()
    {
        if (!m_istream) SF_THROW(std::out_of_range("End of file or could not open: " + m_file));
    }

public:
    explicit wav_reader(const std::string& file):
        m_istream{file}
    {
        m_file = file;
        m_istream = std::ifstream{file};

        // Read the first 12 bytes of the file
        m_istream.read(m_buffer, 12);
        check_istream();

        // Extract parts from the header
        auto riffChunkID = getLE(m_buffer, 0, 4);
        auto chunkSize = getLE(m_buffer, 4, 4);
        auto riffTypeID = getLE(m_buffer, 8, 4);

        // Check the header bytes contains the correct signature
        if (riffChunkID != RIFF_CHUNK_ID) SF_THROW(std::logic_error("Invalid Wav Header data, incorrect riff chunk ID"));
        if (riffTypeID != RIFF_TYPE_ID) SF_THROW(std::logic_error("Invalid Wav Header data, incorrect riff type ID"));

        // Check that the file size matches the number of bytes listed in header
        if (file.length() != chunkSize + 8)
        {
            SF_THROW(std::logic_error{"Header chunk size (" + std::to_string(chunkSize)
                + ") does not match file size (" + std::to_string(file.length()) + ")"});
        }

        auto foundFormat{false};
        auto foundData{false};

        // Search for the Format and Data Chunks
        while (true)
        {
            // Read the first 8 bytes of the chunk (ID and chunk size)
            m_istream.read(m_buffer, 8);
            check_istream();

            // Extract the chunk ID and Size
            long chunkID = getLE(m_buffer, 0, 4);
            chunkSize = getLE(m_buffer, 4, 4);

            // Word align the chunk size
            // chunkSize specifies the number of bytes holding data. However,
            // the data should be word aligned (2 bytes) so we need to calculate
            // the actual number of bytes in the chunk
            auto numChunkBytes = (chunkSize % 2 == 1) ? chunkSize + 1 : chunkSize;

            if (chunkID == FMT_CHUNK_ID)
            {
                // Flag that the format chunk has been found
                foundFormat = true;

                // Read in the header info
                m_istream.read(m_buffer, 16);
                check_istream();

                // Check this is uncompressed data
                auto compressionCode = getLE(m_buffer, 0, 2);
                if (compressionCode != 1) SF_THROW(std::logic_error("Compression Code " + std::to_string(compressionCode) + " not supported"));

                // Extract the format information
                auto num_chans = (uint64_t)getLE(m_buffer, 2, 2);
                m_sample_rate = getLE(m_buffer, 4, 4);
                m_block_align = (uint64_t)getLE(m_buffer, 12, 2);
                m_valid_bits = (uint64_t)getLE(m_buffer, 14, 2);

                if (num_chans == 0) SF_THROW(std::logic_error(
                                "Number of channels specified in header is equal to zero"));
                if (num_chans != 1) SF_THROW(std::logic_error("Only single channel wav supported"));
                if (m_block_align == 0) SF_THROW(std::logic_error("Block Align specified in header is equal to zero"));
                if (m_valid_bits < 2) SF_THROW(std::logic_error("Valid Bits specified in header is less than 2"));
                if (m_valid_bits > 64) SF_THROW(std::logic_error(
                                "Valid Bits specified in header is greater than 64, this is greater than a long can hold"));

                // Calculate the number of bytes required to hold 1 sample
                m_bytes_per_sample = (m_valid_bits + 7) / 8;
                if (m_bytes_per_sample != m_block_align) SF_THROW(std::logic_error(
                                "Block Align does not agree with bytes required for validBits and number of channels"));

                // Account for number of format bytes and then skip over
                // any extra format bytes
                numChunkBytes -= 16;
                if (numChunkBytes > 0) m_istream.seekg(numChunkBytes, m_istream.cur);
            }
            else if (chunkID == DATA_CHUNK_ID)
            {
                // Check if we've found the format chunk,
                // If not, SF_THROW(an exception as we need the format information
                // before we can read the data chunk
                if (foundFormat == false) SF_THROW(std::logic_error("Data chunk found before Format chunk"));

                // Check that the chunkSize (wav data length) is a multiple of
                // the
                // block align (bytes per frame)
                if (chunkSize % m_block_align != 0) SF_THROW(std::logic_error("Data Chunk size is not multiple of Block Align"));

                // Calculate the number of frames
                m_num_frames = chunkSize / m_block_align;

                // Flag that we've found the wave data chunk
                foundData = true;

                break;
            }
            else
            {
                // If an unknown chunk ID is found, just skip over the chunk
                // data
                m_istream.seekg(numChunkBytes, m_istream.cur);
            }
        }

        // SF_THROW(an exception if no data chunk has been found
        if (foundData == false) SF_THROW(std::logic_error{ "Did not find a data chunk" });
    
        m_buffer_pointer = 0;
        m_bytes_read = 0;
        m_frame_counter = 0;
    };
 
    int32_t readSample()
    {
        long val = 0;

        for (uint16_t b = 0; b < m_bytes_per_sample; b++)
        {
            if (m_buffer_pointer == m_bytes_read)
            {
                auto read = m_istream.readsome(m_buffer, BUFFER_SIZE);
                check_istream();
                if (read > (1ll << 32)-1) SF_THROW(std::out_of_range{ "Wav file too long" });
                m_bytes_read = uint32_t(read);
                m_buffer_pointer = 0;
            }

            int32_t v = m_buffer[m_buffer_pointer];
            if (b < m_bytes_per_sample - 1 || m_bytes_per_sample == 1) v &= 0xFF;
            val += v << (b * 8);

            m_buffer_pointer++;
        }

        return val;
    }
};
void signal_to_wav(const std::string& filename_in)
{
    SF_MARK_STACK;
    singal_file_header header;
    auto filename = work_space() + filename_in + ".sig";
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    if (!in) SF_THROW(std::invalid_argument{ "File not found: " + filename});
    auto len = (size_t(in.tellg()) - sizeof(header)) / sizeof(float);
    in.seekg(0);
    auto wavname = output_space() + filename_in + ".wav";
    std::cerr << "Writing wav file: " << wavname << std::endl;
    if (len > std::numeric_limits<uint32_t>::max())
        SF_THROW(std::invalid_argument{ "Signal too long for wav"});
    wavsignal_writer wav{ wavname, uint32_t(len), SAMPLES_PER_SECOND>>1 };

    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in) SF_THROW(std::out_of_range{ "signal file corrupt" });
    auto scale = -header.peak_negative > header.peak_positive ?
        -1.0 / header.peak_negative : 1.0 / header.peak_positive;
    scale *= 0.99;
    std::cerr << "Wave scaling factor: " << scale << std::endl;
    for (decltype(len) idx{ 0 }; idx < len; ++idx)
    {
        char buf[sizeof(float)];
        in.read(buf, sizeof(float));
        if (!in) SF_THROW(std::out_of_range{ "signal file corrupt" });
        float fsamp = *(reinterpret_cast<float*>(buf)) * scale;
        auto isamp = int32_t(fsamp * std::numeric_limits<int32_t>::max());
        wav.writeSample(isamp);
    }
    wav.flush();
}

void wav_to_sig(const std::string&)
{}
} // sonic_field