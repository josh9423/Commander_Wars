#ifndef SCRIPTEVENTCHANGEUNITOWNER_H
#define SCRIPTEVENTCHANGEUNITOWNER_H

#include "ingamescriptsupport/events/scripteventgeneric.h"

class ScriptEventChangeUnitOwner : public ScriptEventGeneric
{
public:
    ScriptEventChangeUnitOwner();
    /**
     * @brief removeCustomStart
     * @param text
     */
    virtual void removeCustomStart(QString& text) override;
    /**
     * @brief writeCustomStart
     */
    virtual void writeCustomStart(QTextStream& stream) override;
};

#endif // SCRIPTEVENTCHANGEUNITOWNER_H
