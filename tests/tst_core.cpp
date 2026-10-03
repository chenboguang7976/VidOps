#include "core/Ffmpeg.h"
#include "core/YtDlp.h"

#include <QtTest>

using namespace vidops;

class TestCore : public QObject
{
    Q_OBJECT
private slots:
    void formatSort_data();
    void formatSort();
    void buildArgs_video();
    void buildArgs_audio();
    void parseProgress();
    void parseOtherLines();
    void parseFfprobe();
    void parseEncoders();
    void pickEncoder();
    void plan_copyWhenMatching();
    void plan_convertVp9ToH264();
    void plan_audioOnlyFix();
    void plan_hevcRetag();
    void plan_missingEncoder();
    void transcodeArgs_hevc10bit();
    void ffmpegProgress();
};

void TestCore::formatSort_data()
{
    QTest::addColumn<int>("format");
    QTest::addColumn<int>("height");
    QTest::addColumn<QString>("expected");
    QTest::newRow("h264 1080") << int(OutputFormat::Mp4H264) << 1080 << "res:1080,vcodec:h264,acodec:aac";
    QTest::newRow("h265 best") << int(OutputFormat::Mp4H265) << 0 << "vcodec:h265,acodec:aac";
    QTest::newRow("av1 720") << int(OutputFormat::Mp4AV1) << 720 << "res:720,vcodec:av01,acodec:aac";
    QTest::newRow("original") << int(OutputFormat::Mp4Original) << 1440 << "res:1440";
    QTest::newRow("m4a") << int(OutputFormat::AudioM4A) << 1080 << "acodec:aac";
    QTest::newRow("mp3") << int(OutputFormat::AudioMP3) << 1080 << "";
}

void TestCore::formatSort()
{
    QFETCH(int, format);
    QFETCH(int, height);
    QFETCH(QString, expected);
    DownloadOptions o;
    o.format = static_cast<OutputFormat>(format);
    o.maxHeight = height;
    QCOMPARE(ytdlp::formatSort(o), expected);
}

void TestCore::buildArgs_video()
{
    DownloadOptions o;
    o.outputDir = QStringLiteral("/tmp/out");
    o.cookiesBrowser = QStringLiteral("firefox");
    o.ffmpegPath = QStringLiteral("/usr/bin/ffmpeg");
    const QStringList a = ytdlp::buildArgs(o, QStringLiteral("https://example.com/v"));
    QCOMPARE(a.last(), QStringLiteral("https://example.com/v"));
    QCOMPARE(a.at(a.size() - 2), QStringLiteral("--"));
    QVERIFY(a.contains(QStringLiteral("--no-playlist")));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("--merge-output-format")) + 1), QStringLiteral("mp4"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-P")) + 1), QStringLiteral("/tmp/out"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("--cookies-from-browser")) + 1), QStringLiteral("firefox"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("--ffmpeg-location")) + 1), QStringLiteral("/usr/bin/ffmpeg"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-o")) + 1), defaultFilenameTemplate());
    QVERIFY(!a.contains(QStringLiteral("-x")));
}

void TestCore::buildArgs_audio()
{
    DownloadOptions o;
    o.format = OutputFormat::AudioMP3;
    o.playlist = true;
    o.cookiesBrowser = QStringLiteral("chrome");
    o.cookiesFile = QStringLiteral("/c.txt");
    const QStringList a = ytdlp::buildArgs(o, QStringLiteral("https://x.y/z"));
    QVERIFY(a.contains(QStringLiteral("-x")));
    QVERIFY(a.contains(QStringLiteral("--yes-playlist")));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("--audio-format")) + 1), QStringLiteral("mp3"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("--cookies")) + 1), QStringLiteral("/c.txt"));
    QVERIFY(!a.contains(QStringLiteral("--cookies-from-browser")));
    QVERIFY(!a.contains(QStringLiteral("-S")));
}

void TestCore::parseProgress()
{
    auto ev = ytdlp::parseLine(QStringLiteral("VIDOPS_PROGRESS|  45.3%|   2.31MiB/s|00:12"));
    QCOMPARE(ev.type, ytdlp::Event::Progress);
    QCOMPARE(ev.percent, 45.3);
    QCOMPARE(ev.speed, QStringLiteral("2.31MiB/s"));
    QCOMPARE(ev.eta, QStringLiteral("00:12"));

    ev = ytdlp::parseLine(QStringLiteral("VIDOPS_PROGRESS|N/A|Unknown B/s|Unknown"));
    QCOMPARE(ev.type, ytdlp::Event::Progress);
    QCOMPARE(ev.percent, -1.0);
    QVERIFY(ev.speed.isEmpty());
    QVERIFY(ev.eta.isEmpty());
}

