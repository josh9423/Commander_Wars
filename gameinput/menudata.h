#ifndef MENUDATA_H
#define MENUDATA_H


#include "3rd_party/oxygine-framework/oxygine-framework.h"
#include <QObject>
#include <QStringList>
#include <QVector>

class Unit;

class MenuData;
typedef oxygine::intrusive_ptr<MenuData> spMenuData;
class MenuData : public QObject, public oxygine::ref_counter
{
    Q_OBJECT
public:
    explicit MenuData();

    QStringList getTexts()
    {
        return texts;
    }
    QStringList getActionIDs()
    {
        return actionIDs;
    }
    QVector<qint32> getCostList()
    {
        return costList;
    }
    QVector<bool> getEnabledList()
    {
        return enabledList;
    }
    QVector<oxygine::spActor> getIconList()
    {
        return iconList;
    }
signals:

public slots:
    /**
     * @brief addData adds data to a later shown menu
     * @param text the text shown in the menu
     * @param actionID the id written into the action data to identify which menu item was selected
     * @param icon the unique string representing the icon to be shown currently units and normal icons are supported
     * @param costs the costs given to the action when executing the given action
     * @param enabled if the menu item is selectable or not
     */
    void addData(QString text, QString actionID, QString icon, qint32 costs = 0, bool enabled = true);
    /**
     * @brief addData adds data to a later shown menu
     * @param text the text shown in the menu
     * @param actionID the id written into the action data to identify which menu item was selected
     * @param pIcon the unit that will be shown including it's current ammo, fuel and hp stats
     * @param costs the costs given to the action when executing the given action
     * @param enabled if the menu item is selectable or not
     */
    void addUnitData(QString text, QString actionID, Unit* pIcon, qint32 costs = 0, bool enabled = true);
    /**
     * @brief validData verifies if the menu data is valid. In case of wrongly used javascript implementation
     * @return
     */
    bool validData();

private:
    QStringList texts;
    QStringList actionIDs;
    QVector<qint32> costList;
    QVector<oxygine::spActor> iconList;
    QVector<bool> enabledList;
};

#endif // MENUDATA_H
