// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Clipboard.h"
#include "core/History.h"
#include "core/Model.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>


#include <atomic>
#include <memory>
#include <mutex>

class AudioOutput;
class QLockFile;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;

/**
 * Ponte entre o modelo (clv::Project) e a interface QML: edição, seleção,
 * cursor de reprodução, histórico, arquivos, importação e exportação.
 *
 * Tempos expostos ao QML são em µs (double; exato até 2^53).
 * Exposto ao QML como o singleton `Project` (registrado em main.cpp).
 */
class ProjectController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Project)
    QML_SINGLETON

    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName NOTIFY projectChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth NOTIFY projectChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight NOTIFY projectChanged)
    Q_PROPERTY(double fps READ fps NOTIFY projectChanged)
    Q_PROPERTY(QString background READ background NOTIFY projectChanged)
    Q_PROPERTY(double duration READ duration NOTIFY projectChanged)
    Q_PROPERTY(bool magnet READ magnet WRITE setMagnet NOTIFY projectChanged)
    Q_PROPERTY(bool snapping READ snapping WRITE setSnapping NOTIFY snappingChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY projectChanged)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY assetsChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY projectChanged)
    Q_PROPERTY(double playhead READ playhead WRITE seek NOTIFY playheadChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QStringList selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedClip READ selectedClip NOTIFY selectedClipChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY historyChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY historyChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportChanged)
    Q_PROPERTY(QStringList videoCodecs READ videoCodecs CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    // Legendas automáticas
    Q_PROPERTY(bool transcriptionAvailable READ transcriptionAvailable CONSTANT)
    Q_PROPERTY(QVariantList captionModels READ captionModels NOTIFY captionModelsChanged)
    Q_PROPERTY(QString downloadingModel READ downloadingModel NOTIFY captionModelsChanged)
    Q_PROPERTY(double modelDownloadProgress READ modelDownloadProgress NOTIFY modelDownloadProgressChanged)
    Q_PROPERTY(bool transcribing READ transcribing NOTIFY transcribeChanged)
    Q_PROPERTY(double transcribeProgress READ transcribeProgress NOTIFY transcribeChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY projectChanged)
    Q_PROPERTY(QVariantList filters READ filters CONSTANT)
    Q_PROPERTY(QVariantList speedPresets READ speedPresets CONSTANT)
    /// Aba inicial do inspetor para capturas automáticas (CLIVCUT_SCREENSHOT_PAGE).
    Q_PROPERTY(QString screenshotPage READ screenshotPage CONSTANT)
    Q_PROPERTY(bool useProxies READ useProxies WRITE setUseProxies NOTIFY proxiesChanged)
    Q_PROPERTY(int proxiesPending READ proxiesPending NOTIFY proxiesChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(bool normalizing READ normalizing NOTIFY audioJobsChanged)
    Q_PROPERTY(bool canPasteAttributes READ canPasteAttributes NOTIFY clipboardChanged)

public:
    /// Sem construtor padrão: o QML obtém a instância única via create().
    explicit ProjectController(QObject *parent);
    ~ProjectController() override;

    static ProjectController *instance();
    /// Fábrica do singleton QML: devolve a instância criada em main().
    static ProjectController *create(QQmlEngine *, QJSEngine *);

    /// Cópia imutável do projeto para outras threads (render, áudio).
    std::shared_ptr<const clv::Project> snapshot() const;

    QString projectName() const { return m_project.name; }
    void setProjectName(const QString &name);
    QString filePath() const { return m_filePath; }
    bool modified() const { return m_history.revision() != m_savedRevision; }
    int canvasWidth() const { return m_project.width; }
    int canvasHeight() const { return m_project.height; }
    double fps() const { return m_project.fps; }
    QString background() const { return m_project.background; }
    double duration() const { return static_cast<double>(m_project.duration()); }
    bool magnet() const { return m_project.mainTrackMagnet; }
    void setMagnet(bool on);
    bool snapping() const { return m_snapping; }
    void setSnapping(bool on);
    QVariantList tracks() const;
    QVariantList assets() const;
    QVariantList markers() const;
    double playhead() const { return static_cast<double>(m_playhead); }
    bool playing() const { return m_playing; }
    QStringList selection() const { return m_selection; }
    QVariantMap selectedClip() const;
    bool canUndo() const { return m_history.canUndo(); }
    bool canRedo() const { return m_history.canRedo(); }
    QString undoText() const { return m_history.undoLabel(); }
    QString redoText() const { return m_history.redoLabel(); }
    int revision() const { return m_revisionCounter; }
    double exportProgress() const { return m_exportProgress; }
    bool exporting() const { return m_exporting; }
    QStringList videoCodecs() const;
    QString statusMessage() const { return m_status; }
    bool transcriptionAvailable() const;
    QVariantList captionModels() const;
    QString downloadingModel() const { return m_downloadingModel; }
    double modelDownloadProgress() const { return m_modelDownloadProgress; }
    bool transcribing() const { return m_transcribing; }
    double transcribeProgress() const { return m_transcribeProgress; }
    QVariantList audioTracks() const;
    QVariantList filters() const;
    QVariantList speedPresets() const;
    QString screenshotPage() const { return qEnvironmentVariable("CLIVCUT_SCREENSHOT_PAGE"); }
    bool canPaste() const;
    bool normalizing() const { return m_normalizing; }
    bool canPasteAttributes() const { return m_attributeSource.has_value(); }
    bool useProxies() const { return m_useProxies; }
    void setUseProxies(bool on);
    int proxiesPending() const { return m_proxyQueue.size() + (m_proxyBuilding.isEmpty() ? 0 : 1); }

    // --- arquivos ---
    Q_INVOKABLE void newProject(int width, int height, double fps);
    Q_INVOKABLE bool openProject(const QUrl &url);
    Q_INVOKABLE bool saveProject();
    Q_INVOKABLE bool saveProjectAs(const QUrl &url);
    Q_INVOKABLE bool importDraft(const QUrl &url);
    Q_INVOKABLE void importMedia(const QList<QUrl> &urls);
    Q_INVOKABLE void setCanvas(int width, int height);
    Q_INVOKABLE void setBackground(const QString &color);
    Q_INVOKABLE void relinkAsset(const QString &assetId, const QUrl &url);

    // --- recuperação ---
    /// Cópias de recuperação deixadas por sessões que não fecharam normalmente.
    Q_INVOKABLE QVariantList recoverableProjects() const;
    Q_INVOKABLE bool recoverProject(const QString &recoveryFile);
    Q_INVOKABLE void discardRecovery(const QString &recoveryFile);
    /// Grava a cópia de recuperação agora (se houver alterações).
    Q_INVOKABLE void autosaveNow();
    /// Abandona as alterações (ao fechar sem salvar): apaga a cópia de recuperação.
    Q_INVOKABLE void discardChanges();
    static QString recoveryDir();

    // --- timeline ---
    Q_INVOKABLE void addAssetToTimeline(const QString &assetId, int trackIndex = -1, double start = -1);
    Q_INVOKABLE void addText(const QString &text = {}, const QString &preset = {});
    Q_INVOKABLE void splitAtPlayhead();
    Q_INVOKABLE void deleteSelection(bool ripple = false);
    Q_INVOKABLE void deleteLeft();
    Q_INVOKABLE void deleteRight();
    Q_INVOKABLE void duplicateSelection();
    Q_INVOKABLE bool moveClip(const QString &clipId, int trackIndex, double start);
    Q_INVOKABLE bool trimClip(const QString &clipId, bool startEdge, double time);
    Q_INVOKABLE double snapTime(double t, const QStringList &ignore, double thresholdUs) const;
    Q_INVOKABLE void setTrackFlag(int trackIndex, const QString &flag, bool value);
    Q_INVOKABLE void addMarker();
    Q_INVOKABLE void freezeFrame();
    /// Separa o áudio dos clipes de vídeo selecionados em clipes de áudio.
    Q_INVOKABLE void detachAudio();
    /// Mede a loudness (EBU R128) dos clipes selecionados e ajusta o ganho para o alvo (LUFS).
    Q_INVOKABLE void normalizeLoudness(double targetLufs);

    // --- seleção e propriedades ---
    Q_INVOKABLE void select(const QString &clipId, bool additive = false);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setClipProperty(const QString &clipId, const QString &key, const QVariant &value);
    /// Várias propriedades numa única edição (um passo de desfazer).
    Q_INVOKABLE void setClipProperties(const QString &clipId, const QVariantMap &values);
    Q_INVOKABLE void toggleKeyframe(const QString &clipId, const QString &prop);
    /// Keyframes de uma propriedade: [{t, v, ease, bx1, by1, bx2, by2}] (t relativo ao clipe).
    Q_INVOKABLE QVariantList keyframes(const QString &clipId, const QString &prop) const;
    /// Valores amostrados ao longo do clipe (para desenhar o gráfico).
    Q_INVOKABLE QVariantList sampleKeyframes(const QString &clipId, const QString &prop, int samples) const;
    Q_INVOKABLE void moveKeyframe(const QString &clipId, const QString &prop, double fromT, double toT, double value);
    Q_INVOKABLE void setKeyframeEasing(const QString &clipId, const QString &prop, double t, const QString &ease);
    Q_INVOKABLE void setKeyframeBezier(const QString &clipId, const QString &prop, double t, double x1, double y1,
                                       double x2, double y2);
    Q_INVOKABLE void setEasingForAll(const QString &clipId, const QString &prop, const QString &ease);
    Q_INVOKABLE void setTransition(const QString &clipId, const QString &kind, double durationUs);
    Q_INVOKABLE void setAnimation(const QString &clipId, bool in, const QString &kind, double durationUs);
    Q_INVOKABLE void setSpeed(const QString &clipId, double speed);
    /// Curva de velocidade [{u, v}] (lista vazia volta à velocidade constante).
    Q_INVOKABLE void setSpeedCurve(const QString &clipId, const QVariantList &points);
    Q_INVOKABLE void applySpeedPreset(const QString &clipId, const QString &presetId);
    Q_INVOKABLE void resetClipProperties(const QString &clipId, const QString &group);

    // --- histórico ---
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // --- reprodução ---
    Q_INVOKABLE void seek(double t);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void stepFrames(int n);
    Q_INVOKABLE void jumpToEdge(bool end);

    // --- export ---
    Q_INVOKABLE void exportVideo(const QUrl &url, int height, double fps, const QString &codec, int crf);
    Q_INVOKABLE void cancelExport();

    // --- legendas ---
    Q_INVOKABLE void downloadModel(const QString &id);
    Q_INVOKABLE void cancelModelDownload();
    Q_INVOKABLE void generateCaptions(const QString &modelId, const QString &language, int trackIndex, bool replace);
    Q_INVOKABLE void cancelTranscription();
    Q_INVOKABLE bool importSubtitles(const QUrl &url);
    Q_INVOKABLE bool exportSubtitles(const QUrl &url);
    Q_INVOKABLE void applyStyleToTrack(const QString &clipId);

    // --- filtros e ajustes ---
    /// Aplica um filtro embutido ao clipe (vazio remove).
    Q_INVOKABLE void applyFilter(const QString &clipId, const QString &filterId);
    /// Adiciona um clipe de filtro/ajuste no cursor (afeta tudo abaixo).
    Q_INVOKABLE void addFilterClip(const QString &filterId);
    Q_INVOKABLE bool loadLut(const QString &clipId, const QUrl &url);
    /// Quadro de referência para prévias de filtro: "assetId/tempoµs" (vazio se não houver).
    Q_INVOKABLE QString previewFrameRef() const;

    // --- área de transferência e grupos ---
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void cutSelection();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void copyAttributes();
    Q_INVOKABLE void pasteAttributesToSelection();
    Q_INVOKABLE void groupSelection();
    Q_INVOKABLE void ungroupSelection();
    /// Seleciona clipes que tocam [t0, t1] nas faixas dadas (seleção por retângulo).
    Q_INVOKABLE void selectInRange(double t0, double t1, const QVariantList &trackIndices, bool additive);
    Q_INVOKABLE void selectAll();

    // --- proxies ---
    Q_INVOKABLE void generateProxy(const QString &assetId);
    Q_INVOKABLE void cancelProxies();

    // --- utilitários para QML ---
    Q_INVOKABLE QString formatTime(double us) const;
    Q_INVOKABLE QString assetPath(const QString &assetId) const;

signals:
    void projectChanged();
    void assetsChanged();
    void fileChanged();
    void modifiedChanged();
    void playheadChanged();
    void playingChanged();
    void selectionChanged();
    void selectedClipChanged();
    void historyChanged();
    void revisionChanged();
    void snappingChanged();
    void exportChanged();
    void exportFinished(bool ok, const QString &message);
    void statusMessageChanged();
    void errorOccurred(const QString &message);
    void captionModelsChanged();
    void modelDownloadProgressChanged();
    void transcribeChanged();
    void proxiesChanged();
    void clipboardChanged();
    void audioJobsChanged();

private:
    /** Executa uma edição com registro no histórico. `coalesceKey` agrupa edições contínuas. */
    template <typename F>
    bool edit(const QString &label, F &&fn, const QString &coalesceKey = {});
    void commitChange();
    void setProject(clv::Project p, const QString &path);
    void setStatus(const QString &msg);
    void tick();
    void refreshProxies();
    void refreshDenoise();
    void startNextDenoise();
    void enqueueProxy(const QString &mediaPath);
    void startNextProxy();
    QString defaultTextFor(const QString &preset) const;

    clv::Project m_project;
    clv::History m_history;
    QString m_filePath;
    int m_savedRevision = 0;
    int m_revisionCounter = 0;
    QStringList m_selection;
    QString m_primary; ///< último clipe clicado (inspetor e "copiar atributos" em seleções múltiplas)
    QString primaryClipId() const;
    bool m_snapping = true;

    clv::Micros m_playhead = 0;
    bool m_playing = false;
    QTimer m_ticker;
    QElapsedTimer m_clock;
    clv::Micros m_playStart = 0;
    std::unique_ptr<AudioOutput> m_audio;

    QTimer m_autosave;
    std::unique_ptr<QLockFile> m_recoveryLock; ///< marca a cópia como "em uso" por esta sessão
    int m_autosavedRevision = 0;
    QString recoveryFileFor(const QString &projectId) const;
    void removeRecovery();

    QString m_lastCoalesceKey;
    QElapsedTimer m_coalesceTimer;

    mutable std::mutex m_snapMutex;
    std::shared_ptr<const clv::Project> m_snapshot;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_modelReply = nullptr;
    QString m_downloadingModel;
    double m_modelDownloadProgress = 0;
    std::atomic<bool> m_cancelTranscription{false};
    bool m_transcribing = false;
    double m_transcribeProgress = 0;

    bool m_normalizing = false;
    QStringList m_denoiseQueue;     ///< "caminho|intensidade" aguardando limpeza
    QString m_denoiseBuilding;
    double m_denoiseProgress = 0;
    std::shared_ptr<std::atomic<bool>> m_denoiseCancel = std::make_shared<std::atomic<bool>>(false);
    clv::Clipboard m_clipboard;
    std::optional<clv::Clip> m_attributeSource;
    bool m_useProxies = true;
    QStringList m_proxyQueue;       ///< caminhos de mídia aguardando proxy
    QString m_proxyBuilding;        ///< caminho em geração
    double m_proxyProgress = 0;
    std::shared_ptr<std::atomic<bool>> m_proxyCancel = std::make_shared<std::atomic<bool>>(false);

    std::atomic<bool> m_cancelExport{false};
    bool m_exporting = false;
    double m_exportProgress = 0;
    QString m_status;
};