void TestCore::parseOtherLines()
{
    auto ev = ytdlp::parseLine(QStringLiteral("VIDOPS_TITLE|Tiêu đề | có dấu"));
    QCOMPARE(ev.type, ytdlp::Event::Title);
    QCOMPARE(ev.text, QStringLiteral("Tiêu đề | có dấu"));

    ev = ytdlp::parseLine(QStringLiteral("VIDOPS_FILE|/a/b c.mp4\r"));
    QCOMPARE(ev.type, ytdlp::Event::File);
    QCOMPARE(ev.text, QStringLiteral("/a/b c.mp4"));

    ev = ytdlp::parseLine(QStringLiteral("VIDOPS_PP|Merger|started"));
    QCOMPARE(ev.type, ytdlp::Event::PostProcess);
    QCOMPARE(ev.text, QStringLiteral("Merger"));

    ev = ytdlp::parseLine(QStringLiteral("ERROR: [youtube] abc: Video unavailable"));
    QCOMPARE(ev.type, ytdlp::Event::Error);
    QCOMPARE(ev.text, QStringLiteral("[youtube] abc: Video unavailable"));

    QCOMPARE(ytdlp::parseLine(QStringLiteral("WARNING: x")).type, ytdlp::Event::Warning);
    QCOMPARE(ytdlp::parseLine(QStringLiteral("[info] hello")).type, ytdlp::Event::None);
}

static const char kProbeJson[] = R"({
  "streams": [
    {"codec_type":"video","codec_name":"mjpeg","disposition":{"attached_pic":1}},
    {"codec_type":"video","codec_name":"vp9","codec_tag_string":"vp09","pix_fmt":"yuv420p10le",
     "width":3840,"height":2160,"disposition":{"attached_pic":0}},
    {"codec_type":"audio","codec_name":"opus","duration":"61.5"}
  ],
  "format": {"duration":"62.000000"}
})";

void TestCore::parseFfprobe()
{
    const auto info = ffmpeg::parseFfprobeJson(kProbeJson);
    QVERIFY(info.valid);
    QCOMPARE(info.videoCodec, QStringLiteral("vp9"));
    QCOMPARE(info.audioCodec, QStringLiteral("opus"));
    QCOMPARE(info.width, 3840);
    QCOMPARE(info.height, 2160);
    QCOMPARE(info.durationSec, 62.0);
    QVERIFY(info.isTenBit());
    QVERIFY(!ffmpeg::parseFfprobeJson("not json").valid);
}

void TestCore::parseEncoders()
{
    const QString out = QStringLiteral(
        "Encoders:\n"
        " V..... = Video\n"
        " A..... = Audio\n"
        " ------\n"
        " V....D libx264              libx264 H.264 / AVC\n"
        " V....D h264_nvenc           NVIDIA NVENC H.264 encoder\n"
        " V....D libsvtav1            SVT-AV1\n"
        " A....D aac                  AAC (Advanced Audio Coding)\n");
    const auto enc = ffmpeg::parseEncoderList(out);
    QVERIFY(enc.contains(QStringLiteral("libx264")));
    QVERIFY(enc.contains(QStringLiteral("h264_nvenc")));
    QVERIFY(enc.contains(QStringLiteral("libsvtav1")));
    QVERIFY(enc.contains(QStringLiteral("aac")));
    QVERIFY(!enc.contains(QStringLiteral("=")));
    QCOMPARE(enc.size(), 4);
}

void TestCore::pickEncoder()
{
    const QSet<QString> avail{QStringLiteral("libx264"), QStringLiteral("libx265"),
                              QStringLiteral("libaom-av1"), QStringLiteral("h264_nvenc")};
    QCOMPARE(ffmpeg::pickVideoEncoder(OutputFormat::Mp4H264, Encoder::Nvenc, avail), QStringLiteral("h264_nvenc"));
    QString note;
    QCOMPARE(ffmpeg::pickVideoEncoder(OutputFormat::Mp4H265, Encoder::Nvenc, avail, &note), QStringLiteral("libx265"));
    QVERIFY(!note.isEmpty());
    QCOMPARE(ffmpeg::pickVideoEncoder(OutputFormat::Mp4AV1, Encoder::Software, avail), QStringLiteral("libaom-av1"));
    QCOMPARE(ffmpeg::pickVideoEncoder(OutputFormat::Mp4AV1, Encoder::Software, {}), QStringLiteral("libsvtav1"));
    QVERIFY(ffmpeg::pickVideoEncoder(OutputFormat::Mp4Original, Encoder::Software, avail).isEmpty());
}

static ffmpeg::MediaInfo media(const char *v, const char *a, const char *pix = "yuv420p", const char *tag = "")
{
    ffmpeg::MediaInfo m;
    m.videoCodec = QString::fromLatin1(v);
    m.audioCodec = QString::fromLatin1(a);
    m.pixFmt = QString::fromLatin1(pix);
    m.videoTag = QString::fromLatin1(tag);
    m.durationSec = 10;
    m.valid = true;
    return m;
}

