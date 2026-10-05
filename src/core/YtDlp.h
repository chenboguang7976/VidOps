#pragma once

#include "core/Options.h"

#include <QStringList>

// Building yt-dlp command lines and parsing the machine-readable lines we ask
// it to print (via --progress-template / --print).
namespace vidops::ytdlp {

inline constexpr char kProgressTag[] = "VIDOPS_PROGRESS|";
inline constexpr char kPostProcessTag[] = "VIDOPS_PP|";
inline constexpr char kTitleTag[] = "VIDOPS_TITLE|";
inline constexpr char kFileTag[] = "VIDOPS_FILE|";

// Value for -S (format sorting); empty when yt-dlp's default is fine.
QString formatSort(const DownloadOptions &o);

// `pathsFile`: yt-dlp appends the final path of every downloaded file to it,
// one per line, always UTF-8. Console output uses the Windows code page when
// piped, which silently drops characters such as Chinese or Vietnamese, so
// the file — not stdout — is the source of truth for paths.
QStringList buildArgs(const DownloadOptions &o, const QString &url,
                      const QString &pathsFile = {});

// Reads the file written via --print-to-file: unique, non-empty lines.
QStringList readPathsFile(const QString &pathsFile);

struct Event {
    enum Type { None, Progress, PostProcess, Title, File, Error, Warning };
    Type type = None;
    double percent = -1;  // Progress only, -1 when unknown
    QString speed;        // Progress only
    QString eta;          // Progress only
    QString text;         // everything else (raw line for None)
};

Event parseLine(const QString &line);

}  // namespace vidops::ytdlp
