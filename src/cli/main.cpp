// SPDX-License-Identifier: GPL-3.0-or-later
// clivcut-render: renderização e conversão sem interface gráfica.

#include "core/DraftImport.h"
#include "core/Captions.h"
#include "core/Edit.h"
#include "core/Serialize.h"
#include "engine/AiModels.h"
#include "engine/Exporter.h"
#include "engine/Matting.h"
#include "engine/Transcriber.h"
#include "engine/WhisperModels.h"
#include "media/FFmpegUtil.h"
#include "media/MediaProbe.h"
#include "media/VideoDecoder.h"

#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>

#include <cstdio>

using namespace clv;

namespace {

bool loadAny(const QString &path, Project &p, QString *error)
{
    const QFileInfo fi(path);
    // Um arquivo de mídia vira um projeto com um único clipe.
    if (fi.isFile() && fi.suffix() != QLatin1String("clivcut") && !fi.fileName().startsWith(QLatin1String("draft_"))) {
        MediaAsset a;
        if (!media::probe(path, a, error))
            return false;
        a.id = newId("ast");
        p = createProject(fi.completeBaseName(), a.width > 0 ? a.width : 1920, a.height > 0 ? a.height : 1080,
                          a.fps > 0 ? a.fps : 30);
        p.assets.emplace(a.id, a);
        const int track = a.kind == MediaKind::Audio ? -1 : 0;
        edit::insertClip(p, createMediaClip(a, 0), track);
        return true;
    }
    if (fi.isDir() || fi.fileName().startsWith(QLatin1String("draft_"))) {
        auto r = importDraft(path);
        for (const auto &w : r.warnings)
            std::fprintf(stderr, "aviso: %s\n", qPrintable(w));
        if (!r.ok) {
            *error = r.error;
            return false;
        }
        p = std::move(r.project);
        return true;
    }
    return loadProjectFile(path, p, error);
}

/** Completa metadados de mídia (dimensões, áudio) lendo os arquivos. */
void refreshAssets(Project &p)
{
    for (auto &[id, a] : p.assets) {
        MediaAsset probed;
        if (media::probe(a.path, probed)) {
            a.kind = probed.kind;
            a.width = probed.width;
            a.height = probed.height;
            a.hasAudio = probed.hasAudio;
            a.hasVideo = probed.hasVideo;
            if (a.duration <= 0)
                a.duration = probed.duration;
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    media::quietFFmpegLogs();
    QGuiApplication::setApplicationName(QStringLiteral("clivcut-render"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CLIVCUT_VERSION));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral(
        "Renderiza um projeto .clivcut (ou um draft JSON) para vídeo, ou converte drafts.\n"
        "  clivcut-render projeto.clivcut saida.mp4\n"
        "  clivcut-render --convert pasta_do_draft projeto.clivcut\n"
        "  clivcut-render --probe arquivo.mp4\n"
        "  clivcut-render --transcribe [--model base] [--language pt] video.mp4 legendas.srt"));
    cli.addHelpOption();
    cli.addVersionOption();
    cli.addPositionalArgument(QStringLiteral("entrada"), QStringLiteral("Projeto .clivcut ou pasta de draft JSON"));
    cli.addPositionalArgument(QStringLiteral("saida"), QStringLiteral("Arquivo de vídeo (.mp4/.mov/.mkv) ou .clivcut"));
    QCommandLineOption codec({QStringLiteral("c"), QStringLiteral("codec")}, QStringLiteral("h264|hevc|av1|prores"),
                             QStringLiteral("codec"), QStringLiteral("h264"));
    QCommandLineOption crf(QStringLiteral("crf"), QStringLiteral("Qualidade (menor = melhor)"), QStringLiteral("n"),
                           QStringLiteral("20"));
    QCommandLineOption height(QStringLiteral("height"), QStringLiteral("Altura de saída (ex.: 720, 1080, 2160)"),
                              QStringLiteral("px"));
    QCommandLineOption fps(QStringLiteral("fps"), QStringLiteral("Quadros por segundo"), QStringLiteral("fps"));
    QCommandLineOption convert(QStringLiteral("convert"), QStringLiteral("Converte a entrada para .clivcut sem renderizar"));
    QCommandLineOption probe(QStringLiteral("probe"), QStringLiteral("Mostra informações de um arquivo de mídia"));
    QCommandLineOption transcribe(QStringLiteral("transcribe"), QStringLiteral("Gera legendas (.srt) com whisper.cpp"));
    QCommandLineOption model(QStringLiteral("model"),
                             QStringLiteral("Modelo do whisper: tiny|base|small|large-v3-turbo ou caminho .bin"),
                             QStringLiteral("modelo"), QStringLiteral("base"));
    QCommandLineOption language(QStringLiteral("language"), QStringLiteral("Idioma da fala (pt, en, es... ou auto)"),
                                QStringLiteral("código"), QStringLiteral("auto"));
    QCommandLineOption maxChars(QStringLiteral("max-chars"), QStringLiteral("Caracteres por legenda (0 = frases)"),
                                QStringLiteral("n"), QStringLiteral("42"));
    QCommandLineOption bench(QStringLiteral("bench"), QStringLiteral("Mede a decodificação de um vídeo (quadros/s)"));
    QCommandLineOption noHw(QStringLiteral("no-hw"), QStringLiteral("Desliga a decodificação por hardware"));
    QCommandLineOption removeBg(QStringLiteral("remove-bg"),
                                QStringLiteral("Gera o vídeo de matte (alfa) de um vídeo/imagem com IA (modelo rvm)"));
    cli.addOptions({codec, crf, height, fps, convert, probe, transcribe, model, language, maxChars, bench, noHw, removeBg});
    cli.process(app);

    const QStringList args = cli.positionalArguments();
    if (cli.isSet(probe)) {
        if (args.isEmpty())
            cli.showHelp(1);
        MediaAsset a;
        QString err;
        if (!media::probe(args[0], a, &err)) {
            std::fprintf(stderr, "erro: %s\n", qPrintable(err));
            return 1;
        }
        const char *kinds[] = {"video", "audio", "image"};
        std::printf("tipo: %s\nduração: %.3f s\ndimensões: %dx%d\nfps: %.3f\náudio: %s\n", kinds[static_cast<int>(a.kind)],
                    a.duration / 1e6, a.width, a.height, a.fps, a.hasAudio ? "sim" : "não");
        return 0;
    }
    if (cli.isSet(removeBg)) {
        if (args.size() != 2)
            cli.showHelp(1);
        const QString modelPath = engine::aiModelPath(engine::kMattingModel);
        if (!engine::aiModelInstalled(engine::kMattingModel)) {
            std::fprintf(stderr, "erro: modelo ausente em %s\nBaixe com: curl -L -o %s %s\n", qPrintable(modelPath),
                         qPrintable(modelPath), qPrintable(engine::findAiModel(engine::kMattingModel)->url.toString()));
            return 1;
        }
        QElapsedTimer timer;
        timer.start();
        QString err;
        const bool ok = engine::generateMatte(args[0], modelPath, args[1], [](double f) {
            std::fprintf(stderr, "\rRemovendo fundo... %3d%%", static_cast<int>(f * 100));
            return true;
        }, &err);
        std::fprintf(stderr, "\n");
        if (!ok) {
            std::fprintf(stderr, "erro: %s\n", qPrintable(err));
            return 1;
        }
        std::printf("Matte salvo em %s (%.1f s)\n", qPrintable(args[1]), timer.elapsed() / 1000.0);
        return 0;
    }
    if (cli.isSet(bench)) {
        if (args.isEmpty())
            cli.showHelp(1);
        const int h = cli.isSet(height) ? cli.value(height).toInt() : 720;
        media::VideoDecoder dec;
        QString err;
        if (!dec.open(args[0], QSize(h * 4, h), &err, !cli.isSet(noHw))) {
            std::fprintf(stderr, "erro: %s\n", qPrintable(err));
            return 1;
        }
        MediaAsset info;
        media::probe(args[0], info);
        const double fpsIn = info.fps > 0 ? info.fps : 30;
        QElapsedTimer timer;
        timer.start();
        int frames = 0;
        for (Micros t = 0; t < dec.duration(); t += static_cast<Micros>(kSecond / fpsIn), ++frames)
            if (dec.frameAt(t).isNull())
                break;
        const double secs = timer.elapsed() / 1000.0;
        std::printf("%s → %dx%d, decodificador: %s\n%d quadros em %.2f s = %.1f quadros/s\n", qPrintable(QFileInfo(args[0]).fileName()),
                    dec.outputSize().width(), dec.outputSize().height(),
                    dec.hardwareDecoder().isEmpty() ? "software" : qPrintable(dec.hardwareDecoder()), frames, secs,
                    frames / secs);
        return 0;
    }
    if (args.size() != 2)
        cli.showHelp(1);

    Project p;
    QString err;
    if (!loadAny(args[0], p, &err)) {
        std::fprintf(stderr, "erro: %s\n", qPrintable(err));
        return 1;
    }
    refreshAssets(p);

    if (cli.isSet(transcribe)) {
        engine::TranscribeOptions topts;
        topts.modelPath = engine::resolveWhisperModel(cli.value(model));
        topts.language = cli.value(language);
        topts.maxChars = cli.value(maxChars).toInt();
        if (!QFileInfo::exists(topts.modelPath)) {
            std::fprintf(stderr, "erro: modelo não encontrado em %s\nBaixe com: curl -L -o %s %s\n",
                         qPrintable(topts.modelPath), qPrintable(topts.modelPath),
                         qPrintable(engine::whisperModelUrl(cli.value(model)).toString()));
            return 1;
        }
        QElapsedTimer timer;
        timer.start();
        std::vector<CaptionSegment> segs;
        const bool ok = engine::transcribeProject(
            p, topts,
            [](double f) {
                std::fprintf(stderr, "\rTranscrevendo... %3d%%", static_cast<int>(f * 100));
                return true;
            },
            segs, &err);
        std::fprintf(stderr, "\n");
        if (!ok) {
            std::fprintf(stderr, "erro: %s\n", qPrintable(err));
            return 1;
        }
        QFile out(args[1]);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::fprintf(stderr, "erro: %s\n", qPrintable(out.errorString()));
            return 1;
        }
        out.write(toSrt(segs).toUtf8());
        std::printf("%zu legendas em %s (%.1f s)\n", segs.size(), qPrintable(args[1]), timer.elapsed() / 1000.0);
        return 0;
    }

    if (cli.isSet(convert) || args[1].endsWith(QLatin1String(".clivcut"))) {
        if (!saveProjectFile(p, args[1], &err)) {
            std::fprintf(stderr, "erro: %s\n", qPrintable(err));
            return 1;
        }
        std::printf("Projeto salvo em %s\n", qPrintable(args[1]));
        return 0;
    }

    engine::ExportOptions opts;
    opts.outputPath = args[1];
    const QString c = cli.value(codec).toLower();
    opts.encode.codec = c == QLatin1String("hevc")     ? media::VideoCodec::HEVC
                        : c == QLatin1String("av1")    ? media::VideoCodec::AV1
                        : c == QLatin1String("prores") ? media::VideoCodec::ProRes
                                                       : media::VideoCodec::H264;
    opts.encode.crf = cli.value(crf).toInt();
    if (cli.isSet(height)) {
        const int h = cli.value(height).toInt();
        opts.encode.height = h;
        opts.encode.width = static_cast<int>(std::lround(static_cast<double>(p.width) * h / p.height));
    } else {
        opts.encode.width = p.width;
        opts.encode.height = p.height;
    }
    opts.encode.fps = cli.isSet(fps) ? cli.value(fps).toDouble() : p.fps;

    QElapsedTimer timer;
    timer.start();
    int lastPct = -1;
    engine::Exporter exporter;
    const bool ok = exporter.run(
        p, opts,
        [&](double f) {
            const int pct = static_cast<int>(f * 100);
            if (pct != lastPct) {
                lastPct = pct;
                std::fprintf(stderr, "\rRenderizando... %3d%%", pct);
            }
            return true;
        },
        &err);
    std::fprintf(stderr, "\n");
    if (!ok) {
        std::fprintf(stderr, "erro: %s\n", qPrintable(err));
        return 1;
    }
    std::printf("Exportado %s em %.1f s\n", qPrintable(args[1]), timer.elapsed() / 1000.0);
    return 0;
}
