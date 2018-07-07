#ifndef INGAMEMENUE_H
#define INGAMEMENUE_H

#include <QObject>
#include <QPoint>

#include "oxygine-framework.h"

class InGameMenue : public QObject, public oxygine::Actor
{
    Q_OBJECT
public:
    explicit InGameMenue(qint32 width = 20, qint32 heigth = 20);
    virtual ~InGameMenue();

signals:
    sigMouseWheel(qint32 direction);
    sigMoveMap(qint32 x, qint32 y);
public slots:
    void mouseWheel(qint32 direction);
    void MoveMap(qint32 x, qint32 y);
private:
    bool m_moveMap{false};
    QPoint m_MoveMapMousePoint;
};

#endif // INGAMEMENUE_H
