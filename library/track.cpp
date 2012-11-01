#include "track.h"
#include "track.pb.h"
#include <QDebug>
#include <boost/scoped_array.hpp>

using namespace boost;

Track::Track(proto::Track ptrack)
{
    location = QString::fromUtf8(ptrack.location().c_str());
    if (ptrack.has_audioproperties()) {
        const proto::AudioProperties ap = ptrack.audioproperties();
        if (ap.has_bitrate())
            audioproperties.bitrate = ap.bitrate();
        if (ap.has_channels())
            audioproperties.channels = ap.channels();
        if (ap.has_length())
            audioproperties.length = ap.length();
        if (ap.has_samplerate())
            audioproperties.samplerate = ap.samplerate();
    }
    for (int i = 0; i < ptrack.metadata().fields_size(); i++)
    {
        const proto::MetadataItem &field = ptrack.metadata().fields(i);
        metadata[QString::fromUtf8(field.name().c_str())] = QString::fromUtf8(field.value().c_str());
    }
    if (ptrack.has_mtime())
        mtime = ptrack.mtime();
    qDebug() << "LOADFROMDISK: " << metadata["title"] << " " << audioproperties.length;
}

void Track::fillProtoTrack(proto::Track& ptrack)
{
    ptrack.Clear();
    ptrack.set_mtime(mtime);
    ptrack.set_location(location.toUtf8());
    proto::AudioProperties* audioProperties = ptrack.mutable_audioproperties();
    audioProperties->set_length(audioproperties.length);
    audioProperties->set_bitrate(audioproperties.bitrate);
    audioProperties->set_samplerate(audioproperties.samplerate);
    audioProperties->set_channels(audioproperties.channels);
    proto::Metadata* meta = ptrack.mutable_metadata();
    for (QMap<QString, QString>::const_iterator it = metadata.begin(); it != metadata.end(); ++it) {
        proto::MetadataItem* item = meta->add_fields();
        item->set_name(it.key().toUtf8());
        item->set_value(it.value().toUtf8());
    }
}
