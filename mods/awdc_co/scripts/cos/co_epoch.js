CO_EPOCH.init = function(co)
{
    co.setPowerStars(3);
    co.setSuperpowerStars(3);
};
CO_EPOCH.activateSuperpower = function(co, powerMode)
{
	CO_EPOCH.activatePower(co, powerMode);
};
CO_EPOCH.getSuperPowerDescription = function()
{
    return CO_EPOCH.getPowerDescription();
};
CO_EPOCH.getSuperPowerName = function()
{
    return CO_EPOCH.getPowerName();
};
CO_EPOCH.getBonusLuck = function(co, unit, posX, posY)
{
    switch (co.getPowerMode())
    {
        case GameEnums.PowerMode_Tagpower:
        case GameEnums.PowerMode_Superpower:
        case GameEnums.PowerMode_Power:
            return 10;
        default:
            if (co.inCORange(Qt.point(posX, posY), unit))
            {
                return 10;
            }
            break;
    }
    return 0;
};
CO_EPOCH.getOffensiveBonus = function(co, attacker, atkPosX, atkPosY,
                             defender, defPosX, defPosY, isDefender, action)
{
    switch (co.getPowerMode())
    {
        case GameEnums.PowerMode_Tagpower:
        case GameEnums.PowerMode_Superpower:
        case GameEnums.PowerMode_Power:
            return 20;
        default:
            if (co.inCORange(Qt.point(atkPosX, atkPosY), attacker))
            {
                return 10;
            }
            break;
    }
    return 0;
};
CO_EPOCH.getDeffensiveBonus = function(co, attacker, atkPosX, atkPosY,
                             defender, defPosX, defPosY, isDefender, action)
{
    switch (co.getPowerMode())
    {
        case GameEnums.PowerMode_Tagpower:
        case GameEnums.PowerMode_Superpower:
        case GameEnums.PowerMode_Power:
            return 20;
        default:
            if (co.inCORange(Qt.point(defPosX, defPosY), defender))
            {
                return 10;
            }
            break;
    }
    return 0;
};
CO_EPOCH.getFirerangeModifier = function(co, unit, posX, posY)
{
    return 0;
};
CO_EPOCH.getMovementpointModifier = function(co, unit, posX, posY)
{
    return 0;
};