void TestCore::plan_copyWhenMatching()
{
    DownloadOptions o;
    const auto plan = ffmpeg::planTranscode(media("h264", "aac"), o, {});
    QVERIFY(!plan.needed);
    o.forceReencode = true;
    QVERIFY(ffmpeg::planTranscode(media("h264", "aac"), o, {}).video);
    o.forceReencode = false;
    o.format = OutputFormat::Mp4Original;
    QVERIFY(!ffmpeg::planTranscode(media("vp9", "opus"), o, {}).needed);
}

void TestCore::plan_convertVp9ToH264()
{
    DownloadOptions o;
    const auto plan = ffmpeg::planTranscode(media("vp9", "opus", "yuv420p10le"), o, {});
    QVERIFY(plan.needed && plan.video && plan.audio);
    QCOMPARE(plan.videoEncoder, QStringLiteral("libx264"));
    const QStringList a = ffmpeg::transcodeArgs(plan, media("vp9", "opus", "yuv420p10le"), o,
                                                QStringLiteral("in.webm"), QStringLiteral("out.mp4"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-c:v")) + 1), QStringLiteral("libx264"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-crf")) + 1), QStringLiteral("23"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-pix_fmt")) + 1), QStringLiteral("yuv420p"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-c:a")) + 1), QStringLiteral("aac"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-b:a")) + 1), QStringLiteral("192k"));
    QCOMPARE(a.last(), QStringLiteral("out.mp4"));
    QVERIFY(!a.contains(QStringLiteral("-tag:v")));
}

void TestCore::plan_audioOnlyFix()
{
    DownloadOptions o;
    const auto plan = ffmpeg::planTranscode(media("h264", "opus"), o, {});
    QVERIFY(plan.needed && !plan.video && plan.audio);
    const QStringList a = ffmpeg::transcodeArgs(plan, media("h264", "opus"), o, QStringLiteral("i"), QStringLiteral("o"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-c:v")) + 1), QStringLiteral("copy"));
}

void TestCore::plan_hevcRetag()
{
    DownloadOptions o;
    o.format = OutputFormat::Mp4H265;
    const auto plan = ffmpeg::planTranscode(media("hevc", "aac", "yuv420p", "hev1"), o, {});
    QVERIFY(plan.needed && !plan.video && !plan.audio);
    QVERIFY(!ffmpeg::planTranscode(media("hevc", "aac", "yuv420p", "hvc1"), o, {}).needed);
}

void TestCore::plan_missingEncoder()
{
    DownloadOptions o;
    o.format = OutputFormat::Mp4AV1;
    const auto plan = ffmpeg::planTranscode(media("h264", "aac"), o, {QStringLiteral("libx264")});
    QVERIFY(!plan.error.isEmpty());
}

void TestCore::transcodeArgs_hevc10bit()
{
    DownloadOptions o;
    o.format = OutputFormat::Mp4H265;
    o.quality = 20;
    o.speed = SpeedPreset::Quality;
    const auto info = media("vp9", "aac", "yuv420p10le");
    const auto plan = ffmpeg::planTranscode(info, o, {});
    const QStringList a = ffmpeg::transcodeArgs(plan, info, o, QStringLiteral("i"), QStringLiteral("o"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-c:v")) + 1), QStringLiteral("libx265"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-crf")) + 1), QStringLiteral("20"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-preset")) + 1), QStringLiteral("slow"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-pix_fmt")) + 1), QStringLiteral("yuv420p10le"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-tag:v")) + 1), QStringLiteral("hvc1"));
    QCOMPARE(a.at(a.indexOf(QStringLiteral("-c:a")) + 1), QStringLiteral("copy"));

    o.encoder = Encoder::Nvenc;
    const auto nv = ffmpeg::planTranscode(info, o, {});
    const QStringList b = ffmpeg::transcodeArgs(nv, info, o, QStringLiteral("i"), QStringLiteral("o"));
    QCOMPARE(b.at(b.indexOf(QStringLiteral("-c:v")) + 1), QStringLiteral("hevc_nvenc"));
    QCOMPARE(b.at(b.indexOf(QStringLiteral("-cq")) + 1), QStringLiteral("20"));
    QCOMPARE(b.at(b.indexOf(QStringLiteral("-pix_fmt")) + 1), QStringLiteral("yuv420p"));
}

void TestCore::ffmpegProgress()
{
    QCOMPARE(ffmpeg::parseProgressSeconds(QStringLiteral("out_time_us=2500000")), 2.5);
    QCOMPARE(ffmpeg::parseProgressSeconds(QStringLiteral("out_time_ms=1000000")), 1.0);
    QCOMPARE(ffmpeg::parseProgressSeconds(QStringLiteral("out_time_us=N/A")), -1.0);
    QCOMPARE(ffmpeg::parseProgressSeconds(QStringLiteral("frame=10")), -1.0);
}

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
