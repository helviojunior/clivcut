// SPDX-License-Identifier: GPL-3.0-or-later
#include "DraftImport.h"

#include "Edit.h"
#include "SpeedCurve.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace clv {

namespace {

Micros us(const QJsonValue &v)
{
    return static_cast<Micros>(v.toDouble(0));
}

QString rgbToHex(const QJsonArray &rgb)
{
    if (rgb.size() < 3)
        return QStringLiteral("#ffffff");
    auto c = [&](int i) { return std::clamp(static_cast<int>(std::lround(rgb[i].toDouble() * 255)), 0, 255); };
    return QStringLiteral("#%1%2%3")
        .arg(c(0), 2, 16, QLatin1Char('0'))
        .arg(c(1), 2, 16, QLatin1Char('0'))
        .arg(c(2), 2, 16, QLatin1Char('0'));
}

/** Resolve o caminho de mídia salvo no draft (placeholder da pasta, barras do Windows). */
QString resolvePath(QString path, const QString &draftDir)
{
    static const QRegularExpression placeholder(QStringLiteral("^##_draftpath_placeholder_[^#]*_##"));
    path.replace(placeholder, draftDir);
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    static const QRegularExpression windowsAbs(QStringLiteral("^[A-Za-z]:/"));
    if (!path.isEmpty() && QFileInfo(path).isRelative() && !windowsAbs.match(path).hasMatch() && !draftDir.isEmpty())
        path = QDir(draftDir).filePath(path);
    return path;
}

Easing easingFromCurve(const QString &curve)
{
    if (curve == QLatin1String("EaseIn"))
        return Easing::EaseIn;
    if (curve == QLatin1String("EaseOut"))
        return Easing::EaseOut;
    if (curve.startsWith(QLatin1String("FreeCurve")) || curve == QLatin1String("EaseInOut"))
        return Easing::EaseInOut;
    return Easing::Linear;
}

struct MaterialRef {
    QString category;
    QJsonObject obj;
};

} // namespace

