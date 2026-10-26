#include "FileAnalysis.h"
#if JUCE_MAC
#include <pthread/qos.h>
#endif
namespace flowtone
{
namespace
{
class SharedInput final:public juce::InputStream
{
public:
    explicit SharedInput(std::shared_ptr<juce::InputStream> input,bool rewind=false):source(std::move(input)),length(source->getTotalLength()){if(rewind)source->setPosition(0);}
    juce::int64 getTotalLength()override{return length;}
    juce::int64 getPosition()override{return source->getPosition();}
    bool setPosition(juce::int64 position)override{return source->setPosition(position);}
    bool isExhausted()override{return source->getPosition()>=length;}
    int read(void* dest,int count)override{return source->read(dest,count);}
private:
    std::shared_ptr<juce::InputStream> source;
    const juce::int64 length;
};
}
FileAnalysis::FileAnalysis(std::function<void()> changed):stateChanged(std::move(changed)),worker([this]{run();}) {}
FileAnalysis::~FileAnalysis(){stop();}
void FileAnalysis::stop()
{
    exiting.store(true);revision.fetch_add(1);ready.notify_one();
    if(worker.joinable())worker.join();
}
void FileAnalysis::load(const juce::File& file,std::shared_ptr<void> lease,std::shared_ptr<juce::InputStream> stream)
{
    // JUCE FileInputStream queries length by pathname. Cache it while the drag
    // source still exists, before a DAW can unlink its temporary export.
    // Do not seek here: an older cancelled job may still own this same stream.
    if(stream&&dynamic_cast<SharedInput*>(stream.get())==nullptr)stream=std::make_shared<SharedInput>(std::move(stream));
    {
        const std::lock_guard<std::mutex> lock(mutex);
        request={file,std::move(lease),revision.fetch_add(1)+1,std::move(stream)};pending=true;
        current={};current.name=file.getFileName();current.path=file.getFullPathName();current.busy=true;
    }
    ready.notify_one();
}
void FileAnalysis::repeat()
{
    Request previous;
    {const std::lock_guard<std::mutex> lock(mutex);previous=request;}
    load(previous.file,std::move(previous.lease),std::move(previous.stream));
}
void FileAnalysis::restore(FileAnalysisSnapshot saved)
{
    const std::lock_guard<std::mutex> lock(mutex);
    revision.fetch_add(1);pending=false;
    saved.busy=false;saved.progress=saved.available?1.0f:0.0f;
    current=std::move(saved);request={};
    if(!current.path.isEmpty()&&juce::File::isAbsolutePath(current.path))request.file=juce::File(current.path);
}
FileAnalysisSnapshot FileAnalysis::snapshot()const
{const std::lock_guard<std::mutex> lock(mutex);return current;}
void FileAnalysis::run()
{
#if JUCE_MAC
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif
    while(!exiting.load())
    {
        Request job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            ready.wait(lock,[this]{return exiting.load()||pending;});
            if(exiting.load())break;
            job=request;pending=false;
        }
        FileAnalysisSnapshot next;next.name=job.file.getFileName();next.path=job.file.getFullPathName();
        auto cancelled=[&]{return exiting.load()||revision.load()!=job.revision;};
        try
        {
            if(!job.stream&&!job.file.existsAsFile())next.error=juce::String::fromUTF8(u8"Файл недоступен. Перетащите его снова или выберите другой.");
            else
            {
                juce::AudioFormatManager formats;formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader(job.stream?formats.createReaderFor(std::make_unique<SharedInput>(job.stream,true)):formats.createReaderFor(job.file));
                if(!reader||reader->lengthInSamples<=0||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>384000||reader->numChannels<1||reader->numChannels>2)
                    next.error=juce::String::fromUTF8(u8"Не удалось прочитать аудио. Попробуйте моно- или стереофайл WAV, AIFF, MP3 или FLAC.");
                else
                {
                    next.duration=static_cast<double>(reader->lengthInSamples)/reader->sampleRate;
                    next.sampleRate=reader->sampleRate;next.channels=static_cast<int>(reader->numChannels);
                    if(next.duration>4*60*60)next.error=juce::String::fromUTF8(u8"Для анализа выберите файл короче четырёх часов.");
                    else
                    {
                        double lastProgress=-1;
                        next.result=TrackAnalysis::analyseFile(*reader,next.waveform,[&](double progress)
                        {
                            if(cancelled())return false;
                            if(progress-lastProgress>=0.005)
                            {
                                const std::lock_guard<std::mutex> lock(mutex);
                                if(cancelled())return false;
                                current.duration=next.duration;current.sampleRate=next.sampleRate;current.channels=next.channels;
                                current.progress=static_cast<float>(progress);lastProgress=progress;
                            }
                            return true;
                        });
                        next.available=!cancelled();next.progress=1;
                    }
                }
            }
        }
        catch(...){next.error=juce::String::fromUTF8(u8"Не удалось закончить чтение файла. Выберите файл заново.");}
        bool published=false;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            if(!cancelled()){current=std::move(next);published=true;}
        }
        // Host state requests may synchronously call snapshot(). Notify only
        // after releasing the result lock, including when the editor is closed.
        if(published&&stateChanged)stateChanged();
    }
}
}
