#include "NativeRegionDrop.h"
namespace flowtone
{
NativeRegionDrop::NativeRegionDrop(){setWantsKeyboardFocus(false);}
NativeRegionDrop::~NativeRegionDrop()=default;
bool NativeRegionDrop::isInterestedInFileDrag(const juce::StringArray& files)
{return files.size()==1&&(!canDrop||canDrop());}
void NativeRegionDrop::fileDragEnter(const juce::StringArray&,int,int)
{if(hover)hover(true);}
void NativeRegionDrop::fileDragExit(const juce::StringArray&)
{if(hover)hover(false);}
void NativeRegionDrop::filesDropped(const juce::StringArray& files,int,int)
{
    if(hover)hover(false);
    if((canDrop&&!canDrop())||!beginDrop)return;
    auto complete=beginDrop();if(!complete)return;
    RegionDrop drop;
    if(files.size()!=1)drop.error=juce::String::fromUTF8(u8"Перетащите один аудиофайл за раз.");
    else
    {
        drop.file=juce::File(files[0]);
        drop.openedStream=std::shared_ptr<juce::InputStream>(drop.file.createInputStream().release());
        if(!drop.openedStream)drop.error=juce::String::fromUTF8(u8"Секвенсор не передал доступный аудиофайл. Используйте перенос из аудиоредактора, Проводник или «Выбрать файл».");
    }
    // Opening the stream is immediate. Decoding remains on the analysis worker;
    // even a multi-gigabyte file is neither copied nor decoded on the UI thread.
    complete(std::move(drop));
}
void NativeRegionDrop::mouseDown(const juce::MouseEvent& event)
{if(seekAt)seekAt(event.position.x);}
}
