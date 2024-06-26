#ifndef REPLAYMENU_H
#define REPLAYMENU_H

#include <QMutex>
#include <QObject>

#include "menue/gamemenue.h"

#include "objects/base/v_scrollbar.h"

#include "gameinput/humanplayerinput.h"
#include "game/viewplayer.h"
#include "game/gameanimation/animationskipper.h"
#include "game/gamerecording/iReplayReader.h"

class ReplayMenu;
using spReplayMenu = std::shared_ptr<ReplayMenu>;

class ReplayMenu final : public GameMenue
{
    Q_OBJECT
    static constexpr qint32 actionPixelSize = 5;
public:
    struct PlayerUiInfo
    {
        QString playerArmy{""};
        QColor color;
    };

    ReplayMenu(QString filename);
    virtual ~ReplayMenu();
    /**
     * @brief getCurrentViewPlayer
     * @return
     */
    virtual Player* getCurrentViewPlayer() const override;
    /**
     * @brief getValid
     * @return
     */
    bool getValid() const;

    void updatePlayerUiData();
    void fetchPlayerUiData();
    Q_INVOKABLE Viewplayer* getViewplayer();

signals:
    void sigShowRecordInvalid();
    void sigExitReplay();
    void sigSwapPlay();
    void sigStartFastForward();
    void sigStopFastForward();
    void sigShowConfig();
    void sigOneStep();
    void sigRewindDay();
    void sigRewindOneStep();
public slots:
    void startReplay();
    void showRecordInvalid();
    void exitReplay();
    void nextReplayAction();
    virtual void showExitGame() override;
    void swapPlay();
    void startFastForward();
    void stopFastForward();
    void showConfig();
    void setViewTeam(qint32 item);
    /**
     * @brief startSeeking
     */
    void startSeeking();
    /**
     * @brief seekRecord
     * @param value
     */
    void seekRecord(float value);
    /**
     * @brief seekChanged
     * @param value
     */
    void seekChanged(float value);
    /**
     * @brief togglePlayUi
     */
    void togglePlayUi();
    /**
     * @brief oneStep
     */
    void oneStep();
    /**
     * @brief rewind
     */
    void rewindDay();
    /**
     * @brief rewindOneStep
     */
    void rewindOneStep();
protected slots:
    virtual void onEnter() override;
    /**
     * @brief keyInput
     * @param event
     */
    virtual void keyInput(oxygine::KeyEvent event) override;
protected:
    /**
     * @brief loadUIButtons
     */
    void loadUIButtons();
    /**
     * @brief loadSeekUi
     */
    void loadSeekUi();
    /**
     * @brief seekToDay
     * @param day
     */
    void seekToDay(IReplayReader::DayInfo dayInfo);
    void endOneStepRewind();
private:
    bool m_paused{false};
    bool m_uiPause{false};
    bool m_pauseRequested{false};
    quint32 m_replayCounter{0};
    spV_Scrollbar m_progressBar;
    QRecursiveMutex m_replayMutex;
    oxygine::spButton m_playButton;
    oxygine::spButton m_pauseButton;
    oxygine::spButton m_fastForwardButton;
    oxygine::spButton m_oneStepButton;
    oxygine::spButton m_rewindDayButton;
    oxygine::spButton m_rewindOneStepButton;
    oxygine::spButton m_configButton;
    oxygine::spBox9Sprite m_taskBar;

    AnimationSkipper m_storedSeekingAnimationSettings;
    AnimationSkipper m_storedAnimationSettings;


    spHumanPlayerInput m_HumanInput;
    spViewplayer m_Viewplayer;

    qint64 m_lastRewind = 0;
    qint32 m_rewindTarget{-1};
    bool m_rewindPause{false};
    qint32 m_rewindReplayCounter{-1};

    bool m_seeking{false};
    bool m_valid{false};
    oxygine::spActor m_seekActor;
    spLabel m_seekDayLabel;
    spIReplayReader m_replayReader;
    QVector<PlayerUiInfo> m_playerUiInfo;
};

#endif // REPLAYMENU_H
