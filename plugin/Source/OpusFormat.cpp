#include "OpusFormat.h"
#include <opusfile.h>

namespace
{
    class OpusReader : public juce::AudioFormatReader
    {
    public:
        OpusReader (juce::InputStream* in, juce::MemoryBlock&& bytes, OggOpusFile* f)
            : AudioFormatReader (in, "Opus"), data (std::move (bytes)), file (f)
        {
            sampleRate = 48000.0;
            bitsPerSample = 32;
            usesFloatingPointData = true;
            numChannels = (unsigned int) juce::jlimit (1, 2, op_channel_count (file, -1));
            lengthInSamples = juce::jmax ((ogg_int64_t) 0, op_pcm_total (file, -1));
        }

        ~OpusReader() override { op_free (file); }

        bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                          juce::int64 startSampleInFile, int numSamples) override
        {
            if (startSampleInFile != position)
            {
                if (op_pcm_seek (file, startSampleInFile) != 0)
                    return false;
                position = startSampleInFile;
            }

            const int srcChans = op_channel_count (file, -1);
            int done = 0;
            while (done < numSamples)
            {
                const int want = juce::jmin (numSamples - done, 5760);
                temp.resize ((size_t) (want * srcChans));
                // op_read_float may return fewer samples than asked; loop until filled or the file ends.
                const int got = op_read_float (file, temp.data(), want * srcChans, nullptr);
                if (got <= 0)
                    break;

                for (int ch = 0; ch < numDestChannels; ++ch)
                    if (auto* dest = reinterpret_cast<float*> (destChannels[ch]))
                    {
                        const int srcCh = juce::jmin (ch, srcChans - 1);
                        for (int i = 0; i < got; ++i)
                            dest[startOffsetInDestBuffer + done + i] = temp[(size_t) (i * srcChans + srcCh)];
                    }
                done += got;
                position += got;
            }

            // Past the end: fill with silence.
            for (int ch = 0; ch < numDestChannels; ++ch)
                if (auto* dest = reinterpret_cast<float*> (destChannels[ch]))
                    juce::FloatVectorOperations::clear (dest + startOffsetInDestBuffer + done, numSamples - done);
            return true;
        }

    private:
        juce::MemoryBlock data; // op_open_memory reads from this, so it must outlive `file`
        OggOpusFile* file;
        juce::int64 position = 0;
        std::vector<float> temp;
    };
}

OpusAudioFormat::OpusAudioFormat() : AudioFormat ("Opus", juce::StringArray (".opus", ".ogg")) {}

juce::AudioFormatReader* OpusAudioFormat::createReaderFor (juce::InputStream* source, bool deleteStreamIfOpeningFails)
{
    juce::MemoryBlock bytes;
    source->readIntoMemoryBlock (bytes);

    int err = 0;
    auto* f = op_open_memory (static_cast<const unsigned char*> (bytes.getData()), bytes.getSize(), &err);
    if (f == nullptr)
    {
        if (deleteStreamIfOpeningFails)
            delete source;
        return nullptr;
    }
    return new OpusReader (source, std::move (bytes), f);
}
