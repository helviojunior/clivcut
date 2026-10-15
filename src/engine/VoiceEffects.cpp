// SPDX-License-Identifier: GPL-3.0-or-later
#include "VoiceEffects.h"

#include "AudioCleanup.h"
#include "Proxy.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>

namespace clv::engine {

namespace {
QMutex g_mutex;
QHash<QString, QString> g_effects;
QString key(const QString &path, const QString &id) { return path + QLatin1Char('|') + id; }
} // namespace

const std::vector<VoiceEffect> &voiceEffects()
{
    // Números com ponto: o processo usa o locale numérico "C" (ver main), que o FFmpeg exige.
    static const std::vector<VoiceEffect> effects{
        {QStringLiteral("echo"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "Echo")),
         "aecho=0.8:0.85:280|560:0.4|0.22"},
        {QStringLiteral("reverb"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "Hall reverb")),
         "aecho=0.8:0.9:43|67|97|131|173:0.32|0.28|0.24|0.2|0.16,aecho=0.7:0.8:241|331:0.2|0.14,lowpass=f=9000"},
        {QStringLiteral("robot"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "Robot")),
         // Zerar a fase deixa a voz metálica e ~11 dB mais baixa: compensa e limita.
         "afftfilt=real='hypot(re\\,im)*sin(0)':imag='hypot(re\\,im)*cos(0)':win_size=512:overlap=0.75,"
         "volume=3.5,alimiter=limit=0.95"},
        {QStringLiteral("high"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "High pitch")),
         "asetrate=64800,aresample=48000,atempo=0.740741"},
        {QStringLiteral("deep"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "Deep voice")),
         "asetrate=36000,aresample=48000,atempo=1.333333"},
        {QStringLiteral("telephone"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "Telephone")),
         "highpass=f=400,lowpass=f=3400,acompressor=threshold=0.125:ratio=4:makeup=2"},
        {QStringLiteral("radio"), QString::fromUtf8(QT_TRANSLATE_NOOP("VoiceEffects", "Old radio")),
         "highpass=f=200,lowpass=f=4500,acompressor=threshold=0.1:ratio=6:makeup=2,acrusher=bits=10:mode=log:mix=0.15"},
    };
    return effects;
}

const VoiceEffect *findVoiceEffect(const QString &id)
{
    for (const auto &e : voiceEffects())
        if (e.id == id)
            return &e;
    return nullptr;
}

QString effectPathFor(const QString &sourcePath, const QString &effectId)
{
    const QFileInfo fi(sourcePath);
    const QByteArray k = fi.absoluteFilePath().toUtf8() + '|' + QByteArray::number(fi.size()) + '|'
                         + QByteArray::number(fi.lastModified().toMSecsSinceEpoch()) + "|fx|" + effectId.toUtf8()
                         + '|' + (findVoiceEffect(effectId) ? findVoiceEffect(effectId)->filters : QByteArray());
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(k, QCryptographicHash::Sha1).toHex().left(16));
    const QString dir = QDir(QFileInfo(proxyCacheDir()).absolutePath()).filePath(QStringLiteral("audio"));
    return QDir(dir).filePath(QStringLiteral("%1_fx-%2_%3.wav").arg(fi.completeBaseName().left(40), effectId, hash));
}

bool generateEffect(const QString &sourcePath, const QString &effectId, const QString &outPath,
                    const std::function<bool(double)> &progress, QString *error)
{
    const VoiceEffect *fx = findVoiceEffect(effectId);
    if (!fx) {
        if (error)
            *error = QCoreApplication::translate("Engine", "Unknown voice effect: %1").arg(effectId);
        return false;
    }
    return renderAudioFilter(sourcePath, outPath, fx->filters, progress, error);
}

void registerEffect(const QString &sourcePath, const QString &effectId, const QString &outPath)
{
    QMutexLocker lock(&g_mutex);
    g_effects.insert(key(sourcePath, effectId), outPath);
}

QString registeredEffect(const QString &sourcePath, const QString &effectId)
{
    QMutexLocker lock(&g_mutex);
    return g_effects.value(key(sourcePath, effectId));
}

bool discoverEffect(const QString &sourcePath, const QString &effectId)
{
    const QString p = effectPathFor(sourcePath, effectId);
    if (QFileInfo(p).size() > 44) {
        registerEffect(sourcePath, effectId, p);
        return true;
    }
    return false;
}

} // namespace clv::engine