DraftImportResult importDraftJson(const QByteArray &json, const QString &draftDir)
{
    DraftImportResult r;
    const QByteArray trimmed = json.trimmed();
    if (trimmed.isEmpty() || trimmed.at(0) != '{') {
        r.error = QCoreApplication::translate("Core", "The draft appears to be encrypted (it is not plain-text JSON). This format is not supported; export the project without encryption.");
        return r;
    }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QCoreApplication::translate("Core", "Invalid draft JSON: %1").arg(pe.errorString());
        return r;
    }
    const QJsonObject root = doc.object();
    if (!root.contains(QLatin1String("tracks")) || !root.contains(QLatin1String("materials"))) {
        r.error = QCoreApplication::translate("Core", "The file does not look like a draft JSON (tracks/materials are missing).");
        return r;
    }

    Project p;
    p.id = newId("prj");
    p.name = root.value("name").toString();
    if (p.name.isEmpty())
        p.name = QFileInfo(draftDir).fileName();
    if (p.name.isEmpty())
        p.name = QStringLiteral("Draft importado");
    const auto canvas = root.value("canvas_config").toObject();
    p.width = canvas.value("width").toInt(1920);
    p.height = canvas.value("height").toInt(1080);
    p.fps = root.value("fps").toDouble(30);
    if (p.fps <= 0)
        p.fps = 30;

    // Índice global id -> material (todas as categorias).
    std::unordered_map<QString, MaterialRef> index;
    const auto materials = root.value("materials").toObject();
    for (auto it = materials.begin(); it != materials.end(); ++it) {
        if (!it.value().isArray())
            continue;
        for (const auto &v : it.value().toArray()) {
            const auto o = v.toObject();
            const QString id = o.value("id").toString();
            if (!id.isEmpty())
                index[id] = {it.key(), o};
        }
    }

    // Mídias: videos (vídeo/foto) e audios viram MediaAssets.
    auto ensureAsset = [&](const MaterialRef &m) -> QString {
        const QString id = m.obj.value("id").toString();
        if (p.assets.count(id))
            return id;
        MediaAsset a;
        a.id = id;
        a.path = resolvePath(m.obj.value("path").toString(), draftDir);
        a.name = m.obj.value("material_name").toString(m.obj.value("name").toString(QFileInfo(a.path).fileName()));
        a.duration = us(m.obj.value("duration"));
        if (m.category == QLatin1String("audios")) {
            a.kind = MediaKind::Audio;
            a.hasAudio = true;
        } else {
            const bool photo = m.obj.value("type").toString() == QLatin1String("photo");
            a.kind = photo ? MediaKind::Image : MediaKind::Video;
            a.hasVideo = true;
            a.hasAudio = !photo && m.obj.value("has_audio").toBool(true);
            a.width = m.obj.value("width").toInt();
            a.height = m.obj.value("height").toInt();
            if (photo)
                a.duration = 0;
        }
        if (!a.path.isEmpty() && !QFileInfo::exists(a.path))
            r.warnings << QCoreApplication::translate("Core", "Media not found (it will need to be relinked): %1").arg(a.path);
        p.assets.emplace(id, a);
        return id;
    };

    std::vector<Track> visualVideo, audio, text;
    for (const auto &tv : root.value("tracks").toArray()) {
        const auto to = tv.toObject();
        const QString type = to.value("type").toString();
        Track track;
        if (type == QLatin1String("video"))
            track = createTrack(TrackKind::Video);
        else if (type == QLatin1String("audio"))
            track = createTrack(TrackKind::Audio);
        else if (type == QLatin1String("text"))
            track = createTrack(TrackKind::Text);
        else {
            r.warnings << QCoreApplication::translate("Core", "Track of type \"%1\" ignored (proprietary effects/stickers are not supported).")
                              .arg(type);
            continue;
        }
        const int attribute = to.value("attribute").toInt();
        track.muted = attribute & 1; // INFERIDO: bit 0 = mudo

        for (const auto &sv : to.value("segments").toArray()) {
            const auto so = sv.toObject();
            const auto mit = index.find(so.value("material_id").toString());
            if (mit == index.end()) {
                r.warnings << QCoreApplication::translate("Core", "Segment %1 has no material; ignored.").arg(so.value("id").toString());
                continue;
            }
            const MaterialRef &mat = mit->second;
            const auto target = so.value("target_timerange").toObject();
            Clip c;
            c.start = us(target.value("start"));
            c.duration = us(target.value("duration"));
            if (c.duration <= 0)
                continue;

            if (mat.category == QLatin1String("texts")) {
                c.type = ClipType::Text;
                c.id = newId("txt");
                const QString content = mat.obj.value("content").toString();
                const auto cdoc = QJsonDocument::fromJson(content.toUtf8());
                if (cdoc.isObject()) {
                    const auto co = cdoc.object();
                    c.text = co.value("text").toString();
                    const auto styles = co.value("styles").toArray();
                    if (!styles.isEmpty()) {
                        const auto s = styles.first().toObject();
                        // O tamanho no draft é relativo; ~15 equivale ao tamanho padrão.
                        if (s.contains("size"))
                            c.style.fontSize = s.value("size").toDouble() * 4.8;
                        c.style.bold = s.value("bold").toBool(false);
                        c.style.italic = s.value("italic").toBool(false);
                        const auto solid = s.value("fill").toObject().value("content").toObject().value("solid").toObject();
                        if (solid.contains("color"))
                            c.style.color = rgbToHex(solid.value("color").toArray());
                        const auto strokes = s.value("strokes").toArray();
                        if (strokes.isEmpty()) {
                            c.style.strokeWidth = 0;
                        } else {
                            const auto st = strokes.first().toObject();
                            c.style.strokeColor = rgbToHex(st.value("content").toObject().value("solid").toObject().value("color").toArray());
                            c.style.strokeWidth = st.value("width").toDouble(0.08) * 50;
                        }
                    }
                } else {
                    c.text = content; // drafts antigos: texto puro
                }
                if (c.text.isEmpty())
                    c.text = mat.obj.value("text").toString();
            } else if (mat.category == QLatin1String("videos") || mat.category == QLatin1String("audios")) {
                c.type = ClipType::Media;
                c.id = newId("clp");
                c.assetId = ensureAsset(mat);
                const auto source = so.value("source_timerange").toObject();
                c.sourceIn = us(source.value("start"));
                c.speed = so.value("speed").toDouble(1);
                if (c.speed <= 0)
                    c.speed = 1;
                c.reverse = so.value("reverse").toBool();
                c.volume = so.value("volume").toDouble(1);
            } else {
                r.warnings << QCoreApplication::translate("Core", "Unsupported material \"%1\"; segment ignored.").arg(mat.category);
                continue;
            }

            // Transformação: no draft +y aponta para cima; no clivcut, para baixo.
            const auto clip = so.value("clip").toObject();
            if (!clip.isEmpty()) {
                c.transform.opacity = clip.value("alpha").toDouble(1);
                c.transform.rotation = clip.value("rotation").toDouble(0);
                const auto sc = clip.value("scale").toObject();
                c.transform.scale = sc.value("x").toDouble(1);
                const auto tr = clip.value("transform").toObject();
                c.transform.x = tr.value("x").toDouble(0);
                c.transform.y = -tr.value("y").toDouble(0);
                const auto flip = clip.value("flip").toObject();
                c.transform.flipH = flip.value("horizontal").toBool();
                c.transform.flipV = flip.value("vertical").toBool();
                if (std::abs(sc.value("x").toDouble(1) - sc.value("y").toDouble(1)) > 1e-6)
                    r.warnings << QCoreApplication::translate("Core", "Non-uniform scale simplified to uniform in clip %1.").arg(c.id);
            }

            // Keyframes (modelo atual: common_keyframes).
            for (const auto &kv : so.value("common_keyframes").toArray()) {
                const auto ko = kv.toObject();
                const QString prop = ko.value("property_type").toString();
                int pi = -1;
                double sign = 1;
                if (prop == QLatin1String("KFTypePositionX")) pi = static_cast<int>(Prop::X);
                else if (prop == QLatin1String("KFTypePositionY")) { pi = static_cast<int>(Prop::Y); sign = -1; }
                else if (prop == QLatin1String("KFTypeScaleX")) pi = static_cast<int>(Prop::Scale);
                else if (prop == QLatin1String("KFTypeRotation")) pi = static_cast<int>(Prop::Rotation);
                else if (prop == QLatin1String("KFTypeAlpha") || prop == QLatin1String("KFTypeGlobalAlpha")) pi = static_cast<int>(Prop::Opacity);
                else if (prop == QLatin1String("KFTypeVolume")) pi = static_cast<int>(Prop::Volume);
                if (pi < 0)
                    continue;
                auto &list = c.keyframes[pi];
                for (const auto &kfv : ko.value("keyframe_list").toArray()) {
                    const auto k = kfv.toObject();
                    const auto values = k.value("values").toArray();
                    if (values.isEmpty())
                        continue;
                    list.push_back({us(k.value("time_offset")), values.first().toDouble() * sign,
                                    easingFromCurve(k.value("curveType").toString())});
                }
                std::sort(list.begin(), list.end(), [](const Keyframe &a, const Keyframe &b) { return a.t < b.t; });
            }

            // Materiais secundários: fades de áudio e transições.
            for (const auto &rv : so.value("extra_material_refs").toArray()) {
                const auto rit = index.find(rv.toString());
                if (rit == index.end())
                    continue;
                const auto &ref = rit->second;
                if (ref.category == QLatin1String("speeds")) {
                    // Curva de velocidade (mode 1): pontos {x: posição 0..1, y: velocidade}.
                    const auto curve = ref.obj.value("curve_speed").toObject().value("speed_points").toArray();
                    if (ref.obj.value("mode").toInt() == 1 && curve.size() >= 2) {
                        for (const auto &pv : curve) {
                            const auto pt = pv.toObject();
                            c.speedCurve.push_back({pt.value("x").toDouble(), pt.value("y").toDouble(1)});
                        }
                        c.speedCurve = normalizedCurve(c.speedCurve);
                        // Mantém o trecho de origem: velocidade = média da curva.
                        c.speed = static_cast<double>(us(so.value("source_timerange").toObject().value("duration"))) /
                                  std::max<Micros>(1, c.duration);
                    }
                } else if (ref.category == QLatin1String("audio_fades")) {
                    c.fadeIn = us(ref.obj.value("fade_in_duration"));
                    c.fadeOut = us(ref.obj.value("fade_out_duration"));
                } else if (ref.category == QLatin1String("transitions")) {
                    // INFERIDO: a transição fica no segmento que sai; no clivcut ela é
                    // guardada no clipe seguinte (transitionIn). Ajustada abaixo.
                    c.transitionIn = Transition{TransitionKind::Dissolve, us(ref.obj.value("duration"))};
                    if (c.transitionIn->duration <= 0)
                        c.transitionIn->duration = 500'000;
                }
            }
            track.clips.push_back(std::move(c));
        }
        edit::sortTrack(track);
        // Move transições do clipe que sai para o que entra.
        for (int i = static_cast<int>(track.clips.size()) - 1; i >= 0; --i) {
            auto &tr = track.clips[i].transitionIn;
            if (!tr)
                continue;
            if (i + 1 < static_cast<int>(track.clips.size()))
                track.clips[i + 1].transitionIn = tr;
            tr.reset();
        }
        if (track.kind == TrackKind::Video)
            visualVideo.push_back(std::move(track));
        else if (track.kind == TrackKind::Audio)
            audio.push_back(std::move(track));
        else
            text.push_back(std::move(track));
    }

    // Ordem do clivcut: principal (1º vídeo), demais vídeos, textos por cima, áudios.
    if (visualVideo.empty())
        visualVideo.push_back(createTrack(TrackKind::Video));
    visualVideo.front().name = QStringLiteral("Principal");
    for (auto &t : visualVideo)
        p.tracks.push_back(std::move(t));
    for (auto &t : text)
        p.tracks.push_back(std::move(t));
    for (auto &t : audio)
        p.tracks.push_back(std::move(t));

    // A faixa principal só fica magnética se não tiver buracos (preserva o tempo original).
    Micros cursor = 0;
    bool contiguous = true;
    for (const auto &c : p.tracks.front().clips) {
        if (c.start != cursor)
            contiguous = false;
        cursor = c.end();
    }
    p.mainTrackMagnet = contiguous;
    r.project = std::move(p);
    r.ok = true;
    return r;
}

DraftImportResult importDraft(const QString &path)
{
    QFileInfo fi(path);
    QString file = path;
    QString dir = fi.absolutePath();
    if (fi.isDir()) {
        dir = fi.absoluteFilePath();
        const QStringList candidates{QStringLiteral("draft_content.json"), QStringLiteral("draft_info.json")};
        file.clear();
        for (const auto &name : candidates) {
            const QString f = QDir(dir).filePath(name);
            if (QFileInfo::exists(f)) {
                file = f;
                break;
            }
        }
        if (file.isEmpty()) {
            DraftImportResult r;
            r.error = QCoreApplication::translate("Core", "The folder contains neither draft_content.json nor draft_info.json.");
            return r;
        }
    }
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        DraftImportResult r;
        r.error = QCoreApplication::translate("Core", "Could not open %1: %2").arg(file, f.errorString());
        return r;
    }
    return importDraftJson(f.readAll(), dir);
}

} // namespace clv
