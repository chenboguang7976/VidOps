#include "core/YtDlp.h"

#include <QFile>

namespace vidops::ytdlp {

QString formatSort(const DownloadOptions &o)
{
    QStringList s;
    if (isAudioOnly(o.format)) {
        // AAC sources can be stored as .m4a without a lossy re-encode.
        if (o.format == OutputFormat::AudioM4A)
            s << QStringLiteral("acodec:aac");
        return s.join(QLatin1Char(','));
    }

    // Resolution first: we would rather convert a 1080p VP9 stream than
    // download a 360p stream that happens to be H.264.
    if (o.maxHeight > 0)
        s << QStringLiteral("res:%1").arg(o.maxHeight);

    switch (o.format) {
    case OutputFormat::Mp4H264: s << QStringLiteral("vcodec:h264"); break;
    case OutputFormat::Mp4H265: s << QStringLiteral("vcodec:h265"); break;
    case OutputFormat::Mp4AV1: s << QStringLiteral("vcodec:av01"); break;
    default: break;
    }
    // Prefer AAC so the audio track can be copied into the MP4 as-is.
    if (convertsVideo(o.format))
        s << QStringLiteral("acodec:aac");
    return s.join(QLatin1Char(','));
}

QStringList buildArgs(const DownloadOptions &o, const QString &url, const QString &pathsFile)
{
    const QString tag = QString::fromLatin1(kProgressTag);
    QStringList a;
    a << QStringLiteral("--encoding") << QStringLiteral("utf-8")
      << QStringLiteral("--newline") << QStringLiteral("--no-colors")
      << QStringLiteral("--progress") << QStringLiteral("--no-simulate")
      << QStringLiteral("--no-mtime") << QStringLiteral("--embed-metadata")
      << QStringLiteral("--progress-template")
      << QStringLiteral("download:") + tag
             + QStringLiteral("%(progress._percent_str)s|%(progress._speed_str)s|%(progress._eta_str)s")
      << QStringLiteral("--progress-template")
      << QStringLiteral("postprocess:") + QString::fromLatin1(kPostProcessTag)
             + QStringLiteral("%(progress.postprocessor)s|%(progress.status)s")
      << QStringLiteral("-O") << QStringLiteral("video:") + QString::fromLatin1(kTitleTag) + QStringLiteral("%(title)s")
      << QStringLiteral("-O") << QStringLiteral("after_move:") + QString::fromLatin1(kFileTag) + QStringLiteral("%(filepath)s")
      << (o.playlist ? QStringLiteral("--yes-playlist") : QStringLiteral("--no-playlist"));

    if (!pathsFile.isEmpty()) {
        // The FILE argument is itself an output template: escape '%'.
        QString escaped = pathsFile;
        escaped.replace(QLatin1Char('%'), QStringLiteral("%%"));
        a << QStringLiteral("--print-to-file") << QStringLiteral("after_move:%(filepath)s") << escaped;
    }

    // YouTube needs a JavaScript runtime to unlock most formats. deno is
    // yt-dlp's default; node is enabled too in case it is installed instead.
    a << QStringLiteral("--js-runtimes")
      << (o.denoPath.isEmpty() ? QStringLiteral("deno") : QStringLiteral("deno:") + o.denoPath)
      << QStringLiteral("--js-runtimes") << QStringLiteral("node");

    if (!o.ffmpegPath.isEmpty())
        a << QStringLiteral("--ffmpeg-location") << o.ffmpegPath;

    if (!o.cookiesFile.isEmpty())
        a << QStringLiteral("--cookies") << o.cookiesFile;
    else if (!o.cookiesBrowser.isEmpty())
        a << QStringLiteral("--cookies-from-browser") << o.cookiesBrowser;

    if (!o.outputDir.isEmpty())
        a << QStringLiteral("-P") << o.outputDir;
    a << QStringLiteral("-o")
      << (o.filenameTemplate.isEmpty() ? defaultFilenameTemplate() : o.filenameTemplate);

    if (isAudioOnly(o.format)) {
        a << QStringLiteral("-f") << QStringLiteral("ba/b") << QStringLiteral("-x")
          << QStringLiteral("--audio-format")
          << (o.format == OutputFormat::AudioM4A ? QStringLiteral("m4a") : QStringLiteral("mp3"))
          << QStringLiteral("--audio-quality") << QStringLiteral("0");
    } else {
        a << QStringLiteral("-f") << QStringLiteral("bv*+ba/b")
          << QStringLiteral("--merge-output-format") << QStringLiteral("mp4")
          << QStringLiteral("--remux-video") << QStringLiteral("mp4");
    }

    const QString sort = formatSort(o);
    if (!sort.isEmpty())
        a << QStringLiteral("-S") << sort;

    a << QStringLiteral("--") << url;
    return a;
}

QStringList readPathsFile(const QString &pathsFile)
{
    QFile f(pathsFile);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QStringList paths;
    const QString text = QString::fromUtf8(f.readAll());
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString path = line.trimmed();
        if (!path.isEmpty() && !paths.contains(path))
            paths << path;
    }
    return paths;
}

static QString cleanField(const QString &s)
{
    const QString t = s.trimmed();
    if (t.isEmpty() || t.startsWith(QLatin1String("Unknown")) || t == QLatin1String("N/A")
        || t == QLatin1String("NA") || t == QLatin1String("None"))
        return {};
    return t;
}

Event parseLine(const QString &rawLine)
{
    Event ev;
    const QString line = rawLine.trimmed();

    auto stripTag = [&line](const char *tag) -> QString {
        return line.mid(int(qstrlen(tag)));
    };

    if (line.startsWith(QLatin1String(kProgressTag))) {
        const QStringList parts = stripTag(kProgressTag).split(QLatin1Char('|'));
        ev.type = Event::Progress;
        if (!parts.isEmpty()) {
            QString pct = parts.at(0).trimmed();
            pct.remove(QLatin1Char('%'));
            bool ok = false;
            const double v = pct.toDouble(&ok);
            if (ok)
                ev.percent = qBound(0.0, v, 100.0);
        }
        if (parts.size() > 1)
            ev.speed = cleanField(parts.at(1));
        if (parts.size() > 2)
            ev.eta = cleanField(parts.at(2));
    } else if (line.startsWith(QLatin1String(kPostProcessTag))) {
        const QStringList parts = stripTag(kPostProcessTag).split(QLatin1Char('|'));
        ev.type = Event::PostProcess;
        ev.text = parts.value(0).trimmed();
    } else if (line.startsWith(QLatin1String(kTitleTag))) {
        ev.type = Event::Title;
        ev.text = stripTag(kTitleTag);
    } else if (line.startsWith(QLatin1String(kFileTag))) {
        ev.type = Event::File;
        ev.text = stripTag(kFileTag);
    } else if (line.startsWith(QLatin1String("ERROR:"))) {
        ev.type = Event::Error;
        ev.text = line.mid(6).trimmed();
    } else if (line.startsWith(QLatin1String("WARNING:"))) {
        ev.type = Event::Warning;
        ev.text = line.mid(8).trimmed();
    } else {
        ev.text = line;
    }
    return ev;
}

}  // namespace vidops::ytdlp
