#pragma once

#include <QFrame>
#include <QList>
#include <QString>
#include <QTimer>
#include <memory>

class QHBoxLayout;
class QLabel;
class QPushButton;

namespace mixxx {
class CoreServices;
}

/// Small "Up Next" status-bar strip for the desktop / DeX UI.
///
/// Shows up to N suggestions for what should play next:
///   * tracks already queued in AutoDJ (highest priority), or
///   * recently-added library tracks (fallback when the queue is short).
///
/// Each pill is a `QPushButton`:
///   * left-click  → load on the next available deck;
///   * middle-click or context-menu → append to the AutoDJ queue.
///
/// The widget is dirt-cheap to keep on screen: it polls SQLite every 3 s with
/// two indexed queries, and pauses the timer when hidden (e.g. during full-
/// screen waveform mode). It also self-refreshes after any user action so the
/// just-loaded suggestion disappears immediately.
///
/// Lives in the bottom QStatusBar of `MixxxMainWindow`.
class WSuggestionsBar : public QFrame {
    Q_OBJECT

  public:
    explicit WSuggestionsBar(
            std::shared_ptr<mixxx::CoreServices> pCoreServices,
            QWidget* pParent = nullptr);
    ~WSuggestionsBar() override = default;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void refresh();

  private:
    struct Pick {
        int trackId = -1;
        QString title;
        QString artist;
        QString location;
        bool fromAutoDj = false;
    };

    QList<Pick> queryPicks(int limit) const;
    void loadOntoNextDeck(const Pick& pick);
    void appendToAutoDj(const Pick& pick);
    void rebuildButtons(const QList<Pick>& picks);

    std::shared_ptr<mixxx::CoreServices> m_pCoreServices;
    QHBoxLayout* m_pLayout = nullptr;
    QLabel* m_pHeader = nullptr;
    QList<QPushButton*> m_pills;
    QTimer m_refreshTimer;
    int m_pickCount = 3;
};
