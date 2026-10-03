#pragma once

#include "core/Options.h"

namespace vidops {

struct AppSettings {
    DownloadOptions options;
    QString ytdlpPath;    // empty = auto-detect
    QString ffmpegPath;   // empty = auto-detect
    QString ffprobePath;  // empty = auto-detect
    int maxConcurrent = 2;

    static QString defaultOutputDir();
    static AppSettings load();
    void save() const;
};

}  // namespace vidops
