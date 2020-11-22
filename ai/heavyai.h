#ifndef HEAVYAI_H
#define HEAVYAI_H

#include <QVector>
#include <QTimer>

#include "ai/coreai.h"
#include "ai/influencefrontmap.h"
#include "game/unitpathfindingsystem.h"

class HeavyAi : public CoreAI
{
    Q_OBJECT
public:
    struct UnitData
    {
        Unit* m_pUnit;
        spUnitPathFindingSystem m_pPfs;
        qint32 m_movepoints{0};
        float m_virtualDamage{0.0f};
    };
    explicit HeavyAi();
    virtual ~HeavyAi() = default;
public slots:
    /**
     * @brief process
     */
    virtual void process() override;
    /**
     * @brief readIni
     * @param name
     */
    virtual void readIni(QString name) override;

    void toggleAiPause();

    void showFrontMap();
    void showFrontLines();
    void hideFrontMap();
protected:

private:
    void setupTurn();
    void createIslandMaps();
    void initUnits(QmlVectorUnit* pUnits, QVector<UnitData> & units, bool enemyUnits);
    void updateUnits();
    void updateUnits(QVector<UnitData> & units, bool enemyUnits);
private:
    QVector<UnitData> m_enemyUnits;
    QVector<UnitData> m_ownUnits;
    QVector<QPoint> m_updatePoints;
    InfluenceFrontMap m_InfluenceFrontMap;

    spQmlVectorUnit m_pUnits = nullptr;
    spQmlVectorUnit m_pEnemyUnits = nullptr;

    QTimer m_timer;
    bool m_pause{false};

    static const qint32 minSiloDamage;
};

#endif // HEAVYAI_H
