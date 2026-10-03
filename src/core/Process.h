#pragma once

class QObject;
class QProcess;

namespace vidops {

// Creates a QProcess set up for our helper tools: UTF-8 output from Python
// (yt-dlp), and its own process group on Unix so it can be stopped together
// with the ffmpeg children it spawns.
QProcess *createToolProcess(QObject *parent);

// Stops the process and everything it started.
void terminateProcessTree(QProcess *process);

}  // namespace vidops
